/**
 * @file fir_core.c
 * @brief Direct-form FIR filter — real-tap and complex-tap variants.
 *
 * Scratch layout: [delay_line (M-1) | input (N)]
 * After filtering: new delay = scratch[N .. N+M-2].
 *
 * Hot loop — real taps, CF32 signal (JM_VEC_F32 width-portable):
 *   Broadcast h[k] to JM_SIMD_WIDTH_F32 float lanes; FMA directly.
 *   Stride = JM_SIMD_WIDTH_F32/2 complex outputs per iteration.
 *   Cost: 1 FMA per tap, stride outputs/iteration.
 *
 * Hot loop — complex taps, CF32 signal (AVX-512 only):
 *   For each tap h[k] = h_r + j*h_i and 8 parallel output lanes:
 *     vx      = loadu_ps(&scratch[i + M-1 - k])
 *     vx_swap = permute_ps(vx, 0xB1)   [swap real/imag in each pair]
 *     acc    += fmadd(h_r, vx, acc)
 *     acc    += fmadd(h_i, vx_swap*SIGN, acc)  [SIGN negates real parts]
 *   Cost: 2 FMA + 1 permute + 1 mul per tap, 8 outputs/iteration.
 *   Falls back to scalar on non-AVX-512 targets.
 */

#include "fir/fir_core.h"

#include <stdlib.h>
#include <string.h>

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

