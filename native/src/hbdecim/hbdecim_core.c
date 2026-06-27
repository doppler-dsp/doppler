/**
 * @file hbdecim_core.c
 * @brief Halfband 2:1 decimator — CF32 implementation.
 *
 * Lifted directly from c/src/hbdecim.c (dp_hbdecim_cf32_t); only
 * the CF32 variant is included here.  No logic changes; rename only.
 * See hbdecim_core.h for the algorithm description.
 */
#include "hbdecim/hbdecim_core.h"

#include <string.h>

/* ================================================================== */
/* Delay-line helpers (dual-write circular buffers)                   */
/* ================================================================== */

static inline void
dl_push_even (hbdecim_state_t *r, float _Complex x)
{
  r->even_head                            = (r->even_head - 1) & r->even_mask;
  r->even_buf[r->even_head]               = x;
  r->even_buf[r->even_head + r->even_cap] = x;
}

static inline void
dl_push_odd (hbdecim_state_t *r, float _Complex x)
{
  r->odd_head                           = (r->odd_head - 1) & r->even_mask;
  r->odd_buf[r->odd_head]               = x;
  r->odd_buf[r->odd_head + r->even_cap] = x;
}

/* ================================================================== */
/* Per-output sample: symmetric FIR + pure-delay branch               */
/* ================================================================== */

static inline float _Complex compute_output (const hbdecim_state_t *r)
{
  const float *h    = r->h;
  size_t       N    = r->num_taps;
  size_t       half = N / 2;
  float        si = 0.0f, sq = 0.0f;

  if (r->fir_on_even)
    {
      /* N even: FIR on even_dl (symmetric pairs, no centre tap).
       * Delay: 0.5 × odd_dl[centre].  The 0.5 factor is baked into
       * h[] at create time for the FIR branch; the delay tap
       * carries the explicit 0.5 from the polyphase identity.      */
      const float _Complex *e = &r->even_buf[r->even_head];
      for (size_t k = 0; k < half; k++)
        {
          float hk = h[k];
          si += hk * (crealf (e[k]) + crealf (e[N - 1 - k]));
          sq += hk * (cimagf (e[k]) + cimagf (e[N - 1 - k]));
        }
      const float _Complex *o = &r->odd_buf[r->odd_head];
      si += 0.5f * crealf (o[r->centre]);
      sq += 0.5f * cimagf (o[r->centre]);
    }
  else
    {
      /* N odd: FIR on odd_dl at offset +1 (polyphase identity:
       * x[2m-2k-1] = odd_dl[k+1] after the push).  Delay from
       * even_dl[centre].  See hbdecim.c for the derivation.       */
      const float _Complex *o = &r->odd_buf[r->odd_head];
      for (size_t k = 0; k < half; k++)
        {
          float hk = h[k];
          si += hk * (crealf (o[k + 1]) + crealf (o[N - 1 - k]));
          sq += hk * (cimagf (o[k + 1]) + cimagf (o[N - 1 - k]));
        }
      const float _Complex *e = &r->even_buf[r->even_head];
      si += 0.5f * crealf (e[r->centre]);
      sq += 0.5f * cimagf (e[r->centre]);
    }

  return CMPLXF (si, sq);
}

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

