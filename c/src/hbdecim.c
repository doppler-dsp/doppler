/**
 * @file hbdecim.c
 * @brief Halfband 2:1 decimator for cf32 IQ samples.
 *
 * Two delay lines track even-indexed and odd-indexed input samples
 * separately.  Per output sample:
 *
 *   1. Push x[2m]   → even delay line  (e[k] = x[2m-2k]).
 *   2. Push x[2m+1] → odd delay line   (o[k] = x[2m+1-2k]).
 *
 * Branch assignment depends on N (FIR branch length from kaiser_prototype):
 *
 *   N even (fir_on_even=1):
 *     FIR: y_fir = Σ h[k]*(e[k]+e[N-1-k]),  k=0..N/2-1  (no centre)
 *     Delay: y_d = 0.5 * o[N/2]
 *
 *   N odd (fir_on_even=0):
 *     FIR: y_fir = Σ h[k]*(o[k+1]+o[N-1-k]),  k=0..N/2-1  (no centre)
 *     Delay: y_d = 0.5 * e[N/2]
 *
 * The +1 offset in the N-odd FIR arises from the polyphase identity
 * x[2m-2k-1] = o[k+1].  The last coefficient h[N-1] is a zero-pad
 * from kaiser_prototype, so the effective symmetry is h[k]=h[N-2-k]
 * and the N/2 symmetric pairs cover all N-1 nonzero taps with no
 * centre tap.
 *
 * Both delay lines use the dual-write circular buffer trick so the
 * inner loop always reads a contiguous window without branch logic.
 */

#include "dp/hbdecim.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/* Internal struct                                                    */
/* ================================================================== */

struct dp_hbdecim_cf32
{
  size_t num_taps; /* FIR branch length        */
  size_t centre;   /* = num_taps / 2           */
  int fir_on_even; /* 1 if N even, 0 if N odd  */
  float *h;        /* FIR coefficients, length num_taps */

  /* Even delay line — stores x[2m], x[2m-2], x[2m-4], ...
   * Dual circular buffer: cap = next power of 2 >= num_taps.   */
  dp_cf32_t *even_buf;
  size_t even_cap;
  size_t even_mask;
  size_t even_head;

  /* Odd delay line — stores x[2m+1], x[2m-1], x[2m-3], ...   */
  dp_cf32_t *odd_buf;
  size_t odd_head;

  /* Pending even sample when the last call had an odd input count. */
  int has_pending;
  dp_cf32_t pending;
};

/* ================================================================== */
/* Delay-line helpers                                                 */
/* ================================================================== */

static inline void
dl_push_even (struct dp_hbdecim_cf32 *r, dp_cf32_t x)
{
  r->even_head = (r->even_head - 1) & r->even_mask;
  r->even_buf[r->even_head] = x;
  r->even_buf[r->even_head + r->even_cap] = x;
}

static inline void
dl_push_odd (struct dp_hbdecim_cf32 *r, dp_cf32_t x)
{
  r->odd_head = (r->odd_head - 1) & r->even_mask;
  r->odd_buf[r->odd_head] = x;
  r->odd_buf[r->odd_head + r->even_cap] = x;
}

/* ================================================================== */
/* Symmetric FIR + pure-delay computation                            */
/* ================================================================== */

static inline dp_cf32_t
compute_output (const struct dp_hbdecim_cf32 *r)
{
  const float *h = r->h;
  size_t N = r->num_taps;
  size_t half = N / 2;
  float si = 0.0f, sq = 0.0f;

  if (r->fir_on_even)
    {
      /* N even: FIR processes even_dl; delay from odd_dl[centre].
       * Symmetric pairs only — no centre tap for even-length FIR.    */
      const dp_cf32_t *e = &r->even_buf[r->even_head];
      for (size_t k = 0; k < half; k++)
        {
          float hk = h[k];
          si += hk * (e[k].i + e[N - 1 - k].i);
          sq += hk * (e[k].q + e[N - 1 - k].q);
        }
      const dp_cf32_t *o = &r->odd_buf[r->odd_head];
      si += 0.5f * o[r->centre].i;
      sq += 0.5f * o[r->centre].q;
    }
  else
    {
      /* N odd: FIR processes odd_dl at offset +1; delay from even_dl.
       *
       * Polyphase decomposition: y[m] = Σ h_e[k]*x[2m-2k]
       *                               + Σ h_o[k]*x[2m-2k-1]
       * With h_o=bank[1]/2 and odd_dl[j]=x[2m+1-2j] after push:
       *   x[2m-2k-1] = x[2m+1-2(k+1)] = odd_dl[k+1]
       * So the FIR sum is Σ h_o[k] * odd_dl[k+1], k=0..N-2.
       *
       * Symmetry of bank[1]: h_o[k]=h_o[N-2-k] (the last tap is a
       * zero-pad from kaiser_prototype).  Symmetric pairs:
       *   h_o[k]*(odd_dl[k+1] + odd_dl[N-1-k]),  k=0..half-1.
       * No centre tap (N-1 effective taps is even for N odd).          */
      const dp_cf32_t *o = &r->odd_buf[r->odd_head];
      for (size_t k = 0; k < half; k++)
        {
          float hk = h[k];
          si += hk * (o[k + 1].i + o[N - 1 - k].i);
          sq += hk * (o[k + 1].q + o[N - 1 - k].q);
        }
      const dp_cf32_t *e = &r->even_buf[r->even_head];
      si += 0.5f * e[r->centre].i;
      sq += 0.5f * e[r->centre].q;
    }

  return (dp_cf32_t){ si, sq };
}

