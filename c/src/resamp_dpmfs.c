/**
 * @file resamp_dpmfs.c
 * @brief DPMFS polyphase resampler for cf32 IQ.
 *
 * Dual Phase Modified Farrow Structure: replaces the large polyphase
 * coefficient table with a compact polynomial bank (M+1)*N*2 float32.
 * Both interpolation and decimation paths share the same 32-bit NCO as
 * resamp.c; only the coefficient evaluation differs.
 *
 * ### Hot-path factoring
 *
 * Coefficients are stored row-major: c[j][m*N+k].
 *
 * Rather than computing M+1 separate dot products and then running
 * Horner on M+1 complex scalars (the v[m] form), we materialise a
 * real h_eff[N] vector via M vectorised FMA passes over c[j][m*N..],
 * then do a single dot_cf32_f32 / iad_madd call:
 *
 *   h_eff[k] = c[M][k]
 *   for m = M-1 downto 0:
 *       h_eff[k] = h_eff[k] * mu_J + c[m][k]    ← one AVX-512 pass
 *   y = dot_cf32_f32(win, h_eff, N)
 *
 * ### Multiply count (M=3, N=19)
 *
 *   v[m] form:  2*(M+1)*N + 2*M = 158 real muls
 *   h_eff form: M*N + 2*N      =  95 real muls  (-40%)
 */

#include "dp/resamp_dpmfs.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

/* Maximum taps per polyphase branch (stack h_eff buffer) */
#define DP_DPMFS_MAX_N 256

/* ------------------------------------------------------------------ */
/* Overflow detection (same as resamp.c / nco.c)                      */
/* ------------------------------------------------------------------ */
#if defined(__GNUC__) || defined(__clang__)
#define ADD_OVF(a, b, res)                                                    \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),             \
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

/* ================================================================== */
/* Internal struct                                                    */
/* ================================================================== */

struct dp_resamp_dpmfs
{
  double rate;
  size_t M;     /* polynomial order (1..3)   */
  size_t N;     /* taps per phase             */
  int upsample; /* 1 = interpolation          */

  /* c[j][(M+1)*N]: row-major [m*N+k] */
  float *c[2];

  /* NCO */
  uint32_t phase;
  uint32_t phase_inc;

  /* Interpolator: dual-buffer delay line (power-of-2) */
  dp_cf32_t *delay_buf;
  size_t delay_cap;
  size_t delay_mask;
  size_t delay_head;

  /* Decimator (transposed form): single iad[N] + tfd[N-1].
   * iad is rounded up to multiple of 8 for AVX-512 safety. */
  dp_cf32_t *iad;
  dp_cf32_t *tfd;
};

/* ================================================================== */
/* Phase → j, μ_J                                                    */
/* ================================================================== */

static inline void
phase_to_j_mu (uint32_t phase, int *j, float *mu_J)
{
  *j = (int)(phase >> 31);
  *mu_J = (float)(phase & 0x7FFFFFFFu) * (1.0f / 2147483648.0f);
}

/* ================================================================== */
/* Delay-line helpers                                                 */
/* ================================================================== */

static inline void
dl_push (struct dp_resamp_dpmfs *r, dp_cf32_t x)
{
  r->delay_head = (r->delay_head - 1) & r->delay_mask;
  r->delay_buf[r->delay_head] = x;
  r->delay_buf[r->delay_head + r->delay_cap] = x;
}

static inline const dp_cf32_t *
dl_ptr (const struct dp_resamp_dpmfs *r)
{
  return &r->delay_buf[r->delay_head];
}

/* ================================================================== */
/* compute_h_eff — Horner over rows of row-major c[j]               */
/*                                                                    */
/* h_eff[k] = c[j][M*N+k]*mu_J^M + ... + c[j][0*N+k]               */
/*                                                                    */
/* Implemented as M sequential vector FMA passes over the N-wide     */
/* rows, allowing the compiler to emit AVX-512 across all N taps     */
/* simultaneously.  No loop-carried dependency between k values.     */
/* ================================================================== */

