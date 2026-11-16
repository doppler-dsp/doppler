/**
 * @file fir_core.c
 * @brief Direct-form FIR filter — real-tap and complex-tap variants.
 *
 * Scratch layout: [delay_line (M-1) | input (N)]
 * After filtering: new delay = scratch[N .. N+M-2].
 *
 * Hot loop — real taps, CF32 signal (JM_VEC_F32 width-portable):
 *   Broadcast h[k] to JM_SIMD_WIDTH_F32 float lanes; FMA directly.
 *   FIR_RGROUP (4) vectors of outputs per pass, each its own running sum.
 *   Cost: 1 FMA per tap per vector, 4 independent sums in flight.
 *
 * Hot loop — complex taps, CF32 signal (AVX-512):
 *   For each tap h[k] = h_r + j*h_i and 8 parallel output lanes:
 *     vx      = loadu_ps(&scratch[i + M-1 - k])
 *     vx_swap = permute_ps(vx, 0xB1)   [swap real/imag in each pair]
 *     acc    += fmadd(h_r, vx, acc)
 *     acc    += fmadd(h_i, vx_swap*SIGN, acc)  [SIGN negates real parts]
 *   Cost: 2 FMA + 1 permute + 1 mul per tap, 8 outputs/iteration.
 *
 * Hot loop — complex taps, AVX2 / SSE2 / NEON (JM_VEC_F32, 8 or 4 lanes):
 *   The same identity with the sign applied once per output, not per tap:
 *   a += h_r * x, b += h_i * swap(x), y = a + [-1 +1] * b. FIR_RGROUP (4)
 *   vectors of outputs per pass. Measured against the scalar loop below,
 *   AVX2+FMA: 0.20x at 15 taps, 0.29x at 63, 0.31x at 255.
 *   Falls back to scalar on every other target.
 */

#include "doppler/fir/fir_core.h"

#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

dp_fir_state_t *
dp_fir_create (const float _Complex *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  dp_fir_state_t *f = (dp_fir_state_t *)calloc (1, sizeof (*f));
  if (!f)
    return NULL;

  f->taps = (float _Complex *)dp_mallocn (num_taps, sizeof (float _Complex));
  if (!f->taps)
    {
      free (f);
      return NULL;
    }
  memcpy (f->taps, taps, num_taps * sizeof (float _Complex));

  if (num_taps > 1)
    {
      f->delay
          = (float _Complex *)calloc (num_taps - 1, sizeof (float _Complex));
      if (!f->delay)
        {
          free (f->taps);
          free (f);
          return NULL;
        }
    }

  f->num_taps = num_taps;
  return f;
}

dp_fir_state_t *
dp_fir_create_real (const float *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  dp_fir_state_t *f = (dp_fir_state_t *)calloc (1, sizeof (*f));
  if (!f)
    return NULL;

  f->rtaps = (float *)dp_mallocn (num_taps, sizeof (float));
  if (!f->rtaps)
    {
      free (f);
      return NULL;
    }
  memcpy (f->rtaps, taps, num_taps * sizeof (float));

  if (num_taps > 1)
    {
      f->delay
          = (float _Complex *)calloc (num_taps - 1, sizeof (float _Complex));
      if (!f->delay)
        {
          free (f->rtaps);
          free (f);
          return NULL;
        }
    }

  f->num_taps = num_taps;
  return f;
}

void
dp_fir_destroy (dp_fir_state_t *state)
{
  if (!state)
    return;
  free (state->taps);
  free (state->rtaps);
  free (state->delay);
  free (state->scratch);
  free (state);
}

void
dp_fir_reset (dp_fir_state_t *state)
{
  if (state->delay && state->num_taps > 1)
    memset (state->delay, 0, (state->num_taps - 1) * sizeof (float _Complex));
}

/* ── Serializable state — standard envelope + delay line (num_taps-1
 * samples); see dp_state.h. ─────────────────────────────────────────────── */

size_t
dp_fir_state_bytes (const dp_fir_state_t *state)
{
  size_t payload = (state->num_taps > 1)
                       ? (state->num_taps - 1) * sizeof (float _Complex)
                       : 0;
  return sizeof (dp_state_hdr_t) + payload;
}

void
dp_fir_get_state (const dp_fir_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, dp_fir_state_bytes (state));
  dp_w_hdr (&w, FIR_STATE_MAGIC, FIR_STATE_VERSION,
            dp_fir_state_bytes (state));
  if (state->num_taps > 1)
    dp_w_cf32 (&w, state->delay, state->num_taps - 1);
}

