#include "symsync/symsync_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

void
symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
              int order)
{
  /* Zero first so an in-place (stack-embedded) init byte-matches the
   * calloc + init done by symsync_create: seed() sets only the timing NCO's
   * phase/phase_inc, leaving the NCO's norm_freq/nmax to this memset. */
  memset (s, 0, sizeof (*s));
  s->sps      = sps ? sps : 1;
  s->bn       = bn;
  s->zeta     = zeta;
  s->base_inc = nominal_inc (s->sps);
  farrow_init (&s->farrow, order);
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* one update per symbol */
  seed (s);
}

symsync_state_t *
symsync_create (size_t sps, double bn, double zeta, int order)
{
  symsync_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  symsync_init (obj, sps, bn, zeta, order);
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

/* Serializable state — pointer-free composition (nco + farrow + loop_filter
 * embedded by value, all POD) + scalar timing state: a whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (symsync, symsync_state_t, SYMSYNC_STATE_MAGIC,
                     SYMSYNC_STATE_VERSION)

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
  size_t        emitted = 0;
  float complex y;
  for (size_t n = 0; n < x_len; n++)
    if (symsync_step (state, x[n], &y) && emitted < max_out)
      out[emitted++] = y;
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
