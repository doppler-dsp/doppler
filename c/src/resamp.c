/**
 * @file resamp.c
 * @brief Continuously-variable polyphase resampler for cf32 IQ.
 *
 * Two architectures under one API:
 *
 *   - Interpolation (rate >= 1): output-driven.  One NCO tick per
 *     output sample; overflow pushes the next input into a delay
 *     line.  Each output is a dot-product of the delay line with
 *     the polyphase branch selected by the NCO phase.
 *
 *   - Decimation (rate < 1): input-driven, transposed form.  One
 *     NCO tick per input sample; the input scalar is multiplied by
 *     all N branch coefficients and the products accumulate in N
 *     integrate-and-dump registers.  On NCO overflow the I&D
 *     registers dump into a transposed tapped delay line that
 *     shifts and produces one output sample.
 *
 * Both paths use the same 32-bit phase accumulator for polyphase
 * branch selection.  The top log2(num_phases) bits of the raw
 * phase word index into the coefficient bank.
 */

#include "dp/resamp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

/* ------------------------------------------------------------------ */
/* Overflow detection (same as nco.c)                                 */
/* ------------------------------------------------------------------ */
#if defined(__GNUC__) || defined(__clang__)
#define ADD_OVF(a, b, res)                                     \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a),             \
                                    (uint32_t)(b),             \
                                    (uint32_t *)(res)))