hbdecim_state_t *
hbdecim_create (size_t num_taps, const float *h)
{
  if (!num_taps || !h)
    return NULL;

  hbdecim_state_t *r = calloc (1, sizeof (*r));
  if (!r)
    return NULL;

  r->num_taps    = num_taps;
  r->centre      = num_taps / 2;
  r->fir_on_even = !(num_taps & 1); /* even N → FIR on even inputs */

  r->h = malloc (num_taps * sizeof (float));
  if (!r->h)
    goto fail;

  /* Scale by 0.5 (rate = 0.5) to compensate for the ×phases factor
   * baked in by kaiser_prototype; mirrors dp_hbdecim_cf32_create.  */
  for (size_t k = 0; k < num_taps; k++)
    r->h[k] = h[k] * 0.5f;

  /* Dual-write ring: next power of 2 >= num_taps */
  r->even_cap = 1;
  while (r->even_cap < num_taps)
    r->even_cap <<= 1;
  r->even_mask = r->even_cap - 1;

  r->even_buf = calloc (2 * r->even_cap, sizeof (float _Complex));
  r->odd_buf  = calloc (2 * r->even_cap, sizeof (float _Complex));
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
hbdecim_destroy (hbdecim_state_t *r)
{
  if (!r)
    return;
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
}

void
hbdecim_reset (hbdecim_state_t *r)
{
  r->even_head   = 0;
  r->odd_head    = 0;
  r->has_pending = 0;
  memset (r->even_buf, 0, 2 * r->even_cap * sizeof (float _Complex));
  memset (r->odd_buf, 0, 2 * r->even_cap * sizeof (float _Complex));
}

/* ── Serializable state ─────────────────────────────────────────────────────
 * Order: even_head, odd_head, has_pending, pending, then the two dual-write
 * delay rings (2*even_cap cf32 each). */

size_t
hbdecim_state_bytes (const hbdecim_state_t *r)
{
  return 2 * sizeof (size_t) + sizeof (int) + sizeof (float _Complex)
         + 2 * (2 * r->even_cap * sizeof (float _Complex));
}

void
hbdecim_get_state (const hbdecim_state_t *r, void *blob)
{
  char        *p  = (char *)blob;
  const size_t bb = 2 * r->even_cap * sizeof (float _Complex);
  memcpy (p, &r->even_head, sizeof (size_t)), p += sizeof (size_t);
  memcpy (p, &r->odd_head, sizeof (size_t)), p += sizeof (size_t);
  memcpy (p, &r->has_pending, sizeof (int)), p += sizeof (int);
  memcpy (p, &r->pending, sizeof (float _Complex)),
      p += sizeof (float _Complex);
  memcpy (p, r->even_buf, bb), p += bb;
  memcpy (p, r->odd_buf, bb);
}

int
hbdecim_set_state (hbdecim_state_t *r, const void *blob)
{
  const char  *p  = (const char *)blob;
  const size_t bb = 2 * r->even_cap * sizeof (float _Complex);
  memcpy (&r->even_head, p, sizeof (size_t)), p += sizeof (size_t);
  memcpy (&r->odd_head, p, sizeof (size_t)), p += sizeof (size_t);
  memcpy (&r->has_pending, p, sizeof (int)), p += sizeof (int);
  memcpy (&r->pending, p, sizeof (float _Complex)),
      p += sizeof (float _Complex);
  memcpy (r->even_buf, p, bb), p += bb;
  memcpy (r->odd_buf, p, bb);
  return 0;
}

/* ================================================================== */
/* Properties                                                          */
/* ================================================================== */

double
hbdecim_get_rate (const hbdecim_state_t *r)
{
  (void)r;
  return 0.5;
}

size_t
hbdecim_get_num_taps (const hbdecim_state_t *r)
{
  return r->num_taps;
}

/* ================================================================== */
/* Execute                                                             */
/* ================================================================== */

size_t
hbdecim_execute (hbdecim_state_t *r, const float _Complex *in, size_t num_in,
                 float _Complex *out, size_t max_out)
{
  if (!num_in || !max_out)
    return 0;

  size_t oi = 0;
  size_t xi = 0;

  /* Complete a pending even sample with the first odd sample of
   * this block, then resume normal pair processing.               */
  if (r->has_pending && oi < max_out)
    {
      dl_push_even (r, r->pending);
      dl_push_odd (r, in[xi++]);
      out[oi++]      = compute_output (r);
      r->has_pending = 0;
    }

  /* Process complete (even, odd) pairs */
  while (xi + 1 < num_in && oi < max_out)
    {
      dl_push_even (r, in[xi]);
      dl_push_odd (r, in[xi + 1]);
      xi += 2;
      out[oi++] = compute_output (r);
    }

  /* Buffer any dangling even sample for the next call */
  if (xi < num_in)
    {
      r->pending     = in[xi];
      r->has_pending = 1;
    }

  return oi;
}
