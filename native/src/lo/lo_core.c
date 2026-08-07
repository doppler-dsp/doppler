/**
 * @file lo_core.c
 * @brief Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors.
 *
 * Re-implemented from the doppler reference (native/src/lo/lo_core.c).
 * Algorithm and AVX-512 paths are identical; the state struct and API
 * use doppler conventions (double norm_freq, jm property names).
 */
#include "lo/lo_core.h"

#include <math.h>
#include <string.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

/* ------------------------------------------------------------------
 * Sine LUT — 2^16 single-precision entries.
 *
 * LUT index is the top 16 bits of the 32-bit phase accumulator:
 *   idx    = (uint16_t)(phase >> 16)
 *   sin(θ) = lo_sin_lut[idx]
 *   cos(θ) = lo_sin_lut[(uint16_t)(idx + LO_LUT_QTR)]
 *
 * LO_LUT_QTR = N/4 = 16384 shifts by π/2, mapping sin → cos without
 * extra storage.  The uint16_t cast wraps at 65536 branchlessly.
 *
 * 65536 floats × 4 bytes = 256 KB — fits in L2 on all modern CPUs.
 * SFDR: ~96 dBc from 16-bit phase truncation.
 * ------------------------------------------------------------------ */
/* LO_LUT_BITS / LO_LUT_SIZE / LO_LUT_QTR are defined in lo_core.h so the
 * inline lo_step() in the header can share them. */

/* Definition of the LUT declared `extern` in lo_core.h.  Filled lazily by
 * lut_init() on the first lo_create()/lo_init(); read-only afterwards. */
float      lo_sin_lut[LO_LUT_SIZE];
static int lut_ready = 0;