/* ================================================================== */
/* Lifecycle                                                          */
/* ================================================================== */

dp_hbdecim_cf32_t *
dp_hbdecim_cf32_create (size_t num_taps, const float *h)
{
  if (!num_taps || !h)
    return NULL;

  dp_hbdecim_cf32_t *r = calloc (1, sizeof *r);
  if (!r)
    return NULL;

  r->num_taps = num_taps;
  r->centre = num_taps / 2;
  r->fir_on_even = !(num_taps & 1); /* even N → FIR on even inputs */

  r->h = malloc (num_taps * sizeof (float));
  if (!r->h)
    goto fail;
  /* Scale by 0.5 (rate = 0.5) to compensate for the ×phases factor
   * applied by kaiser_prototype; mirrors what dp_resamp_cf32_create
   * does for decimation (scale = (float)rate).                       */
  for (size_t k = 0; k < num_taps; k++)
    r->h[k] = h[k] * 0.5f;

  /* Dual-write circular buffer: cap = next power of 2 >= num_taps */
  r->even_cap = 1;
  while (r->even_cap < num_taps)
    r->even_cap <<= 1;
  r->even_mask = r->even_cap - 1;

  r->even_buf = calloc (2 * r->even_cap, sizeof (dp_cf32_t));
  r->odd_buf = calloc (2 * r->even_cap, sizeof (dp_cf32_t));
  if (!r->even_buf || !r->odd_buf)
    goto fail;

  return r;

fail:
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
  return NULL;
}

void
dp_hbdecim_cf32_destroy (dp_hbdecim_cf32_t *r)
{
  if (!r)
    return;
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
}

void
dp_hbdecim_cf32_reset (dp_hbdecim_cf32_t *r)
{
  r->even_head = 0;
  r->odd_head = 0;
  r->has_pending = 0;
  memset (r->even_buf, 0, 2 * r->even_cap * sizeof (dp_cf32_t));
  memset (r->odd_buf, 0, 2 * r->even_cap * sizeof (dp_cf32_t));
}

/* ================================================================== */
/* Properties                                                         */
/* ================================================================== */

double
dp_hbdecim_cf32_rate (const dp_hbdecim_cf32_t *r)
{
  (void)r;
  return 0.5;
}

size_t
dp_hbdecim_cf32_num_taps (const dp_hbdecim_cf32_t *r)
{
  return r->num_taps;
}

/* ================================================================== */
/* Execute                                                            */
/* ================================================================== */

size_t
dp_hbdecim_cf32_execute (dp_hbdecim_cf32_t *r, const dp_cf32_t *in,
                         size_t num_in, dp_cf32_t *out, size_t max_out)
{
  if (!num_in || !max_out)
    return 0;

  size_t oi = 0;
  size_t xi = 0;

  /* Complete a pending even sample with the first odd sample of this
   * block, then resume normal pair processing.                       */
  if (r->has_pending && oi < max_out)
    {
      dl_push_even (r, r->pending);
      dl_push_odd (r, in[xi++]);
      out[oi++] = compute_output (r);
      r->has_pending = 0;
    }

  /* Process complete pairs (even, odd) */
  while (xi + 1 < num_in && oi < max_out)
    {
      dl_push_even (r, in[xi]);
      dl_push_odd (r, in[xi + 1]);
      xi += 2;
      out[oi++] = compute_output (r);
    }

  /* Save dangling even sample for the next call */
  if (xi < num_in)
    {
      r->pending = in[xi];
      r->has_pending = 1;
    }

  return oi;
}