int
dp_fir_set_state (dp_fir_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, dp_fir_state_bytes (state),
                              FIR_STATE_MAGIC, FIR_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, dp_fir_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  if (state->num_taps > 1)
    dp_r_cf32 (&r, state->delay, state->num_taps - 1);
  return DP_OK;
}

size_t
dp_fir_get_num_taps (const dp_fir_state_t *state)
{
  return state->num_taps;
}

double
dp_fir_dc_gain (const dp_fir_state_t *state)
{
  double sum = 0.0;
  for (size_t i = 0; i < state->num_taps; i++)
    sum += state->rtaps ? (double)state->rtaps[i]
                        : (double)crealf (state->taps[i]);
  return sum;
}

int
dp_fir_get_is_real (const dp_fir_state_t *state)
{
  return state->rtaps != NULL;
}

/* FIR is 1:1; max_out == n_in which is unknown at create time. */
size_t
dp_fir_execute_max_out (dp_fir_state_t *state)
{
  (void)state;
  return 0; /* execute() emits one sample per input sample */
}

/* ── Scratch management ─────────────────────────────────────────────────── */

/*
 * Zeroed complex samples after the input in scratch. The last, partial group
 * of outputs runs through the SAME vector body as every full group (see
 * inner_real_cf32), and that body reads a whole group of inputs: up to
 * FIR_PAD - 1 past the end. The pad is the widest group any kernel runs:
 * 8 complex at least (the AVX-512 complex-tap group), and the real-tap and
 * AVX2 / SSE2 / NEON complex-tap group of FIR_RGROUP vectors where it is
 * larger (16 on AVX2, 32 on AVX-512).
 */
#define FIR_PAD FIR_PAD_
#if JM_SIMD_WIDTH_F32 >= 2
/* inner_real_cf32's group: FIR_RGROUP vectors, one sum each. */
#define FIR_RGROUP 4
#define FIR_RGROUP_C (FIR_RGROUP * (JM_SIMD_WIDTH_F32 / 2))
#define FIR_PAD_ (FIR_RGROUP_C > 8 ? FIR_RGROUP_C : 8)
#else
#define FIR_PAD_ 8
#endif

static int
ensure_scratch (dp_fir_state_t *f, size_t num_samples)
{
  /* A block too long to size is refused like an allocation failure. */
  size_t needed;
  if (!dp_size_add (f->num_taps - 1, num_samples, &needed)
      || !dp_size_add (needed, FIR_PAD, &needed))
    return -1;
  if (needed <= f->scratch_cap)
    return 0;
  float _Complex *tmp = (float _Complex *)dp_reallocn (
      f->scratch, needed, sizeof (float _Complex));
  if (!tmp)
    return -1;
  f->scratch     = tmp;
  f->scratch_cap = needed;
  return 0;
}

/* ── Complex-tap inner loop — AVX-512 (permute + sign mask) ─────────────── */

#if defined(__AVX512F__) && defined(__AVX512DQ__)
#include <immintrin.h>

static const float fir_sign[16] __attribute__ ((aligned (64)))
= { -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1 };

/* Eight complex outputs from buf[i .. i+8+M-2]. One body, used for every
 * group including the last (see inner_cf32): an output's rounding is then a
 * property of the output, not of where the caller split the stream. */