static void
lut_init (void)
{
  if (lut_ready)
    return;
  for (unsigned i = 0; i < LO_LUT_SIZE; i++)
    lo_sin_lut[i] = sinf (2.0f * (float)M_PI * (float)i / (float)LO_LUT_SIZE);
  lut_ready = 1;
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

void
lo_init (lo_state_t *state, double norm_freq)
{
  lut_init ();
  state->phase     = 0;
  state->phase_inc = nco_norm_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

lo_state_t *
lo_create (double norm_freq)
{
  lo_state_t *state = malloc (sizeof (*state));
  if (!state)
    return NULL;
  lo_init (state, norm_freq);
  return state;
}

void
lo_destroy (lo_state_t *state)
{
  free (state);
}

void
lo_reset (lo_state_t *state)
{
  state->phase = 0;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

double
lo_get_norm_freq (const lo_state_t *state)
{
  return state->norm_freq;
}

void
lo_set_norm_freq (lo_state_t *state, double norm_freq)
{
  state->phase_inc = nco_norm_to_inc (norm_freq);
  state->norm_freq = norm_freq;
}

uint32_t
lo_get_phase (const lo_state_t *state)
{
  return state->phase;
}

void
lo_set_phase (lo_state_t *state, uint32_t phase)
{
  state->phase = phase;
}

/* ── Serializable state — standard envelope + phase (the only per-sample
 * state); see dp_state.h. ───────────────────────────────────────────────── */

size_t
lo_state_bytes (const lo_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (uint32_t);
}

void
lo_get_state (const lo_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, lo_state_bytes (state));
  dp_w_hdr (&w, LO_STATE_MAGIC, LO_STATE_VERSION, lo_state_bytes (state));
  dp_w_u32 (&w, state->phase);
}

int
lo_set_state (lo_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, lo_state_bytes (state), LO_STATE_MAGIC,
                              LO_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, lo_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  state->phase  = dp_r_u32 (&r);
  return DP_OK;
}

uint32_t
lo_get_phase_inc (const lo_state_t *state)
{
  return state->phase_inc;
}

/* ================================================================== */
/* Block generators                                                    */
/* ================================================================== */

/*
 * Pre-allocated buffer size for all generator methods.  The Python
 * extension allocates output buffers of this size at create time; calling
 * with n > 65536 overflows the buffer and is undefined behaviour.
 */
#define LO_MAX_OUT 65536u

size_t
lo_steps_max_out (lo_state_t *state)
{
  (void)state;
  return LO_MAX_OUT;
}

size_t
lo_steps_ctrl_max_out (lo_state_t *state)
{
  (void)state;
  return LO_MAX_OUT;
}

/* ================================================================== */
/* Execute — free-running                                              */
/* ================================================================== */

#ifdef __AVX512F__

size_t
lo_steps (lo_state_t *state, size_t n, float complex *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;

  /*
   * Permutation vectors interleave unpacklo/hi 128-bit lanes into
   * contiguous IQ pairs.  See the doppler reference lo_core.c for
   * the full derivation.
   */
  static const int32_t perm0_arr[16]
      = { 0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23 };
  static const int32_t perm1_arr[16]
      = { 8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31 };
  __m512i vperm0 = _mm512_loadu_si512 (perm0_arr);
  __m512i vperm1 = _mm512_loadu_si512 (perm1_arr);

  __m512i vsamp = _mm512_setr_epi32 (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                     13, 14, 15);
  __m512i vinc  = _mm512_set1_epi32 ((int32_t)inc);

  size_t i = 0;
  for (; i + 16 <= n; i += 16, ph += 16u * inc)
    {
      __m512i vph = _mm512_add_epi32 (_mm512_set1_epi32 ((int32_t)ph),
                                      _mm512_mullo_epi32 (vsamp, vinc));

      __m512i vsin_idx = _mm512_srli_epi32 (vph, 16);
      __m512i vcos_idx = _mm512_and_epi32 (
          _mm512_add_epi32 (vsin_idx, _mm512_set1_epi32 (LO_LUT_QTR)),
          _mm512_set1_epi32 (0xFFFF));

      __m512 vsin = _mm512_i32gather_ps (vsin_idx, lo_sin_lut, 4);
      __m512 vcos = _mm512_i32gather_ps (vcos_idx, lo_sin_lut, 4);

      __m512 lo_v = _mm512_unpacklo_ps (vcos, vsin);
      __m512 hi_v = _mm512_unpackhi_ps (vcos, vsin);

      _mm512_storeu_ps ((float *)(out + i),
                        _mm512_permutex2var_ps (lo_v, vperm0, hi_v));
      _mm512_storeu_ps ((float *)(out + i + 8),
                        _mm512_permutex2var_ps (lo_v, vperm1, hi_v));
    }

  /* Scalar tail */
  for (; i < n; i++)
    {
      uint16_t idx = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc;
    }

  state->phase = ph;
  return n;
}

#else /* scalar fallback */

size_t
lo_steps (lo_state_t *state, size_t n, float complex *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (n > max_out)
    n = max_out;
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;
  for (size_t i = 0; i < n; i++)
    {
      uint16_t idx = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc;
    }
  state->phase = ph;
  return n;
}

#endif /* __AVX512F__ */

/* ================================================================== */
/* Execute — with per-sample FM control port                           */
/* ================================================================== */

/*
 * One implementation, deliberately.
 *
 * This function carried a hand-written AVX-512 body beside the scalar
 * loop: a 4-step log-2 prefix scan over 16 per-sample deltas, then a LUT
 * gather. It was deleted rather than kept, for two independent reasons.
 *
 * It was never faster. Measured over 65536 samples, lo_steps_ctrl runs at
 * 1015 Msamp/s as this scalar loop at the shipped -march=x86-64-v2
 * baseline, against 767 for the AVX-512 body -- 1.3x SLOWER hand-written
 * than compiled. That is the effect the -march policy in CMakeLists.txt
 * already documents ("forcing 512-bit vectors measured slower than SSE4.2
 * on complex-float DSP kernels") and caps with -mprefer-vector-width=256,
 * which constrains auto-vectorization but cannot constrain an explicit
 * _mm512_* intrinsic. The block was the one thing that policy exists to
 * prevent, exempted by being hand-written.
 *
 * And it had drifted. It derived ctrl_inc from a float32 fold via
 * _mm512_cvtps_epu32 while this tail called the shared double-precision
 * nco_norm_to_inc(), so the two halves of ONE function disagreed -- by
 * 3673 phase units and 175 differing outputs over 4096 samples, because
 * ctrl_inc feeds a prefix sum and a per-sample error integrates. A second
 * implementation of a kernel is a second thing to keep correct, and this
 * one bought nothing to pay for that with.
 *
 * The block-equals-one-sample-at-a-time test in test_lo_core.c is what
 * caught the drift; it now guards against a replacement reintroducing it.
 */

size_t
lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
               float complex *out, size_t max_out)
{
  /* Emission stops at the caller's capacity (jm gh-138). */
  if (ctrl_len > max_out)
    ctrl_len = max_out;
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;
  for (size_t i = 0; i < ctrl_len; i++)
    {
      uint32_t ctrl_inc = nco_norm_to_inc ((double)ctrl[i]);
      uint16_t idx      = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc + ctrl_inc;
    }
  state->phase = ph;
  return ctrl_len;
}