#else
static inline uint8_t
add_ovf_ (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define ADD_OVF(a, b, res) add_ovf_ ((a), (b), (res))
#endif

/* ------------------------------------------------------------------ */
/* Normalised frequency → uint32 phase increment                      */
/* ------------------------------------------------------------------ */
static uint32_t
norm_to_inc (double f)
{
  f -= floor (f);
  return (uint32_t)(f * 4294967296.0);
}

/* ------------------------------------------------------------------ */
/* Next power of two → log2                                           */
/* ------------------------------------------------------------------ */
static unsigned
log2_u (size_t v)
{
  unsigned r = 0;
  while ((1u << r) < v)
    r++;
  return r;
}

/* ================================================================== */
/* Internal struct                                                    */
/* ================================================================== */

struct dp_resamp_cf32
{
  /* Configuration */
  double   rate;
  size_t   num_phases;
  size_t   num_taps;
  unsigned log2_phases;
  int      upsample;       /* 1 = interpolation, 0 = decimation */

  /* Coefficient bank [num_phases][num_taps], float32.
   * For decimation the bank is pre-reversed and pre-scaled by r. */
  float *bank;

  /* Phase accumulator */
  uint32_t phase;
  uint32_t phase_inc;

  /* Interpolator state: dual-buffer delay line.
   * Capacity = next power of 2 >= num_taps. */
  dp_cf32_t *delay_buf;    /* 2 * delay_cap elements              */
  size_t     delay_cap;
  size_t     delay_mask;
  size_t     delay_head;

  /* Decimator state (transposed form):
   *   iad[num_taps]     — integrate-and-dump accumulators
   *   tfd[num_taps - 1] — transposed delay line registers */
  dp_cf32_t *iad;
  dp_cf32_t *tfd;
};

/* ================================================================== */
/* Delay-line helpers (inlined for the hot path)                      */
/* ================================================================== */

static inline void
dl_push (struct dp_resamp_cf32 *r, dp_cf32_t x)
{
  r->delay_head = (r->delay_head - 1) & r->delay_mask;
  r->delay_buf[r->delay_head]                = x;
  r->delay_buf[r->delay_head + r->delay_cap] = x;
}

static inline const dp_cf32_t *
dl_ptr (const struct dp_resamp_cf32 *r)
{
  return &r->delay_buf[r->delay_head];
}

/* ================================================================== */
/* Branch lookup                                                      */
/* ================================================================== */

static inline const float *
get_branch (const struct dp_resamp_cf32 *r, uint32_t phase)
{
  size_t idx = phase >> (32 - r->log2_phases);
  return &r->bank[idx * r->num_taps];
}

/* ================================================================== */
/* Lifecycle                                                          */
/* ================================================================== */

dp_resamp_cf32_t *
dp_resamp_cf32_create (size_t       num_phases,
                       size_t       num_taps,
                       const float *bank,
                       double       rate)
{
  if (!num_phases || !num_taps || !bank || rate <= 0.0)
    return NULL;

  dp_resamp_cf32_t *r = calloc (1, sizeof *r);
  if (!r)
    return NULL;

  r->rate        = rate;
  r->num_phases  = num_phases;
  r->num_taps    = num_taps;
  r->log2_phases = log2_u (num_phases);
  r->upsample    = (rate >= 1.0);

  /* Copy and condition the coefficient bank */
  size_t bank_len = num_phases * num_taps;
  r->bank = malloc (bank_len * sizeof (float));
  if (!r->bank)
    {
      free (r);
      return NULL;
    }

  if (r->upsample)
    {
      memcpy (r->bank, bank, bank_len * sizeof (float));
    }
  else
    {
      /* Decimator: reverse each phase row and scale by r */
      float scale = (float)rate;
      for (size_t p = 0; p < num_phases; p++)
        {
          const float *src = &bank[p * num_taps];
          float       *dst = &r->bank[p * num_taps];
          for (size_t t = 0; t < num_taps; t++)
            dst[t] = src[num_taps - 1 - t] * scale;
        }
    }

  /* NCO phase accumulator */
  r->phase     = 0;
  r->phase_inc = r->upsample ? norm_to_inc (1.0 / rate)
                              : norm_to_inc (rate);

  /* Delay line (interpolator) — power-of-2 dual buffer */
  r->delay_cap  = 1;
  while (r->delay_cap < num_taps)
    r->delay_cap <<= 1;
  r->delay_mask = r->delay_cap - 1;
  r->delay_head = 0;
  r->delay_buf  = calloc (2 * r->delay_cap, sizeof (dp_cf32_t));
  if (!r->delay_buf)
    {
      free (r->bank);
      free (r);
      return NULL;
    }

  /* I&D + transposed delay line (decimator).
   * Round up to multiple of 8 complex (64 bytes = AVX-512 line)
   * so SIMD stores never cross a cache line boundary. */
  size_t iad_n = (num_taps + 7) & ~(size_t)7;
  size_t tfd_n = num_taps > 1 ? num_taps - 1 : 1;
  size_t tfd_alloc = (tfd_n + 7) & ~(size_t)7;
  r->iad = calloc (iad_n, sizeof (dp_cf32_t));
  r->tfd = calloc (tfd_alloc, sizeof (dp_cf32_t));
  if (!r->iad || !r->tfd)
    {
      free (r->delay_buf);
      free (r->iad);
      free (r->tfd);
      free (r->bank);
      free (r);
      return NULL;
    }

  return r;
}

void
dp_resamp_cf32_destroy (dp_resamp_cf32_t *r)
{
  if (!r)
    return;
  free (r->delay_buf);
  free (r->iad);
  free (r->tfd);
  free (r->bank);
  free (r);
}

void
dp_resamp_cf32_reset (dp_resamp_cf32_t *r)
{
  r->phase      = 0;
  r->delay_head = 0;
  memset (r->delay_buf, 0,
          2 * r->delay_cap * sizeof (dp_cf32_t));
  memset (r->iad, 0, r->num_taps * sizeof (dp_cf32_t));
  size_t tfd_n = r->num_taps > 1 ? r->num_taps - 1 : 1;
  memset (r->tfd, 0, tfd_n * sizeof (dp_cf32_t));
}

/* ================================================================== */
/* Properties                                                         */
/* ================================================================== */

double
dp_resamp_cf32_rate (const dp_resamp_cf32_t *r)
{
  return r->rate;
}

size_t
dp_resamp_cf32_num_phases (const dp_resamp_cf32_t *r)
{
  return r->num_phases;
}

size_t
dp_resamp_cf32_num_taps (const dp_resamp_cf32_t *r)
{
  return r->num_taps;
}

/* ================================================================== */
/* Interpolation (output-driven)                                      */
/* ================================================================== */

/*
 * Dot-product: Σ win[j].iq * h[j], j = 0..N-1
 *
 * win[] is interleaved cf32 [i0,q0, i1,q1, ...], h[] is float32.
 * AVX-512: process 8 taps (16 floats) per iteration.
 *   - Load 8 coefficients, duplicate each: [h0,h0,h1,h1,...,h7,h7]
 *   - Multiply against the interleaved win[] block.
 *   - Horizontal reduction at the end.
 */

#ifdef __AVX512F__

/* Permute indices: duplicate 8 floats from broadcast_f32x8 output.
 * broadcast_f32x8([h0..h7]) = [h0,h1,h2,h3,h4,h5,h6,h7,
 *                               h0,h1,h2,h3,h4,h5,h6,h7]
 * We want: [h0,h0,h1,h1,h2,h2,h3,h3,h4,h4,h5,h5,h6,h6,h7,h7]
 * Index into the broadcast result (positions 0-15):
 *   pos 0,1 → h0 = index 0
 *   pos 2,3 → h1 = index 1
 *   ...
 *   pos 14,15 → h7 = index 7
 */
static const int32_t dup_idx_arr[16] = {
    0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7
};

static inline void
dot_cf32_f32 (const dp_cf32_t *win, const float *h,
              size_t N, float *out_i, float *out_q)
{
  __m512 acc = _mm512_setzero_ps ();
  __m512i dup_idx = _mm512_loadu_si512 (dup_idx_arr);

  size_t j = 0;
  for (; j + 8 <= N; j += 8)
    {
      __m256 hraw = _mm256_loadu_ps (&h[j]);
      __m512 hb   = _mm512_broadcast_f32x8 (hraw);
      __m512 hdup = _mm512_permutexvar_ps (dup_idx, hb);
      __m512 w    = _mm512_loadu_ps ((const float *)&win[j]);
      acc = _mm512_fmadd_ps (w, hdup, acc);
    }

  /* Horizontal reduce: sum 16 floats, split even/odd for I and Q */
  __m256 lo  = _mm512_castps512_ps256 (acc);
  __m256 hi  = _mm512_extractf32x8_ps (acc, 1);
  __m256 s8  = _mm256_add_ps (lo, hi);
  __m128 s4l = _mm256_castps256_ps128 (s8);
  __m128 s4h = _mm256_extractf128_ps (s8, 1);
  __m128 s4  = _mm_add_ps (s4l, s4h);
  /* s4 = [i0+i2, q0+q2, i1+i3, q1+q3] */
  __m128 s2  = _mm_add_ps (s4,
                            _mm_movehl_ps (s4, s4));
  /* s2[0] = I sum, s2[1] = Q sum */
  float si = _mm_cvtss_f32 (s2);
  float sq = _mm_cvtss_f32 (_mm_shuffle_ps (s2, s2, 0x1));

  /* Scalar tail */
  for (; j < N; j++)
    {
      si += win[j].i * h[j];
      sq += win[j].q * h[j];
    }

  *out_i = si;
  *out_q = sq;
}

#else /* scalar fallback */

static inline void
dot_cf32_f32 (const dp_cf32_t *win, const float *h,
              size_t N, float *out_i, float *out_q)
{
  float si = 0.0f, sq = 0.0f;
  for (size_t j = 0; j < N; j++)
    {
      si += win[j].i * h[j];
      sq += win[j].q * h[j];
    }
  *out_i = si;
  *out_q = sq;
}

#endif /* __AVX512F__ */

static size_t
interp_execute (dp_resamp_cf32_t *r,
                const dp_cf32_t  *in,
                size_t            num_in,
                dp_cf32_t        *out,
                size_t            max_out)
{
  size_t   xi  = 0;
  size_t   oi  = 0;
  size_t   N   = r->num_taps;
  uint32_t ph  = r->phase;
  uint32_t inc = r->phase_inc;

  while (xi < num_in && oi < max_out)
    {
      /* Advance NCO */
      uint32_t new_ph;
      uint8_t  ovf = ADD_OVF (ph, inc, &new_ph);

      /* Select branch and MAC */
      const float     *h   = get_branch (r, ph);
      const dp_cf32_t *win = dl_ptr (r);
      dot_cf32_f32 (win, h, N, &out[oi].i, &out[oi].q);
      oi++;

      ph = new_ph;

      /* Push next input on overflow */
      if (ovf)
        {
          dl_push (r, in[xi]);
          xi++;
        }
    }

  r->phase = ph;
  return oi;
}

/* ================================================================== */
/* Decimation (input-driven, transposed form)                         */
/* ================================================================== */

/*
 * I&D accumulate: iad[j].iq += x.iq * h[j], j = 0..N-1
 *
 * AVX-512: broadcast the input sample as [xi_i, xi_q] repeated 8×,
 * duplicate each h[j] to match the interleaved layout,
 * then FMA directly into the I&D array.  8 taps per iteration.
 */

#ifdef __AVX512F__

static inline void
iad_madd (dp_cf32_t *iad, const float *h, size_t N,
          float xi_i, float xi_q)
{
  /* Build [xi_i, xi_q] × 8 across 512 bits */
  __m256 xp = _mm256_set_ps (xi_q, xi_i, xi_q, xi_i,
                              xi_q, xi_i, xi_q, xi_i);
  __m512 xv = _mm512_broadcast_f32x8 (xp);
  __m512i dup_idx = _mm512_loadu_si512 (dup_idx_arr);

  size_t j = 0;
  for (; j + 8 <= N; j += 8)
    {
      __m256 hraw = _mm256_loadu_ps (&h[j]);
      __m512 hb   = _mm512_broadcast_f32x8 (hraw);
      __m512 hdup = _mm512_permutexvar_ps (dup_idx, hb);

      float  *p   = (float *)&iad[j];
      __m512  av  = _mm512_loadu_ps (p);
      av = _mm512_fmadd_ps (xv, hdup, av);
      _mm512_storeu_ps (p, av);
    }

  /* Scalar tail */
  for (; j < N; j++)
    {
      iad[j].i += xi_i * h[j];
      iad[j].q += xi_q * h[j];
    }
}

#else /* scalar fallback */

static inline void
iad_madd (dp_cf32_t *iad, const float *h, size_t N,
          float xi_i, float xi_q)
{
  for (size_t j = 0; j < N; j++)
    {
      iad[j].i += xi_i * h[j];
      iad[j].q += xi_q * h[j];
    }
}

#endif /* __AVX512F__ */

static size_t
decim_execute (dp_resamp_cf32_t *r,
               const dp_cf32_t  *in,
               size_t            num_in,
               dp_cf32_t        *out,
               size_t            max_out)
{
  size_t    oi  = 0;
  size_t    N   = r->num_taps;
  uint32_t  ph  = r->phase;
  uint32_t  inc = r->phase_inc;
  dp_cf32_t *iad = r->iad;
  dp_cf32_t *tfd = r->tfd;

  for (size_t xi = 0; xi < num_in && oi < max_out; xi++)
    {
      /* Advance NCO */
      uint32_t new_ph;
      uint8_t  ovf = ADD_OVF (ph, inc, &new_ph);

      /* Scalar × all N branch coefficients → I&D */
      const float *h = get_branch (r, ph);
      iad_madd (iad, h, N, in[xi].i, in[xi].q);

      ph = new_ph;

      if (ovf)
        {
          /* Transposed delay line: shift and add */
          float yi = iad[0].i + tfd[0].i;
          float yq = iad[0].q + tfd[0].q;

#ifdef __AVX512F__
          /* Process 8 complex taps per AVX-512 iteration.
           * Reads iad[j+1..j+8] and tfd[j+1..j+8],
           * writes tfd[j..j+7].  No overlap. */
          size_t j = 0;
          for (; j + 2 + 8 <= N; j += 8)
            {
              __m512 a = _mm512_loadu_ps (
                  (const float *)&iad[j + 1]);
              __m512 b = _mm512_loadu_ps (
                  (const float *)&tfd[j + 1]);
              _mm512_storeu_ps ((float *)&tfd[j],
                                _mm512_add_ps (a, b));
            }
          for (; j + 2 < N; j++)
            {
              tfd[j].i = iad[j + 1].i + tfd[j + 1].i;
              tfd[j].q = iad[j + 1].q + tfd[j + 1].q;
            }
#else
          for (size_t j = 0; j + 2 < N; j++)
            {
              tfd[j].i = iad[j + 1].i + tfd[j + 1].i;
              tfd[j].q = iad[j + 1].q + tfd[j + 1].q;
            }
#endif
          tfd[N - 2].i = iad[N - 1].i;
          tfd[N - 2].q = iad[N - 1].q;

          out[oi].i = yi;
          out[oi].q = yq;
          oi++;

          /* Reset I&D */
          memset (iad, 0, N * sizeof (dp_cf32_t));
        }
    }

  r->phase = ph;
  return oi;
}

/* ================================================================== */
/* Public execute — dispatches to interp or decim                     */
/* ================================================================== */

size_t
dp_resamp_cf32_execute (dp_resamp_cf32_t *r,
                        const dp_cf32_t  *in,
                        size_t            num_in,
                        dp_cf32_t        *out,
                        size_t            max_out)
{
  if (r->upsample)
    return interp_execute (r, in, num_in, out, max_out);
  else
    return decim_execute (r, in, num_in, out, max_out);
}
