/**
 * @file fir.c
 * @brief Complex FIR filter implementation.
 *
 * Hot loop strategy (CF32, interleaved I/Q):
 *
 *   Memory layout:  [r0, i0, r1, i1, ..., r7, i7]  ← 8 complex = __m512
 *
 *   For each tap h[k] = h_r + j*h_i and 8 parallel output lanes:
 *
 *     vx      = loadu_ps(&buf[n-k])          [r0,i0,...,r7,i7]
 *     vx_swap = permute_ps(vx, 0xB1)         [i0,r0,...,i7,r7]
 *     acc    += fmadd(h_r, vx,             acc)   += h_r*[r,i]
 *     acc    += fmadd(h_i, vx_swap*SIGN,  acc)   += h_i*[-i,+r]
 *
 *   Result per lane pair [2m, 2m+1]:
 *     acc[2m]   = h_r*r_m - h_i*i_m  = re(h*x_m)
 *     acc[2m+1] = h_r*i_m + h_i*r_m  = im(h*x_m)
 *
 *   Total: 2 FMA + 1 permute + 1 mul per tap, 8 outputs/iteration.
 *
 * For CI8/CI16/CI32 inputs the hot path upcasts to CF32 into the
 * scratch buffer before running the same inner loop.
 */

#include "dp/fir.h"
#include "dp/stream.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Internal state
 * ========================================================================= */

struct dp_fir
{
  dp_cf32_t *taps;    /* complex tap coefficients (NULL if real-tap)    */
  float *rtaps;       /* real tap coefficients (NULL if complex-tap)    */
  dp_cf32_t *delay;   /* delay line, length num_taps - 1                */
  dp_cf32_t *scratch; /* flat [delay | input] workspace, reused         */
  size_t scratch_cap; /* capacity of scratch in complex samples         */
  size_t num_taps;
};

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

