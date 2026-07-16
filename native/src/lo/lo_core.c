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

/* ------------------------------------------------------------------
 * Normalised frequency → uint32 phase increment.
 *
 * Uses double arithmetic to avoid rounding at the float→uint32 boundary.
 * floor() folds negative frequencies correctly: −0.25 → 0.75 → 3×2^30.
 *
 * Rounds to the nearest 32-bit increment (llround) rather than truncating:
 * a 32-bit phase word can only ever represent frequency in fs/2^32 steps
 * (a one-time, unavoidable quantization — no fixed-width accumulator can
 * be exact except at those specific levels), but truncating always rounds
 * toward zero, giving a *systematic* one-sided bias up to a full step;
 * rounding halves the worst case and centers the residual at zero.  This
 * has no bearing on tracking-loop performance downstream — a carrier loop
 * exists precisely to null out a small, constant residual like this one,
 * regardless of its source or size.  `d` is always in [0, 1) here, so
 * `llround`'s result is always in [0, 2^32]; the one edge case (d rounds
 * up to exactly 1.0 cycle, i.e. 2^32) wraps to phase-increment 0 via the
 * well-defined long long → uint32_t conversion — correct, since a full
 * extra cycle per sample is indistinguishable from no rotation at all,
 * the same aliasing identity the negative-frequency folding above uses.
 * ------------------------------------------------------------------ */
static uint32_t
norm_to_inc (double norm_freq)
{
  double d = norm_freq - floor (norm_freq);
  return (uint32_t)llround (d * 4294967296.0);
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

void
lo_init (lo_state_t *state, double norm_freq)
{
  lut_init ();
  state->phase     = 0;
  state->phase_inc = norm_to_inc (norm_freq);
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
  state->phase_inc = norm_to_inc (norm_freq);
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
lo_steps (lo_state_t *state, size_t n, float complex *out)
{
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
lo_steps (lo_state_t *state, size_t n, float complex *out)
{
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

#ifdef __AVX512F__

/*
 * AVX-512 path for lo_steps_ctrl.
 *
 * Each sample's phase depends on the previous sample's ctrl value:
 *   ph[i] = ph[i-1] + inc + ctrl_inc[i-1]
 * This is a running sum.  Solved with a 4-step log-2 exclusive prefix
 * scan over the 16 per-sample deltas, then a single vector add of the
 * base phase.  LUT gather and IQ interleave are identical to lo_steps.
 *
 * Precision: ctrl_inc uses float32 (multiply by 2^32).  The ULP at
 * 2^32 in float32 is 2^8 = 256, so ctrl_inc is accurate to ±128 in
 * its lowest 8 bits — below one LUT bin (2^16 counts).  No effect on
 * the top 16 bits that drive the LUT.
 */
size_t
lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
               float complex *out)
{
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;

  static const int32_t perm0_arr[16]
      = { 0, 1, 2, 3, 16, 17, 18, 19, 4, 5, 6, 7, 20, 21, 22, 23 };
  static const int32_t perm1_arr[16]
      = { 8, 9, 10, 11, 24, 25, 26, 27, 12, 13, 14, 15, 28, 29, 30, 31 };
  __m512i vperm0 = _mm512_loadu_si512 (perm0_arr);
  __m512i vperm1 = _mm512_loadu_si512 (perm1_arr);

  __m512i vinc  = _mm512_set1_epi32 ((int32_t)inc);
  __m512i vzero = _mm512_setzero_si512 ();
  __m512i vmask = _mm512_set1_epi32 (0xFFFF);
  __m512i vqtr  = _mm512_set1_epi32 (LO_LUT_QTR);
  __m512  v2p32 = _mm512_set1_ps (4294967296.0f);

  size_t i = 0;
  for (; i + 16 <= ctrl_len; i += 16)
    {
      /* Load 16 ctrl floats; use fractional part = ctrl - floor(ctrl). */
      __m512 vc    = _mm512_loadu_ps (ctrl + i);
      __m512 vfrac = _mm512_sub_ps (vc, _mm512_floor_ps (vc));

      /* ctrl_inc[k] = (uint32_t)(frac[k] * 2^32) */
      __m512i vci = _mm512_cvttps_epu32 (_mm512_mul_ps (vfrac, v2p32));

      /* delta[k] = inc + ctrl_inc[k] */
      __m512i vd = _mm512_add_epi32 (vinc, vci);

      /* Inclusive prefix scan (Hillis-Steele, 4 passes):
       * after pass p, vs[k] = sum(delta[max(0, k-2^p+1)..k])    */
      __m512i vs = vd;
      vs         = _mm512_add_epi32 (vs, _mm512_alignr_epi32 (vs, vzero, 15));
      vs         = _mm512_add_epi32 (vs, _mm512_alignr_epi32 (vs, vzero, 14));
      vs         = _mm512_add_epi32 (vs, _mm512_alignr_epi32 (vs, vzero, 12));
      vs         = _mm512_add_epi32 (vs, _mm512_alignr_epi32 (vs, vzero, 8));

      /* Exclusive scan: shift right 1 element, insert 0 at lane 0.
       * vpsum[k] = sum(delta[0..k-1])  (== 0 for k=0)           */
      __m512i vpsum = _mm512_alignr_epi32 (vs, vzero, 15);

      /* Phase for each sample: ph_base + vpsum[k] */
      __m512i vph = _mm512_add_epi32 (_mm512_set1_epi32 ((int32_t)ph), vpsum);

      __m512i vsin_idx = _mm512_srli_epi32 (vph, 16);
      __m512i vcos_idx
          = _mm512_and_epi32 (_mm512_add_epi32 (vsin_idx, vqtr), vmask);

      __m512 vsin = _mm512_i32gather_ps (vsin_idx, lo_sin_lut, 4);
      __m512 vcos = _mm512_i32gather_ps (vcos_idx, lo_sin_lut, 4);

      __m512 lo_v = _mm512_unpacklo_ps (vcos, vsin);
      __m512 hi_v = _mm512_unpackhi_ps (vcos, vsin);
      _mm512_storeu_ps ((float *)(out + i),
                        _mm512_permutex2var_ps (lo_v, vperm0, hi_v));
      _mm512_storeu_ps ((float *)(out + i + 8),
                        _mm512_permutex2var_ps (lo_v, vperm1, hi_v));

      /* Advance base phase by the total of all 16 deltas. */
      ph += (uint32_t)_mm512_reduce_add_epi32 (vd);
    }

  /* Scalar tail — double precision for the ctrl_inc conversion. */
  for (; i < ctrl_len; i++)
    {
      double d = (double)ctrl[i];
      d -= floor (d);
      uint32_t ctrl_inc = (uint32_t)(d * 4294967296.0);
      uint16_t idx      = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc + ctrl_inc;
    }

  state->phase = ph;
  return ctrl_len;
}

#else /* scalar fallback */

size_t
lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
               float complex *out)
{
  uint32_t ph  = state->phase;
  uint32_t inc = state->phase_inc;
  for (size_t i = 0; i < ctrl_len; i++)
    {
      double d = (double)ctrl[i];
      d -= floor (d);
      uint32_t ctrl_inc = (uint32_t)(d * 4294967296.0);
      uint16_t idx      = (uint16_t)(ph >> (32u - LO_LUT_BITS));
      out[i] = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                       lo_sin_lut[idx]);
      ph += inc + ctrl_inc;
    }
  state->phase = ph;
  return ctrl_len;
}

#endif /* __AVX512F__ */