/* ================================================================== */
/* Architecture D2 — real-to-complex halfband with embedded fs/4 mix */
/* ================================================================== */

/*
 * D2 real-input halfband: mixes by e^{-j(π/2)n} (fs/4 shift) and
 * decimates by 2 in a single pass with zero extra multiplications.
 *
 * The per-sample rotation e^{j(π/2)k} is absorbed into the FIR taps
 * at construction (h_mod[k] = h_fir[k] * (-1)^k * 0.5f).  The
 * output-domain correction e^{-jπm} = (-1)^m is a sign toggle.
 *
 * Two cases arise depending on which polyphase branch is the FIR:
 *
 *   fir_on_even=1 (N even):
 *     FIR  on even_dl  → I channel  (antisymmetric: differences)
 *     Centre from odd_dl[centre]  → Q channel  (delay_sign = ±1)
 *
 *   fir_on_even=0 (N odd):
 *     FIR  on odd_dl   → Q channel  (antisymmetric: differences)
 *     Centre from even_dl[centre] → I channel  (delay_sign = ±1)
 *
 * In both cases h_mod is antisymmetric (h_mod[k] = −h_mod[eff−1−k]
 * where eff = effective FIR length), so paired terms subtract.
 */

struct dp_hbdecim_r2cf32
{
  size_t num_taps;  /* FIR branch length                              */
  size_t centre;    /* = num_taps / 2                                 */
  int fir_on_even;  /* 1 if N even, 0 if N odd                        */
  float delay_sign; /* ±1: sign applied to the delay-branch output    */
  float *h;         /* h_fir[k]*(-1)^k*0.5, length num_taps           */

  /* Real delay lines (float, not dp_cf32_t) */
  float *even_buf; /* x[2m], x[2m-2], ...                              */
  size_t even_cap;
  size_t even_mask;
  size_t even_head;

  float *odd_buf; /* x[2m+1], x[2m-1], ...                             */
  size_t odd_head;

  int has_pending;
  float pending;
  int parity; /* 0 → output sign +1;  1 → output sign -1  ((-1)^m)  */
};

/* ------------------------------------------------------------------ */
/* Delay-line helpers (real)                                          */
/* ------------------------------------------------------------------ */

static inline void
r2_push_even (struct dp_hbdecim_r2cf32 *r, float x)
{
  r->even_head = (r->even_head - 1) & r->even_mask;
  r->even_buf[r->even_head] = x;
  r->even_buf[r->even_head + r->even_cap] = x;
}

static inline void
r2_push_odd (struct dp_hbdecim_r2cf32 *r, float x)
{
  r->odd_head = (r->odd_head - 1) & r->even_mask;
  r->odd_buf[r->odd_head] = x;
  r->odd_buf[r->odd_head + r->even_cap] = x;
}

/* ------------------------------------------------------------------ */
/* Per-output I/Q computation                                         */
/* ------------------------------------------------------------------ */