dp_fir_t *
dp_fir_create (const dp_cf32_t *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  dp_fir_t *f = (dp_fir_t *)calloc (1, sizeof (dp_fir_t));
  if (!f)
    return NULL;

  f->taps = (dp_cf32_t *)malloc (num_taps * sizeof (dp_cf32_t));
  if (!f->taps)
    {
      free (f);
      return NULL;
    }
  memcpy (f->taps, taps, num_taps * sizeof (dp_cf32_t));

  if (num_taps > 1)
    {
      f->delay = (dp_cf32_t *)calloc (num_taps - 1, sizeof (dp_cf32_t));
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

void
dp_fir_reset (dp_fir_t *f)
{
  if (f && f->delay && f->num_taps > 1)
    memset (f->delay, 0, (f->num_taps - 1) * sizeof (dp_cf32_t));
}

void
dp_fir_destroy (dp_fir_t *f)
{
  if (!f)
    return;
  free (f->taps);
  free (f->rtaps);
  free (f->delay);
  free (f->scratch);
  free (f);
}

dp_fir_t *
dp_fir_create_real (const float *taps, size_t num_taps)
{
  if (!taps || num_taps == 0)
    return NULL;

  dp_fir_t *f = (dp_fir_t *)calloc (1, sizeof (dp_fir_t));
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
      f->delay = (dp_cf32_t *)calloc (num_taps - 1, sizeof (dp_cf32_t));
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

/* =========================================================================
 * Scratch buffer management
 *
 * scratch = [delay_line | new_input_as_cf32]
 * length  = (num_taps - 1) + num_samples
 *
 * y[n] = sum_k h[k] * scratch[n + (num_taps-1) - k]
 * ========================================================================= */

static int
ensure_scratch (dp_fir_t *f, size_t num_samples)
{
  size_t needed = (f->num_taps - 1) + num_samples;
  if (needed <= f->scratch_cap)
    return 0;

  dp_cf32_t *tmp
      = (dp_cf32_t *)realloc (f->scratch, needed * sizeof (dp_cf32_t));
  if (!tmp)
    return -1;

  f->scratch = tmp;
  f->scratch_cap = needed;
  return 0;
}

/* =========================================================================
 * SIMD hot loop — CF32 interleaved I/Q
 * ========================================================================= */

#if defined(__AVX512F__) && defined(__AVX512DQ__)
#include <immintrin.h>

/*
 * Sign mask for complex multiply: negate even-indexed lanes (real parts)
 * so that h_i * vx_swap gives [-h_i*i, +h_i*r] per complex pair.
 */
static const float _dp_fir_sign[16] __attribute__ ((aligned (64)))
= { -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1 };

/*
 * Disable -faggressive-loop-optimizations for this function only.
 * GCC flags the scalar tail as theoretically overflowing ptrdiff_t at
 * ~2^61 iterations — impossible for any real DSP buffer but technically
 * correct UB.  The SIMD path is unaffected by this attribute.
 */
#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
static void
inner_loop_avx512 (const dp_cf32_t *buf, /* scratch: delay | input */
                   const dp_cf32_t *h, size_t num_taps, dp_cf32_t *out,
                   size_t num_samples)
{
  const __m512 SIGN = _mm512_load_ps (_dp_fir_sign);
  const size_t M = num_taps;

  /* --- 8-wide SIMD loop -------------------------------------------- */
  size_t i = 0;
  for (; i + 8 <= num_samples; i += 8)
    {
      __m512 acc = _mm512_setzero_ps ();

      for (size_t k = 0; k < M; k++)
        {
          /*
           * 8 consecutive complex samples for tap k, starting at
           * position (i + M - 1 - k) in the scratch buffer.
           */
          const float *xp = (const float *)&buf[i + M - 1 - k];
          __m512 vx = _mm512_loadu_ps (xp);

          /* Swap I and Q within each complex pair: [r,i]→[i,r] */
          __m512 vx_swap = _mm512_permute_ps (vx, 0xB1);

          __m512 vhr = _mm512_set1_ps (h[k].i); /* broadcast tap real */
          __m512 vhi = _mm512_set1_ps (h[k].q); /* broadcast tap imag */

          /* acc += h_r * [r, i, ...] */
          acc = _mm512_fmadd_ps (vhr, vx, acc);

          /* acc += h_i * [-i, +r, ...] = h_i * (vx_swap * SIGN) */
          acc = _mm512_fmadd_ps (vhi, _mm512_mul_ps (vx_swap, SIGN), acc);
        }

      _mm512_storeu_ps ((float *)&out[i], acc);
    }

  /* --- scalar tail -------------------------------------------------- */
  /*
   * Use signed ptrdiff_t so the compiler can prove base_pos >= k >= 0
   * and avoids -Waggressive-loop-optimizations on unsigned subtraction.
   */
  for (ptrdiff_t ii = (ptrdiff_t)i; (size_t)ii < num_samples; ii++)
    {
      float re = 0.0f, im = 0.0f;
      ptrdiff_t base_pos = ii + (ptrdiff_t)(M - 1);
      for (ptrdiff_t k = 0; k < (ptrdiff_t)M; k++)
        {
          float xr = buf[base_pos - k].i;
          float xi = buf[base_pos - k].q;
          re += h[k].i * xr - h[k].q * xi;
          im += h[k].i * xi + h[k].q * xr;
        }
      out[ii].i = re;
      out[ii].q = im;
    }
}

#else /* scalar fallback */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
static void
inner_loop_avx512 (const dp_cf32_t *buf, const dp_cf32_t *h, size_t num_taps,
                   dp_cf32_t *out, size_t num_samples)
{
  const ptrdiff_t M = (ptrdiff_t)num_taps;
  for (ptrdiff_t ii = 0; (size_t)ii < num_samples; ii++)
    {
      float re = 0.0f, im = 0.0f;
      ptrdiff_t base_pos = ii + M - 1;
      for (ptrdiff_t k = 0; k < M; k++)
        {
          float xr = buf[base_pos - k].i;
          float xi = buf[base_pos - k].q;
          re += h[k].i * xr - h[k].q * xi;
          im += h[k].i * xi + h[k].q * xr;
        }
      out[ii].i = re;
      out[ii].q = im;
    }
}

#endif /* AVX-512 complex */

/* =========================================================================
 * SIMD hot loop — real taps, CF32 interleaved I/Q signal
 *
 * With real h[k], the complex multiply reduces to a scalar multiply:
 *   re(y[n]) = sum_k h[k] * re(x[n-k])
 *   im(y[n]) = sum_k h[k] * im(x[n-k])
 *
 * Because I and Q are contiguous floats and h[k] multiplies both
 * identically, we broadcast h[k] to all 16 float lanes and FMA
 * directly against the interleaved buffer — no permute, no sign mask.
 *
 * Cost per tap: 1 FMA  (vs 2 FMA + 1 permute + 1 mul for complex taps)
 * ========================================================================= */

#if defined(__AVX512F__)

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
static void
inner_loop_real_avx512 (const dp_cf32_t *buf, const float *h, size_t num_taps,
                        dp_cf32_t *out, size_t num_samples)
{
  const size_t M = num_taps;

  size_t i = 0;
  for (; i + 8 <= num_samples; i += 8)
    {
      __m512 acc = _mm512_setzero_ps ();

      for (size_t k = 0; k < M; k++)
        {
          const float *xp = (const float *)&buf[i + M - 1 - k];
          __m512 vx = _mm512_loadu_ps (xp);
          __m512 vh = _mm512_set1_ps (h[k]);
          acc = _mm512_fmadd_ps (vh, vx, acc);
        }

      _mm512_storeu_ps ((float *)&out[i], acc);
    }

  /* scalar tail */
  for (ptrdiff_t ii = (ptrdiff_t)i; (size_t)ii < num_samples; ii++)
    {
      float re = 0.0f, im = 0.0f;
      ptrdiff_t base_pos = ii + (ptrdiff_t)(M - 1);
      for (ptrdiff_t k = 0; k < (ptrdiff_t)M; k++)
        {
          re += h[k] * buf[base_pos - k].i;
          im += h[k] * buf[base_pos - k].q;
        }
      out[ii].i = re;
      out[ii].q = im;
    }
}

#else /* scalar fallback */

#if defined(__GNUC__) && !defined(__clang__)
__attribute__ ((optimize ("no-aggressive-loop-optimizations")))
#endif
static void
inner_loop_real_avx512 (const dp_cf32_t *buf, const float *h, size_t num_taps,
                        dp_cf32_t *out, size_t num_samples)
{
  const ptrdiff_t M = (ptrdiff_t)num_taps;
  for (ptrdiff_t ii = 0; (size_t)ii < num_samples; ii++)
    {
      float re = 0.0f, im = 0.0f;
      ptrdiff_t base_pos = ii + M - 1;
      for (ptrdiff_t k = 0; k < M; k++)
        {
          re += h[k] * buf[base_pos - k].i;
          im += h[k] * buf[base_pos - k].q;
        }
      out[ii].i = re;
      out[ii].q = im;
    }
}

#endif /* AVX-512 real */

/* =========================================================================
 * Shared execute core: fill scratch, run hot loop, update delay line
 * ========================================================================= */

static int
execute_core (dp_fir_t *f, dp_cf32_t *out, size_t num_samples)
{
  inner_loop_avx512 (f->scratch, f->taps, f->num_taps, out, num_samples);

  /* Update delay line: last (num_taps-1) samples of scratch */
  if (f->num_taps > 1)
    memcpy (f->delay, f->scratch + num_samples,
            (f->num_taps - 1) * sizeof (dp_cf32_t));

  return 0;
}

/* =========================================================================
 * CF32 execute — native type, zero conversion cost
 * ========================================================================= */

int
dp_fir_execute_cf32 (dp_fir_t *f, const dp_cf32_t *in, dp_cf32_t *out,
                     size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  /* scratch = [delay | in] */
  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));
  memcpy (f->scratch + dly, in, num_samples * sizeof (dp_cf32_t));

  return execute_core (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * CI8 → CF32  (RTL-SDR, HackRF: 2 bytes/sample)
 * ========================================================================= */

int
dp_fir_execute_ci8 (dp_fir_t *f, const dp_ci8_t *in, dp_cf32_t *out,
                    size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  /* Upcast CI8 → CF32 in-place into scratch[dly..] */
  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__) && defined(__AVX512BW__)
  /*
   * Process 16 CI8 samples (32 int8 bytes) → 16 CF32 per iteration.
   * Load 16 int8 pairs, sign-extend to int32, convert to float.
   */
  size_t n = 0;
  for (; n + 16 <= num_samples; n += 16)
    {
      /* Load 32 int8 → 256-bit lane */
      __m256i v8 = _mm256_loadu_si256 ((const __m256i *)&in[n]);

      /* Sign-extend each int8 lane to int32: low 16 and high 16 */
      __m512i v32_lo = _mm512_cvtepi8_epi32 (_mm256_castsi256_si128 (v8));
      __m512i v32_hi = _mm512_cvtepi8_epi32 (_mm256_extracti128_si256 (v8, 1));

      /* Convert int32 → float */
      __m512 vf_lo = _mm512_cvtepi32_ps (v32_lo);
      __m512 vf_hi = _mm512_cvtepi32_ps (v32_hi);

      _mm512_storeu_ps ((float *)(dst + n), vf_lo);
      _mm512_storeu_ps ((float *)(dst + n + 8), vf_hi);
    }
  /* scalar tail */
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * CI16 → CF32  (LimeSDR, USRP, PlutoSDR: 4 bytes/sample)
 * ========================================================================= */

int
dp_fir_execute_ci16 (dp_fir_t *f, const dp_ci16_t *in, dp_cf32_t *out,
                     size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__)
  /*
   * Process 8 CI16 samples (16 int16 = 32 bytes) → 8 CF32 per iter.
   * _mm512_cvtepi16_epi32 sign-extends 16×int16 → 16×int32 in one op.
   */
  size_t n = 0;
  for (; n + 8 <= num_samples; n += 8)
    {
      __m256i v16 = _mm256_loadu_si256 ((const __m256i *)&in[n]);
      __m512i v32 = _mm512_cvtepi16_epi32 (v16);
      _mm512_storeu_ps ((float *)(dst + n), _mm512_cvtepi32_ps (v32));
    }
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * CI32 → CF32  (4-byte integer samples)
 * ========================================================================= */

int
dp_fir_execute_ci32 (dp_fir_t *f, const dp_ci32_t *in, dp_cf32_t *out,
                     size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__)
  size_t n = 0;
  for (; n + 8 <= num_samples; n += 8)
    {
      __m512i v32 = _mm512_loadu_si512 ((const __m512i *)&in[n]);
      _mm512_storeu_ps ((float *)(dst + n), _mm512_cvtepi32_ps (v32));
    }
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * Real-tap execute core
 * ========================================================================= */

static int
execute_core_real (dp_fir_t *f, dp_cf32_t *out, size_t num_samples)
{
  inner_loop_real_avx512 (f->scratch, f->rtaps, f->num_taps, out, num_samples);

  if (f->num_taps > 1)
    memcpy (f->delay, f->scratch + num_samples,
            (f->num_taps - 1) * sizeof (dp_cf32_t));

  return 0;
}

/* =========================================================================
 * Real-tap execute — CF32 input
 * ========================================================================= */

int
dp_fir_execute_real_cf32 (dp_fir_t *f, const dp_cf32_t *in, dp_cf32_t *out,
                          size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));
  memcpy (f->scratch + dly, in, num_samples * sizeof (dp_cf32_t));

  return execute_core_real (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * Real-tap execute — CI8 input
 * ========================================================================= */

int
dp_fir_execute_real_ci8 (dp_fir_t *f, const dp_ci8_t *in, dp_cf32_t *out,
                         size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__) && defined(__AVX512BW__)
  size_t n = 0;
  for (; n + 16 <= num_samples; n += 16)
    {
      __m256i v8 = _mm256_loadu_si256 ((const __m256i *)&in[n]);
      __m512i v32_lo = _mm512_cvtepi8_epi32 (_mm256_castsi256_si128 (v8));
      __m512i v32_hi = _mm512_cvtepi8_epi32 (_mm256_extracti128_si256 (v8, 1));
      _mm512_storeu_ps ((float *)(dst + n), _mm512_cvtepi32_ps (v32_lo));
      _mm512_storeu_ps ((float *)(dst + n + 8), _mm512_cvtepi32_ps (v32_hi));
    }
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core_real (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * Real-tap execute — CI16 input
 * ========================================================================= */

int
dp_fir_execute_real_ci16 (dp_fir_t *f, const dp_ci16_t *in, dp_cf32_t *out,
                          size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__)
  size_t n = 0;
  for (; n + 8 <= num_samples; n += 8)
    {
      __m256i v16 = _mm256_loadu_si256 ((const __m256i *)&in[n]);
      __m512i v32 = _mm512_cvtepi16_epi32 (v16);
      _mm512_storeu_ps ((float *)(dst + n), _mm512_cvtepi32_ps (v32));
    }
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core_real (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}

/* =========================================================================
 * Real-tap execute — CI32 input
 * ========================================================================= */

int
dp_fir_execute_real_ci32 (dp_fir_t *f, const dp_ci32_t *in, dp_cf32_t *out,
                          size_t num_samples)
{
  if (!f || !in || !out || num_samples == 0)
    return DP_ERR_INVALID;

  if (ensure_scratch (f, num_samples) < 0)
    return DP_ERR_MEMORY;

  const size_t dly = f->num_taps - 1;

  if (dly)
    memcpy (f->scratch, f->delay, dly * sizeof (dp_cf32_t));

  dp_cf32_t *dst = f->scratch + dly;

#if defined(__AVX512F__)
  size_t n = 0;
  for (; n + 8 <= num_samples; n += 8)
    {
      __m512i v32 = _mm512_loadu_si512 ((const __m512i *)&in[n]);
      _mm512_storeu_ps ((float *)(dst + n), _mm512_cvtepi32_ps (v32));
    }
  for (; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#else
  for (size_t n = 0; n < num_samples; n++)
    {
      dst[n].i = (float)in[n].i;
      dst[n].q = (float)in[n].q;
    }
#endif

  return execute_core_real (f, out, num_samples) == 0 ? DP_OK : DP_ERR_MEMORY;
}