static inline __m512
cf32_group (const float _Complex *JM_RESTRICT buf,
            const float _Complex *JM_RESTRICT h, size_t M, size_t i)
{
  const __m512 SIGN = _mm512_load_ps (fir_sign);
  __m512       acc  = _mm512_setzero_ps ();
  for (size_t k = 0; k < M; k++)
    {
      __m512 vx  = _mm512_loadu_ps ((const float *)&buf[i + M - 1 - k]);
      __m512 vxs = _mm512_permute_ps (vx, 0xB1);
      __m512 vhr = _mm512_set1_ps (crealf (h[k]));
      __m512 vhi = _mm512_set1_ps (cimagf (h[k]));
      acc        = _mm512_fmadd_ps (vhr, vx, acc);
      acc        = _mm512_fmadd_ps (vhi, _mm512_mul_ps (vxs, SIGN), acc);
    }
  return acc;
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_cf32 (const float _Complex *JM_RESTRICT buf,
            const float _Complex *JM_RESTRICT h, size_t M,
            float _Complex *JM_RESTRICT out, size_t N)
{
  size_t i = 0;
  for (; i + 8 <= N; i += 8)
    _mm512_storeu_ps ((float *)&out[i], cf32_group (buf, h, M, i));
  if (i < N)
    {
      /* A scalar tail would round differently from the fused body above
       * (a separate multiply and add, or, under -ffast-math, a reassociated
       * sum over the taps), so the last output of a call would differ from
       * the same output inside a longer call (#1893). Run the body once more
       * over the zero pad and keep the lanes that exist. */
      float _Complex tmp[8];
      _mm512_storeu_ps ((float *)tmp, cf32_group (buf, h, M, i));
      memcpy (&out[i], tmp, (N - i) * sizeof (float _Complex));
    }
}

#elif JM_SIMD_WIDTH_F32 == 8 || JM_SIMD_WIDTH_F32 == 4

/* Swap I and Q in every complex pair of a vector: [a b c d] -> [b a d c].
 * jm_simd.h has no lane permute, so it is spelled per instruction set. */
#if JM_SIMD_WIDTH_F32 == 8
#define FIR_SWAP_IQ(v) _mm256_permute_ps (v, 0xB1)
#elif defined(__aarch64__)
#define FIR_SWAP_IQ(v) vrev64q_f32 (v)
#else
#define FIR_SWAP_IQ(v) _mm_shuffle_ps (v, v, 0xB1)
#endif

/* FIR_RGROUP vectors of outputs from buf[i ..]. For x = [xr xi] and tap
 * h = hr + j hi, two running sums per vector: a += hr * x = [hr xr, hr xi]
 * and b += hi * swap(x) = [hi xi, hi xr]. Then y = a + [-1 +1] * b
 * = [hr xr - hi xi, hr xi + hi xr], the sign applied once, not per tap.
 * Every output, the last partial group's included, comes out of this one
 * body (#1893), and the groups keep 2 * FIR_RGROUP sums in flight. */
static inline void
cf32_vgroup (const float _Complex *JM_RESTRICT buf,
             const float _Complex *JM_RESTRICT h, size_t M, size_t i,
             float _Complex *JM_RESTRICT dst)
{
  static const float sign[8]
      = { -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f };
  const size_t w = JM_SIMD_WIDTH_F32 / 2;
  JM_VEC_F32   a[FIR_RGROUP], b[FIR_RGROUP];
  for (int s = 0; s < FIR_RGROUP; s++)
    a[s] = b[s] = JM_ZERO_F32 ();
  for (size_t k = 0; k < M; k++)
    {
      const JM_VEC_F32 hr = JM_SPLAT_F32 (crealf (h[k]));
      const JM_VEC_F32 hi = JM_SPLAT_F32 (cimagf (h[k]));
      for (int s = 0; s < FIR_RGROUP; s++)
        {
          const JM_VEC_F32 x
              = JM_LOAD_F32 ((const float *)&buf[i + s * w + M - 1 - k]);
          JM_FMA_F32 (a[s], x, hr);
          JM_FMA_F32 (b[s], FIR_SWAP_IQ (x), hi);
        }
    }
  const JM_VEC_F32 sg = JM_LOAD_F32 (sign);
  for (int s = 0; s < FIR_RGROUP; s++)
    JM_STORE_F32 ((float *)&dst[s * w],
                  JM_ADD_F32 (a[s], JM_MUL_F32 (b[s], sg)));
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_cf32 (const float _Complex *JM_RESTRICT buf,
            const float _Complex *JM_RESTRICT h, size_t M,
            float _Complex *JM_RESTRICT out, size_t N)
{
  size_t i = 0;
  for (; i + FIR_RGROUP_C <= N; i += FIR_RGROUP_C)
    cf32_vgroup (buf, h, M, i, &out[i]);
  if (i < N)
    {
      /* The same body over the zero pad (FIR_PAD >= one group). */
      float _Complex tmp[FIR_RGROUP_C];
      cf32_vgroup (buf, h, M, i, tmp);
      memcpy (&out[i], tmp, (N - i) * sizeof (float _Complex));
    }
}

#else /* scalar fallback for complex taps */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_cf32 (const float _Complex *JM_RESTRICT buf,
            const float _Complex *JM_RESTRICT h, size_t M,
            float _Complex *JM_RESTRICT out, size_t N)
{
  const ptrdiff_t iM = (ptrdiff_t)M;
  for (ptrdiff_t ii = 0; (size_t)ii < N; ii++)
    {
      float     re = 0.0f, im = 0.0f;
      ptrdiff_t base = ii + iM - 1;
      for (ptrdiff_t k = 0; k < iM; k++)
        {
          float xr = crealf (buf[base - k]);
          float xi = cimagf (buf[base - k]);
          re += crealf (h[k]) * xr - cimagf (h[k]) * xi;
          im += crealf (h[k]) * xi + cimagf (h[k]) * xr;
        }
      out[ii] = CMPLXF (re, im);
    }
}

#endif /* AVX-512 complex path */

/* ── Real-tap inner loop — width-portable via JM_VEC_F32 ────────────────── */
/*
 * Each JM_VEC_F32 holds JM_SIMD_WIDTH_F32 floats = JM_SIMD_WIDTH_F32/2
 * complex samples.  JM_MAC_F32 broadcasts the real scalar tap h[k] across
 * all lanes and FMAs with the interleaved I/Q data — no permute needed.
 *
 * Width  | ISA       | complex outputs per group (FIR_RGROUP = 4 vectors)
 * -------|-----------|---------------------------------------------------
 *    16  | AVX-512   | 32
 *     8  | AVX2+FMA  | 16
 *     4  | SSE2      | 8 (from jm_simd.h's SSE2 tier, just-makeit#2651;
 *        |           |    a multiply then an add: SSE has no FMA)
 *     1  | scalar    | not compiled; the auto-vectorised loop below runs
 *
 * One running sum per vector is a dependent chain M taps long; four let the
 * adds overlap. Measured on the 255-tap row: 0.34x of one sum under
 * AVX2+FMA, 0.38x under that SSE2 tier.
 */

#if JM_SIMD_WIDTH_F32 >= 2

/* FIR_RGROUP vectors of outputs, each its own running sum over the taps in
 * order. One sum alone is a dependent add chain M long (a multiply then an
 * add per tap where there is no FMA); independent sums let the adds overlap.
 * Every output, the last partial group's included, comes out of this one
 * body, so an output's bits do not depend on where the caller split the
 * stream (#1893, #2805). */
static inline void
real_group (const float _Complex *JM_RESTRICT buf, const float *JM_RESTRICT h,
            size_t M, size_t i, float _Complex *JM_RESTRICT dst)
{
  const size_t w = JM_SIMD_WIDTH_F32 / 2;
  JM_VEC_F32   acc[FIR_RGROUP];
  for (int s = 0; s < FIR_RGROUP; s++)
    acc[s] = JM_ZERO_F32 ();
  for (size_t k = 0; k < M; k++)
    for (int s = 0; s < FIR_RGROUP; s++)
      JM_MAC_F32 (acc[s], (const float *)&buf[i + s * w + M - 1 - k], h[k]);
  for (int s = 0; s < FIR_RGROUP; s++)
    JM_STORE_F32 ((float *)&dst[s * w], acc[s]);
}

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_real_cf32 (const float _Complex *JM_RESTRICT buf,
                 const float *JM_RESTRICT h, size_t M,
                 float _Complex *JM_RESTRICT out, size_t N)
{
  size_t i = 0;
  for (; i + FIR_RGROUP_C <= N; i += FIR_RGROUP_C)
    real_group (buf, h, M, i, &out[i]);
  if (i < N)
    {
      /* The same body over the zero pad (FIR_PAD >= one group), keeping
       * the outputs that exist. */
      float _Complex tmp[FIR_RGROUP_C];
      real_group (buf, h, M, i, tmp);
      memcpy (&out[i], tmp, (N - i) * sizeof (float _Complex));
    }
}

#else /* scalar — auto-vectorised */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_real_cf32 (const float _Complex *JM_RESTRICT buf,
                 const float *JM_RESTRICT h, size_t M,
                 float _Complex *JM_RESTRICT out, size_t N)
{
  const ptrdiff_t iM = (ptrdiff_t)M;
  for (ptrdiff_t ii = 0; (size_t)ii < N; ii++)
    {
      float     re = 0.0f, im = 0.0f;
      ptrdiff_t base = ii + iM - 1;
      for (ptrdiff_t k = 0; k < iM; k++)
        {
          re += h[k] * crealf (buf[base - k]);
          im += h[k] * cimagf (buf[base - k]);
        }
      out[ii] = CMPLXF (re, im);
    }
}

#endif /* JM_SIMD_WIDTH_F32 */

/* ── Execute ────────────────────────────────────────────────────────────── */

size_t
dp_fir_execute (dp_fir_state_t *state, const float _Complex *in, size_t n_in,
                float _Complex *out)
{
  if (!state || !in || !out || n_in == 0)
    return 0;
  if (ensure_scratch (state, n_in) < 0)
    return 0;

  const size_t dly = state->num_taps - 1;
  if (dly)
    memcpy (state->scratch, state->delay, dly * sizeof (float _Complex));
  memcpy (state->scratch + dly, in, n_in * sizeof (float _Complex));
  memset (state->scratch + dly + n_in, 0, FIR_PAD * sizeof (float _Complex));

  if (state->rtaps)
    inner_real_cf32 (state->scratch, state->rtaps, state->num_taps, out, n_in);
  else
    inner_cf32 (state->scratch, state->taps, state->num_taps, out, n_in);

  if (dly)
    memcpy (state->delay, state->scratch + n_in,
            dly * sizeof (float _Complex));

  return n_in;
}
