#include "doppler/timing_est/timing_est_core.h"

#include <math.h>

dp_timing_est_state_t *
dp_timing_est_create (double sps)
{
  /* The symbol-rate line of the power sits at 1/sps cycles a sample, so it
     is below Nyquist only for sps > 2. NaN fails the comparison. */
  if (!(sps > 2.0) || !isfinite (sps))
    return NULL;
  dp_timing_est_state_t *obj = dp_callocn (1, 1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->sps = sps;
  obj->inc = nco_norm_freq_to_inc (1.0 / sps);
  return obj;
}

void
dp_timing_est_destroy (dp_timing_est_state_t *state)
{
  free (state);
}

void
dp_timing_est_reset (dp_timing_est_state_t *state)
{
  double   sps = state->sps;
  uint32_t inc = state->inc;
  memset (state, 0, sizeof *state);
  state->sps = sps;
  state->inc = inc;
}

/* Fold a difference of two timings, each in (-1/2, 1/2], to [-1/2, 1/2). */
static double
wrap_half (double d)
{
  return d - floor (d + 0.5);
}

dp_timing_estimate_t
dp_timing_est_finish (dp_timing_est_state_t *state)
{
  dp_timing_estimate_t r = { 0 };
  if (state->blk_n == 0)
    return r;
  double n  = (double)state->blk_n;
  r.symbols = n / state->sps;
  r.power   = state->pwr / n;
  if (state->pwr > 0.0)
    {
      /* The power is A + B cos (2 pi (phi - tau)), so c carries
         exp (-j 2 pi tau): the eye center's clock phase is -arg c. */
      r.tau     = -atan2 (state->c_im, state->c_re) / 6.283185307179586476925;
      r.quality = hypot (state->c_re, state->c_im) / state->pwr;
      /* The block's middle on the clock, in symbols since create or reset:
         the estimate is the timing there when the timing is moving. */
      double mid = ((double)state->blk_first + 0.5 * n) / state->sps;
      if (state->have_prev && mid > state->prev_mid)
        {
          r.rate
              = wrap_half (r.tau - state->prev_tau) / (mid - state->prev_mid);
          r.has_rate = 1;
        }
      state->have_prev = 1;
      state->prev_tau  = r.tau;
      state->prev_mid  = mid;
    }
  state->c_re  = 0.0;
  state->c_im  = 0.0;
  state->pwr   = 0.0;
  state->blk_n = 0;
  return r;
}

dp_timing_estimate_t
dp_timing_est_estimate (dp_timing_est_state_t *state, const float _Complex *in,
                        size_t n_in)
{
  for (size_t i = 0; i < n_in; i++)
    {
      double re = (double)crealf (in[i]), im = (double)cimagf (in[i]);
      dp_timing_est_accumulate (state, re * re + im * im);
    }
  return dp_timing_est_finish (state);
}

/* ── Serializable state — standard envelope (see dp_state.h) ─────────────────
 * Order: phase, have_prev, count, blk_n, blk_first, then c_re, c_im, pwr,
 * prev_tau, prev_mid. Config (sps, inc) is create()'s and is not in it. */

size_t
dp_timing_est_state_bytes (const dp_timing_est_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + 2 * sizeof (uint32_t)
         + 3 * sizeof (uint64_t) + 5 * sizeof (double);
}

void
dp_timing_est_get_state (const dp_timing_est_state_t *state, void *blob)
{
  size_t      total = dp_timing_est_state_bytes (state);
  dp_writer_t w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, TIMING_EST_STATE_MAGIC, TIMING_EST_STATE_VERSION, total);
  dp_w_u32 (&w, state->phase);
  dp_w_u32 (&w, (uint32_t)state->have_prev);
  dp_w_u64 (&w, state->count);
  dp_w_u64 (&w, state->blk_n);
  dp_w_u64 (&w, state->blk_first);
  dp_w_f64 (&w, state->c_re);
  dp_w_f64 (&w, state->c_im);
  dp_w_f64 (&w, state->pwr);
  dp_w_f64 (&w, state->prev_tau);
  dp_w_f64 (&w, state->prev_mid);
}

int
dp_timing_est_set_state (dp_timing_est_state_t *state, const void *blob)
{
  size_t total = dp_timing_est_state_bytes (state);
  int    rc    = dp_state_validate (blob, total, TIMING_EST_STATE_MAGIC,
                                    TIMING_EST_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, total);
  r.off         = sizeof (dp_state_hdr_t);
  /* Decode whole, check whole, then commit: the envelope reads no payload
     field, so a forged one keeps the size and has to be refused here. */
  dp_timing_est_state_t t = *state;
  t.phase                 = dp_r_u32 (&r);
  uint32_t have           = dp_r_u32 (&r);
  t.count                 = dp_r_u64 (&r);
  t.blk_n                 = dp_r_u64 (&r);
  t.blk_first             = dp_r_u64 (&r);
  t.c_re                  = dp_r_f64 (&r);
  t.c_im                  = dp_r_f64 (&r);
  t.pwr                   = dp_r_f64 (&r);
  t.prev_tau              = dp_r_f64 (&r);
  t.prev_mid              = dp_r_f64 (&r);
  t.have_prev             = (int)have;
  if (have > 1u || t.blk_n > t.count || t.blk_first > t.count - t.blk_n
      || !isfinite (t.c_re) || !isfinite (t.c_im) || !isfinite (t.pwr)
      || t.pwr < 0.0 || !isfinite (t.prev_tau) || !(t.prev_tau > -0.5)
      || !(t.prev_tau <= 0.5) || !isfinite (t.prev_mid)
      || t.prev_mid < 0.0
      /* |c| <= sum |x|^2 for any stream; a little room for rounding */
      || hypot (t.c_re, t.c_im) > t.pwr * (1.0 + 1e-9) + 1e-300)
    return DP_ERR_INVALID;
  *state = t;
  return DP_OK;
}