static inline void
compute_h_eff (const float *restrict cj, size_t M, size_t N, float mu_J,
               float *restrict h_eff)
{
  /* Process each 16-wide chunk through all M Horner passes before
   * writing to h_eff[].  Keeping h_reg in an AVX-512 register
   * across passes avoids the store-load forwarding stall that
   * arises when writing and immediately re-reading an N-wide array
   * in successive passes. */
#ifdef __AVX512F__
  __m512 mu = _mm512_set1_ps (mu_J);
  size_t k = 0;
  for (; k + 16 <= N; k += 16)
    {
      __m512 h = _mm512_loadu_ps (&cj[M * N + k]);
      for (int m = (int)M - 1; m >= 0; m--)
        h = _mm512_fmadd_ps (h, mu, _mm512_loadu_ps (&cj[m * N + k]));
      _mm512_storeu_ps (&h_eff[k], h);
    }
  for (; k < N; k++)
    {
      float h = cj[M * N + k];
      for (int m = (int)M - 1; m >= 0; m--)
        h = h * mu_J + cj[m * N + k];
      h_eff[k] = h;
    }
#else
  /* Scalar fallback: sequential passes auto-vectorised by the
   * compiler for non-AVX-512 targets. */
  memcpy (h_eff, &cj[M * N], N * sizeof (float));
  for (int m = (int)M - 1; m >= 0; m--)
    {
      const float *cm = &cj[(size_t)m * N];
      for (size_t k = 0; k < N; k++)
        h_eff[k] = h_eff[k] * mu_J + cm[k];
    }
#endif
}

/* ================================================================== */
/* dot_cf32_f32 — real coefficients × complex signal                 */
/* ================================================================== */

#ifdef __AVX512F__

static const int32_t dup_idx_arr[16]
    = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7 };

