/**
 * @file hbdecim_r2c_core.c
 * @brief Real-to-complex halfband 2:1 decimator (Architecture D2).
 *
 * Lifted from c/src/hbdecim.c (dp_hbdecim_r2cf32_t section).
 *
 * Algorithm:
 *   Input is split into even (x[2m]) and odd (x[2m+1]) streams, each
 *   stored in its own dual-write circular delay line.  One output per
 *   pair:
 *
 *     N even (fir_on_even=1):
 *       I = antisymmetric FIR on even stream
 *       Q = delay_sign × 0.5 × odd[centre]
 *
 *     N odd (fir_on_even=0):
 *       Q = antisymmetric FIR on odd stream (at offset +1)
 *       I = delay_sign × 0.5 × even[centre]
 *
 *   Output: y[m] = (I + jQ) × (-1)^m  (parity flip provides fs/4 shift)
 *
 * Coefficients are baked as h_mod[k] = h_fir[k] × (-1)^k × 0.5.
 */

#include "hbdecim/hbdecim_r2c_core.h"

#include <stdlib.h>
#include <string.h>

struct hbdecim_r2c_state
{
  size_t num_taps;
  size_t centre;
  int    fir_on_even;
  float  delay_sign;
  float *h;

  float *even_buf;
  size_t even_cap;
  size_t even_mask;
  size_t even_head;

  float *odd_buf;
  size_t odd_head;

  int   has_pending;
  float pending;
  int   parity;
};

static inline void
r2_push_even (struct hbdecim_r2c_state *r, float x)
{
  r->even_head                            = (r->even_head - 1) & r->even_mask;
  r->even_buf[r->even_head]               = x;
  r->even_buf[r->even_head + r->even_cap] = x;
}

static inline void
r2_push_odd (struct hbdecim_r2c_state *r, float x)
{
  r->odd_head                           = (r->odd_head - 1) & r->even_mask;
  r->odd_buf[r->odd_head]               = x;
  r->odd_buf[r->odd_head + r->even_cap] = x;
}

static inline float _Complex r2_compute_output (
    const struct hbdecim_r2c_state *r)
{
  const float *h    = r->h;
  size_t       N    = r->num_taps;
  size_t       half = N / 2;
  float        ri = 0.0f, rq = 0.0f;

  if (r->fir_on_even)
    {
      const float *e = &r->even_buf[r->even_head];
      for (size_t k = 0; k < half; k++)
        ri += h[k] * (e[k] - e[N - 1 - k]);
      const float *o = &r->odd_buf[r->odd_head];
      rq             = r->delay_sign * 0.5f * o[r->centre];
    }
  else
    {
      const float *o = &r->odd_buf[r->odd_head];
      for (size_t k = 0; k < half; k++)
        rq += h[k] * (o[k + 1] - o[N - 1 - k]);
      const float *e = &r->even_buf[r->even_head];
      ri             = r->delay_sign * 0.5f * e[r->centre];
    }

  if (r->parity)
    {
      ri = -ri;
      rq = -rq;
    }
  return CMPLXF (ri, rq);
}

hbdecim_r2c_state_t *
hbdecim_r2c_create (size_t num_taps, const float *h)
{
  if (!num_taps || !h)
    return NULL;

  hbdecim_r2c_state_t *r = calloc (1, sizeof *r);
  if (!r)
    return NULL;

  r->num_taps    = num_taps;
  r->centre      = num_taps / 2;
  r->fir_on_even = !(num_taps & 1);

  if (r->fir_on_even)
    r->delay_sign = ((num_taps & 3) == 2) ? 1.0f : -1.0f;
  else
    r->delay_sign = (((num_taps - 1) / 2) & 1) ? -1.0f : 1.0f;

  r->h = malloc (num_taps * sizeof (float));
  if (!r->h)
    goto fail;

  for (size_t k = 0; k < num_taps; k++)
    r->h[k] = h[k] * ((k & 1) ? -0.5f : 0.5f);

  r->even_cap = 1;
  while (r->even_cap < num_taps)
    r->even_cap <<= 1;
  r->even_mask = r->even_cap - 1;

  r->even_buf = calloc (2 * r->even_cap, sizeof (float));
  r->odd_buf  = calloc (2 * r->even_cap, sizeof (float));
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
hbdecim_r2c_destroy (hbdecim_r2c_state_t *r)
{
  if (!r)
    return;
  free (r->h);
  free (r->even_buf);
  free (r->odd_buf);
  free (r);
}

void
hbdecim_r2c_reset (hbdecim_r2c_state_t *r)
{
  r->even_head   = 0;
  r->odd_head    = 0;
  r->has_pending = 0;
  r->parity      = 0;
  memset (r->even_buf, 0, 2 * r->even_cap * sizeof (float));
  memset (r->odd_buf, 0, 2 * r->even_cap * sizeof (float));
}

double
hbdecim_r2c_get_rate (const hbdecim_r2c_state_t *r)
{
  (void)r;
  return 0.5;
}

size_t
hbdecim_r2c_get_num_taps (const hbdecim_r2c_state_t *r)
{
  return r->num_taps;
}

size_t
hbdecim_r2c_execute (hbdecim_r2c_state_t *r, const float *in, size_t num_in,
                     float _Complex *out, size_t max_out)
{
  if (!num_in || !max_out)
    return 0;

  size_t oi = 0, xi = 0;

  if (r->has_pending && oi < max_out)
    {
      r2_push_even (r, r->pending);
      r2_push_odd (r, in[xi++]);
      out[oi++] = r2_compute_output (r);
      r->parity ^= 1;
      r->has_pending = 0;
    }

  while (xi + 1 < num_in && oi < max_out)
    {
      r2_push_even (r, in[xi]);
      r2_push_odd (r, in[xi + 1]);
      xi += 2;
      out[oi++] = r2_compute_output (r);
      r->parity ^= 1;
    }

  if (xi < num_in)
    {
      r->pending     = in[xi];
      r->has_pending = 1;
    }

  return oi;
}
