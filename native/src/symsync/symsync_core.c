#include "symsync/symsync_core.h"

#include <math.h>
#include <stdlib.h>

/* Nominal NCO increment: the accumulator advances by 1/sps of full scale per
 * input sample, wrapping once per symbol.  The on-time strobe is the wrap and
 * the mid-symbol strobe is the half-scale crossing, giving the Gardner
 * detector its two interpolants per symbol from one accumulator. */
static uint32_t
nominal_inc (size_t sps)
{
  double s = (double)(sps ? sps : 1);
  return (uint32_t)(4294967296.0 / s);
}

static void
seed (symsync_state_t *s)
{
  s->timing.phase     = 0;
  s->timing.phase_inc = s->base_inc;
  s->have_ontime      = 0;
  s->prev_ontime      = 0.0f;
  s->mid              = 0.0f;
  s->last_error       = 0.0;
  s->rate_est         = (double)s->sps;
  s->pwr_avg          = 1.0;
  farrow_reset (&s->farrow);
}

symsync_state_t *
symsync_create (size_t sps, double bn, double zeta, int order)
{
  symsync_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->sps      = sps ? sps : 1;
  obj->bn       = bn;
  obj->zeta     = zeta;
  obj->base_inc = nominal_inc (obj->sps);
  farrow_init (&obj->farrow, order);
  loop_filter_init (&obj->lf, bn, zeta, 1.0); /* one update per symbol */
  seed (obj);
  return obj;
}

void
symsync_destroy (symsync_state_t *state)
{
  free (state);
}

void
symsync_reset (symsync_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

void
symsync_configure (symsync_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

size_t
symsync_steps_max_out (symsync_state_t *state)
{
  (void)state;
  return 0;
}

size_t
symsync_steps (symsync_state_t *state, const float complex *x, size_t x_len,
               float complex *out, size_t max_out)
{
  const uint32_t HALF    = 0x80000000u;
  size_t         emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      farrow_push (&state->farrow, x[n]);
      /* One integer NCO wraps once per symbol.  The two Gardner strobes are
       * derived from the phase VALUE — a half-scale crossing (mid-symbol) and
       * a full-scale wrap (on-time) — not a parity counter, so a proportional
       * phase nudge can never desync them. inc <= 2^31 ⇒ ≤ one event/sample.
       */
      uint32_t old        = state->timing.phase;
      uint64_t sum        = (uint64_t)old + state->timing.phase_inc;
      state->timing.phase = (uint32_t)sum;

      int wrapped = sum >> 32 != 0;
      int mid_evt = !wrapped && old < HALF && (uint32_t)sum >= HALF;
      if (!wrapped && !mid_evt)
        continue;

      double inc = (double)state->timing.phase_inc;
      if (mid_evt)
        {
          /* mid-symbol interpolant (the half-scale crossing) */
          float mu   = (float)(1.0 - ((double)((uint32_t)sum - HALF)) / inc);
          state->mid = farrow_eval (&state->farrow, mu);
          continue;
        }
      /* on-time strobe (the wrap): crossing fraction past the basepoint */
      float         mu = (float)(1.0 - (double)state->timing.phase / inc);
      float complex y  = farrow_eval (&state->farrow, mu);
      if (state->have_ontime)
        {
          /* Gardner TED: e = Re{ conj(mid) * (y - prev_ontime) }, normalised
           * by a RUNNING-average symbol power so the detector gain is ~unity
           * without the pattern-dependent bias an instantaneous normaliser
           * introduces. */
          float complex diff = y - state->prev_ontime;
          double        num  = (double)(crealf (state->mid) * crealf (diff)
                                        + cimagf (state->mid) * cimagf (diff));
          double        inst_pwr
              = (double)(crealf (y) * crealf (y) + cimagf (y) * cimagf (y));
          state->pwr_avg += 0.01 * (inst_pwr - state->pwr_avg);
          double e          = num / (state->pwr_avg + 1e-6);
          state->last_error = e;
          /* Steer the integer NCO through its FREQUENCY only: the full PI
           * output (proportional + integral) adjusts the strobe rate. Folding
           * the proportional term into the rate (rather than nudging the phase
           * directly) keeps the strobe count smooth — a direct phase nudge
           * near a wrap boundary would insert/delete a strobe (a cycle slip).
           */
          double control = loop_filter_step (&state->lf, e);
          state->timing.phase_inc
              = (uint32_t)((double)state->base_inc * (1.0 + control));
          /* smoothed estimate of the actual strobe rate: the instantaneous
           * samples/symbol, clamped to a sane band so an acquisition transient
           * cannot poison the EMA, then low-pass filtered. */
          double inst = (double)state->sps / (1.0 + control);
          double lo_r = 0.5 * (double)state->sps,
                 hi_r = 1.5 * (double)state->sps;
          if (inst < lo_r)
            inst = lo_r;
          else if (inst > hi_r)
            inst = hi_r;
          state->rate_est += 0.02 * (inst - state->rate_est);
          if (emitted < max_out)
            out[emitted++] = y;
        }
      else
        {
          state->have_ontime = 1;
        }
      state->prev_ontime = y;
    }
  return emitted;
}

double
symsync_get_bn (const symsync_state_t *state)
{
  return state->bn;
}

void
symsync_set_bn (symsync_state_t *state, double val)
{
  symsync_configure (state, val, state->zeta);
}

double
symsync_get_timing_error (const symsync_state_t *state)
{
  return state->last_error;
}

double
symsync_get_rate (const symsync_state_t *state)
{
  /* effective samples/symbol the loop is tracking (EMA of the instantaneous
   * strobe rate) */
  return state->rate_est;
}
