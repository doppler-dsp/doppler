#include "loop_filter/loop_filter_core.h"
#include <stdlib.h>

void
loop_filter_init (loop_filter_state_t *state, double bn, double zeta, double t)
{
  /* Standard 2nd-order PI loop-filter gains. bn is the loop noise bandwidth
   * (normalized, cycles/sample), zeta the damping factor, t the update period
   * in samples. wn is the natural frequency; the discrete kp/ki follow the
   * canonical bilinear-mapped form (e.g. Stephens & Thomas). integ is left
   * untouched so a reconfigure preserves lock. */
  double wn   = 8.0 * zeta * bn / (4.0 * zeta * zeta + 1.0);
  double th   = wn * t;
  double den  = 4.0 + 4.0 * zeta * th + th * th;
  state->bn   = bn;
  state->zeta = zeta;
  state->t    = t;
  state->kp   = (8.0 * zeta * th) / den;
  state->ki   = (4.0 * th * th) / den;
}

loop_filter_state_t *
loop_filter_create (double bn, double zeta, double t)
{
  loop_filter_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  loop_filter_init (obj, bn, zeta, t); /* integ already zeroed by calloc */
  return obj;
}

void
loop_filter_destroy (loop_filter_state_t *state)
{
  free (state);
}

void
loop_filter_configure (loop_filter_state_t *state, double bn, double zeta,
                       double t)
{
  loop_filter_init (state, bn, zeta, t); /* recompute gains, keep integ */
}

void
loop_filter_reset (loop_filter_state_t *state)
{
  state->integ = 0.0;
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (loop_filter, loop_filter_state_t, LOOP_FILTER_STATE_MAGIC,
                     LOOP_FILTER_STATE_VERSION)

void
loop_filter_steps (loop_filter_state_t *state, const double *input,
                   double *output, size_t n)
{
  for (size_t i = 0; i < n; i++)
    output[i] = loop_filter_step (state, input[i]);
}