static inline dp_cf32_t
r2_compute_output (const struct dp_hbdecim_r2cf32 *r)
{
  const float *h = r->h;
  size_t N = r->num_taps;
  size_t half = N / 2;
  float ri = 0.0f, rq = 0.0f;

  if (r->fir_on_even)
    {
      /* FIR on even_dl → I channel (antisymmetric pairs: subtract)
       * Delay from odd_dl[centre]  → Q channel                    */
      const float *e = &r->even_buf[r->even_head];
      for (size_t k = 0; k < half; k++)
        ri += h[k] * (e[k] - e[N - 1 - k]);

      const float *o = &r->odd_buf[r->odd_head];
      rq = r->delay_sign * 0.5f * o[r->centre];
    }
  else
    {
      /* FIR on odd_dl  → Q channel (antisymmetric pairs: subtract)
       * Delay from even_dl[centre] → I channel                    */
      const float *o = &r->odd_buf[r->odd_head];
      for (size_t k = 0; k < half; k++)
        rq += h[k] * (o[k + 1] - o[N - 1 - k]);

      const float *e = &r->even_buf[r->even_head];
      ri = r->delay_sign * 0.5f * e[r->centre];
    }

  /* Apply output correction (-1)^m */
  if (r->parity)
    {
      ri = -ri;
      rq = -rq;
    }
  return (dp_cf32_t){ ri, rq };
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                          */
/* ------------------------------------------------------------------ */

dp_hbdecim_r2cf32_t *
dp_hbdecim_r2cf32_create (size_t num_taps, const float *h)
{
  if (!num_taps || !h)
    return NULL;

  dp_hbdecim_r2cf32_t *r = calloc (1, sizeof *r);
  if (!r)
    return NULL;

  r->num_taps = num_taps;
  r->centre = num_taps / 2;
  r->fir_on_even = !(num_taps & 1); /* even N → FIR on even inputs */

  /* Compute delay_sign from the prototype centre tap position.
   *
   * For fir_on_even=1 (N even):  centre prototype index = N-1 (odd).
   *   Rotation: e^{j(π/2)(N-1)} = j^{N-1}.
   *   Im(j^{N-1}) = +1 when (N-1)≡1 (mod 4), i.e. N%4==2.
   *               = -1 when (N-1)≡3 (mod 4), i.e. N%4==0.
   *
   * For fir_on_even=0 (N odd):   centre prototype index = N-1 (even).
   *   Rotation: e^{j(π/2)(N-1)} = (-1)^{(N-1)/2}.
   */
  if (r->fir_on_even)
    r->delay_sign = ((num_taps & 3) == 2) ? 1.0f : -1.0f;
  else
    r->delay_sign = (((num_taps - 1) / 2) & 1) ? -1.0f : 1.0f;

  r->h = malloc (num_taps * sizeof (float));
  if (!r->h)
    goto fail;

  /* Bake in alternating sign flip and decimation scale (0.5).
   * h_mod[k] = h_fir[k] * (-1)^k * 0.5                           */
  for (size_t k = 0; k < num_taps; k++)
    r->h[k] = h[k] * ((k & 1) ? -0.5f : 0.5f);

  /* Dual-write circular buffers (real) */
  r->even_cap = 1;
  while (r->even_cap < num_taps)
    r->even_cap <<= 1;
  r->even_mask = r->even_cap - 1;

  r->even_buf = calloc (2 * r->even_cap, sizeof (float));
  r->odd_buf = calloc (2 * r->even_cap, sizeof (float));
  if (!r->even_buf || !r->odd_buf)
    goto fail;

  return r;

fail:
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
  return NULL;
}

void
dp_hbdecim_r2cf32_destroy (dp_hbdecim_r2cf32_t *r)
{
  if (!r)
    return;
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
}

void
dp_hbdecim_r2cf32_reset (dp_hbdecim_r2cf32_t *r)
{
  r->even_head = 0;
  r->odd_head = 0;
  r->has_pending = 0;
  r->parity = 0;
  memset (r->even_buf, 0, 2 * r->even_cap * sizeof (float));
  memset (r->odd_buf, 0, 2 * r->even_cap * sizeof (float));
}

/* ------------------------------------------------------------------ */
/* Properties                                                         */
/* ------------------------------------------------------------------ */

double
dp_hbdecim_r2cf32_rate (const dp_hbdecim_r2cf32_t *r)
{
  (void)r;
  return 0.5;
}

size_t
dp_hbdecim_r2cf32_num_taps (const dp_hbdecim_r2cf32_t *r)
{
  return r->num_taps;
}

/* ------------------------------------------------------------------ */
/* Execute                                                            */
/* ------------------------------------------------------------------ */

size_t
dp_hbdecim_r2cf32_execute (dp_hbdecim_r2cf32_t *r, const float *in,
                           size_t num_in, dp_cf32_t *out, size_t max_out)
{
  if (!num_in || !max_out)
    return 0;

  size_t oi = 0;
  size_t xi = 0;

  /* Complete a pending even sample with the first odd sample */
  if (r->has_pending && oi < max_out)
    {
      r2_push_even (r, r->pending);
      r2_push_odd (r, in[xi++]);
      out[oi++] = r2_compute_output (r);
      r->parity ^= 1;
      r->has_pending = 0;
    }

  /* Process complete pairs (even, odd) */
  while (xi + 1 < num_in && oi < max_out)
    {
      r2_push_even (r, in[xi]);
      r2_push_odd (r, in[xi + 1]);
      xi += 2;
      out[oi++] = r2_compute_output (r);
      r->parity ^= 1;
    }

  /* Save dangling even sample */
  if (xi < num_in)
    {
      r->pending = in[xi];
      r->has_pending = 1;
    }

  return oi;
}