static inline void
dot_cf32_f32 (const dp_cf32_t *win, const float *h, size_t N, float *out_i,
              float *out_q)
{
  __m512 acc = _mm512_setzero_ps ();
  __m512i dup_idx = _mm512_loadu_si512 (dup_idx_arr);

  size_t j = 0;
  for (; j + 8 <= N; j += 8)
    {
      __m256 hraw = _mm256_loadu_ps (&h[j]);
      __m512 hb = _mm512_broadcast_f32x8 (hraw);
      __m512 hdup = _mm512_permutexvar_ps (dup_idx, hb);
      __m512 w = _mm512_loadu_ps ((const float *)&win[j]);
      acc = _mm512_fmadd_ps (w, hdup, acc);
    }

  __m256 lo = _mm512_castps512_ps256 (acc);
  __m256 hi = _mm512_extractf32x8_ps (acc, 1);
  __m256 s8 = _mm256_add_ps (lo, hi);
  __m128 s4l = _mm256_castps256_ps128 (s8);
  __m128 s4h = _mm256_extractf128_ps (s8, 1);
  __m128 s4 = _mm_add_ps (s4l, s4h);
  __m128 s2 = _mm_add_ps (s4, _mm_movehl_ps (s4, s4));
  float si = _mm_cvtss_f32 (s2);
  float sq = _mm_cvtss_f32 (_mm_shuffle_ps (s2, s2, 0x1));

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
dot_cf32_f32 (const dp_cf32_t *win, const float *h, size_t N, float *out_i,
              float *out_q)
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

/* ================================================================== */
/* iad_madd — I&D accumulate: iad[k] += x * h[k]                    */
/* ================================================================== */

#ifdef __AVX512F__

static inline void
iad_madd (dp_cf32_t *iad, const float *h, size_t N, float xi_i, float xi_q)
{
  __m256 xp = _mm256_set_ps (xi_q, xi_i, xi_q, xi_i, xi_q, xi_i, xi_q, xi_i);
  __m512 xv = _mm512_broadcast_f32x8 (xp);
  __m512i dup_idx = _mm512_loadu_si512 (dup_idx_arr);

  size_t j = 0;
  for (; j + 8 <= N; j += 8)
    {
      __m256 hraw = _mm256_loadu_ps (&h[j]);
      __m512 hb = _mm512_broadcast_f32x8 (hraw);
      __m512 hdup = _mm512_permutexvar_ps (dup_idx, hb);
      float *p = (float *)&iad[j];
      __m512 av = _mm512_loadu_ps (p);
      av = _mm512_fmadd_ps (xv, hdup, av);
      _mm512_storeu_ps (p, av);
    }
  for (; j < N; j++)
    {
      iad[j].i += xi_i * h[j];
      iad[j].q += xi_q * h[j];
    }
}

#else /* scalar fallback */

static inline void
iad_madd (dp_cf32_t *iad, const float *h, size_t N, float xi_i, float xi_q)
{
  for (size_t j = 0; j < N; j++)
    {
      iad[j].i += xi_i * h[j];
      iad[j].q += xi_q * h[j];
    }
}

#endif /* __AVX512F__ */

/* ================================================================== */
/* Lifecycle                                                          */
/* ================================================================== */

dp_resamp_dpmfs_t *
dp_resamp_dpmfs_create (size_t M, size_t N, const float *c0, const float *c1,
                        double rate)
{
  if (!M || !N || !c0 || !c1 || rate <= 0.0)
    return NULL;
  assert (M <= 3);
  assert (N <= DP_DPMFS_MAX_N);

  dp_resamp_dpmfs_t *r = calloc (1, sizeof *r);
  if (!r)
    return NULL;

  r->rate = rate;
  r->M = M;
  r->N = N;
  r->upsample = (rate >= 1.0);

  size_t bank_len = (M + 1) * N;

  r->c[0] = malloc (bank_len * sizeof (float));
  r->c[1] = malloc (bank_len * sizeof (float));
  if (!r->c[0] || !r->c[1])
    goto fail_coeff;

  if (r->upsample)
    {
      memcpy (r->c[0], c0, bank_len * sizeof (float));
      memcpy (r->c[1], c1, bank_len * sizeof (float));
    }
  else
    {
      /* Decimator: reverse each row [m*N .. (m+1)*N) and scale by
       * rate so the dump loop needs no extra scale factor. */
      float scale = (float)rate;
      for (int ji = 0; ji < 2; ji++)
        {
          const float *src = (ji == 0) ? c0 : c1;
          float *dst = r->c[ji];
          for (size_t m = 0; m <= M; m++)
            {
              const float *sm = &src[m * N];
              float *dm = &dst[m * N];
              for (size_t t = 0; t < N; t++)
                dm[t] = sm[N - 1 - t] * scale;
            }
        }
    }

  /* NCO */
  r->phase = 0;
  r->phase_inc = r->upsample ? norm_to_inc (1.0 / rate) : norm_to_inc (rate);

  /* Delay line (interpolator) */
  r->delay_cap = 1;
  while (r->delay_cap < N)
    r->delay_cap <<= 1;
  r->delay_mask = r->delay_cap - 1;
  r->delay_head = 0;
  r->delay_buf = calloc (2 * r->delay_cap, sizeof (dp_cf32_t));
  if (!r->delay_buf)
    goto fail_delay;

  /* I&D + transposed delay line (decimator).
   * Round iad allocation up to multiple of 8 for AVX-512 safety. */
  size_t iad_n = (N + 7) & ~(size_t)7;
  size_t tfd_n = N > 1 ? N - 1 : 1;
  size_t tfd_alloc = (tfd_n + 7) & ~(size_t)7;
  r->iad = calloc (iad_n, sizeof (dp_cf32_t));
  r->tfd = calloc (tfd_alloc, sizeof (dp_cf32_t));
  if (!r->iad || !r->tfd)
    goto fail_iad;

  return r;

fail_iad:
  free (r->iad);
  free (r->tfd);
  free (r->delay_buf);
fail_delay:
fail_coeff:
  free (r->c[0]);
  free (r->c[1]);
  free (r);
  return NULL;
}

void
dp_resamp_dpmfs_destroy (dp_resamp_dpmfs_t *r)
{
  if (!r)
    return;
  free (r->delay_buf);
  free (r->iad);
  free (r->tfd);
  free (r->c[0]);
  free (r->c[1]);
  free (r);
}

void
dp_resamp_dpmfs_reset (dp_resamp_dpmfs_t *r)
{
  size_t N = r->N;
  size_t iad_n = (N + 7) & ~(size_t)7;
  size_t tfd_n = N > 1 ? N - 1 : 1;
  size_t tfd_alloc = (tfd_n + 7) & ~(size_t)7;

  r->phase = 0;
  r->delay_head = 0;
  memset (r->delay_buf, 0, 2 * r->delay_cap * sizeof (dp_cf32_t));
  memset (r->iad, 0, iad_n * sizeof (dp_cf32_t));
  memset (r->tfd, 0, tfd_alloc * sizeof (dp_cf32_t));
}

/* ================================================================== */
/* Properties                                                         */
/* ================================================================== */

double
dp_resamp_dpmfs_rate (const dp_resamp_dpmfs_t *r)
{
  return r->rate;
}

size_t
dp_resamp_dpmfs_num_taps (const dp_resamp_dpmfs_t *r)
{
  return r->N;
}

size_t
dp_resamp_dpmfs_poly_order (const dp_resamp_dpmfs_t *r)
{
  return r->M;
}

/* ================================================================== */
/* Interpolation (output-driven)                                      */
/* ================================================================== */

static size_t
interp_execute (dp_resamp_dpmfs_t *r, const dp_cf32_t *in, size_t num_in,
                dp_cf32_t *out, size_t max_out)
{
  size_t xi = 0;
  size_t oi = 0;
  size_t N = r->N;
  size_t M = r->M;
  uint32_t ph = r->phase;
  uint32_t inc = r->phase_inc;

  while (xi < num_in && oi < max_out)
    {
      uint32_t new_ph;
      uint8_t ovf = ADD_OVF (ph, inc, &new_ph);

      int j;
      float mu_J;
      phase_to_j_mu (ph, &j, &mu_J);

      /* Vectorised Horner: M AVX-512 FMA passes over N-wide rows,
       * then one dot product.  Total: (M+2)*N real multiplies. */
      float h_eff[DP_DPMFS_MAX_N];
      compute_h_eff (r->c[j], M, N, mu_J, h_eff);

      float oi_val, oq_val;
      dot_cf32_f32 (dl_ptr (r), h_eff, N, &oi_val, &oq_val);
      out[oi].i = oi_val;
      out[oi].q = oq_val;
      oi++;

      ph = new_ph;

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

static size_t
decim_execute (dp_resamp_dpmfs_t *r, const dp_cf32_t *in, size_t num_in,
               dp_cf32_t *out, size_t max_out)
{
  size_t oi = 0;
  size_t N = r->N;
  size_t M = r->M;
  uint32_t ph = r->phase;
  uint32_t inc = r->phase_inc;
  dp_cf32_t *iad = r->iad;
  dp_cf32_t *tfd = r->tfd;

  for (size_t xi = 0; xi < num_in && oi < max_out; xi++)
    {
      uint32_t new_ph;
      uint8_t ovf = ADD_OVF (ph, inc, &new_ph);

      int j;
      float mu_J;
      phase_to_j_mu (ph, &j, &mu_J);

      /* Vectorised Horner → single iad_madd.
       * Coefficients already reversed+scaled at create time. */
      float h_eff[DP_DPMFS_MAX_N];
      compute_h_eff (r->c[j], M, N, mu_J, h_eff);
      iad_madd (iad, h_eff, N, in[xi].i, in[xi].q);

      ph = new_ph;

      if (ovf)
        {
          /* Transposed delay line: shift and add */
          float yout_i = iad[0].i + tfd[0].i;
          float yout_q = iad[0].q + tfd[0].q;

#ifdef __AVX512F__
          size_t k = 0;
          for (; k + 2 + 8 <= N; k += 8)
            {
              __m512 a = _mm512_loadu_ps ((const float *)&iad[k + 1]);
              __m512 b = _mm512_loadu_ps ((const float *)&tfd[k + 1]);
              _mm512_storeu_ps ((float *)&tfd[k], _mm512_add_ps (a, b));
            }
          for (; k + 2 < N; k++)
            {
              tfd[k].i = iad[k + 1].i + tfd[k + 1].i;
              tfd[k].q = iad[k + 1].q + tfd[k + 1].q;
            }
#else
          for (size_t k = 0; k + 2 < N; k++)
            {
              tfd[k].i = iad[k + 1].i + tfd[k + 1].i;
              tfd[k].q = iad[k + 1].q + tfd[k + 1].q;
            }
#endif
          tfd[N - 2].i = iad[N - 1].i;
          tfd[N - 2].q = iad[N - 1].q;

          out[oi].i = yout_i;
          out[oi].q = yout_q;
          oi++;

          memset (iad, 0, N * sizeof (dp_cf32_t));
        }
    }

  r->phase = ph;
  return oi;
}

/* ================================================================== */
/* Public execute                                                     */
/* ================================================================== */

size_t
dp_resamp_dpmfs_execute (dp_resamp_dpmfs_t *r, const dp_cf32_t *in,
                         size_t num_in, dp_cf32_t *out, size_t max_out)
{
  if (r->upsample)
    return interp_execute (r, in, num_in, out, max_out);
  else
    return decim_execute (r, in, num_in, out, max_out);
}