fir_state_t *
fir_create (const float complex *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  fir_state_t *f = (fir_state_t *)calloc (1, sizeof (*f));
  if (!f)
    return NULL;

  f->taps = (float complex *)malloc (num_taps * sizeof (float complex));
  if (!f->taps)
    {
      free (f);
      return NULL;
    }
  memcpy (f->taps, taps, num_taps * sizeof (float complex));

  if (num_taps > 1)
    {
      f->delay
          = (float complex *)calloc (num_taps - 1, sizeof (float complex));
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

fir_state_t *
fir_create_real (const float *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  fir_state_t *f = (fir_state_t *)calloc (1, sizeof (*f));
  if (!f)
    return NULL;

  f->rtaps = (float *)malloc (num_taps * sizeof (float));
  if (!f->rtaps)
    {
      free (f);
      return NULL;
    }
  memcpy (f->rtaps, taps, num_taps * sizeof (float));

  if (num_taps > 1)
    {
      f->delay
          = (float complex *)calloc (num_taps - 1, sizeof (float complex));
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
fir_destroy (fir_state_t *state)
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
fir_reset (fir_state_t *state)
{
  if (state->delay && state->num_taps > 1)
    memset (state->delay, 0, (state->num_taps - 1) * sizeof (float complex));
}

size_t
fir_get_num_taps (const fir_state_t *state)
{
  return state->num_taps;
}

int
fir_get_is_real (const fir_state_t *state)
{
  return state->rtaps != NULL;
}

/* FIR is 1:1; max_out == n_in which is unknown at create time. */
size_t
fir_execute_max_out (fir_state_t *state)
{
  (void)state;
  return 0;
}

/* ── Scratch management ─────────────────────────────────────────────────── */

static int
ensure_scratch (fir_state_t *f, size_t num_samples)
{
  size_t needed = (f->num_taps - 1) + num_samples;
  if (needed <= f->scratch_cap)
    return 0;
  float complex *tmp
      = (float complex *)realloc (f->scratch, needed * sizeof (float complex));
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

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_cf32 (const float complex *JM_RESTRICT buf,
            const float complex *JM_RESTRICT h, size_t M,
            float complex *JM_RESTRICT out, size_t N)
{
  const __m512 SIGN = _mm512_load_ps (fir_sign);
  size_t       i    = 0;
  for (; i + 8 <= N; i += 8)
    {
      __m512 acc = _mm512_setzero_ps ();
      for (size_t k = 0; k < M; k++)
        {
          __m512 vx  = _mm512_loadu_ps ((const float *)&buf[i + M - 1 - k]);
          __m512 vxs = _mm512_permute_ps (vx, 0xB1);
          __m512 vhr = _mm512_set1_ps (crealf (h[k]));
          __m512 vhi = _mm512_set1_ps (cimagf (h[k]));
          acc        = _mm512_fmadd_ps (vhr, vx, acc);
          acc        = _mm512_fmadd_ps (vhi, _mm512_mul_ps (vxs, SIGN), acc);
        }
      _mm512_storeu_ps ((float *)&out[i], acc);
    }
  for (ptrdiff_t ii = (ptrdiff_t)i; (size_t)ii < N; ii++)
    {
      float     re = 0.0f, im = 0.0f;
      ptrdiff_t base = ii + (ptrdiff_t)(M - 1);
      for (ptrdiff_t k = 0; k < (ptrdiff_t)M; k++)
        {
          float xr = crealf (buf[base - k]);
          float xi = cimagf (buf[base - k]);
          re += crealf (h[k]) * xr - cimagf (h[k]) * xi;
          im += crealf (h[k]) * xi + cimagf (h[k]) * xr;
        }
      out[ii] = CMPLXF (re, im);
    }
}

#else /* scalar fallback for complex taps */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_cf32 (const float complex *JM_RESTRICT buf,
            const float complex *JM_RESTRICT h, size_t M,
            float complex *JM_RESTRICT out, size_t N)
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
 * Width  | ISA       | complex outputs/iter
 * -------|-----------|---------------------
 *    16  | AVX-512   | 8
 *     8  | AVX2+FMA  | 4
 *     1  | scalar    | loop never runs; tail handles all samples
 */

#if JM_SIMD_WIDTH_F32 >= 2

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_real_cf32 (const float complex *JM_RESTRICT buf,
                 const float *JM_RESTRICT h, size_t M,
                 float complex *JM_RESTRICT out, size_t N)
{
  const size_t stride = JM_SIMD_WIDTH_F32 / 2;
  size_t       i      = 0;
  for (; i + stride <= N; i += stride)
    {
      JM_VEC_F32 acc = JM_ZERO_F32 ();
      for (size_t k = 0; k < M; k++)
        JM_MAC_F32 (acc, (const float *)&buf[i + M - 1 - k], h[k]);
      JM_STORE_F32 ((float *)&out[i], acc);
    }
  for (ptrdiff_t ii = (ptrdiff_t)i; (size_t)ii < N; ii++)
    {
      float     re = 0.0f, im = 0.0f;
      ptrdiff_t base = ii + (ptrdiff_t)(M - 1);
      for (ptrdiff_t k = 0; k < (ptrdiff_t)M; k++)
        {
          re += h[k] * crealf (buf[base - k]);
          im += h[k] * cimagf (buf[base - k]);
        }
      out[ii] = CMPLXF (re, im);
    }
}

#else /* scalar — auto-vectorised */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
JM_HOT static void
inner_real_cf32 (const float complex *JM_RESTRICT buf,
                 const float *JM_RESTRICT h, size_t M,
                 float complex *JM_RESTRICT out, size_t N)
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
fir_execute (fir_state_t *state, const float complex *in, size_t n_in,
             float complex *out)
{
  if (!state || !in || !out || n_in == 0)
    return 0;
  if (ensure_scratch (state, n_in) < 0)
    return 0;

  const size_t dly = state->num_taps - 1;
  if (dly)
    memcpy (state->scratch, state->delay, dly * sizeof (float complex));
  memcpy (state->scratch + dly, in, n_in * sizeof (float complex));

  if (state->rtaps)
    inner_real_cf32 (state->scratch, state->rtaps, state->num_taps, out, n_in);
  else
    inner_cf32 (state->scratch, state->taps, state->num_taps, out, n_in);

  if (dly)
    memcpy (state->delay, state->scratch + n_in, dly * sizeof (float complex));

  return n_in;
}
