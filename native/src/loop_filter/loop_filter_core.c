#include "loop_filter/loop_filter_core.h"
#include <math.h>
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
  /* This is the UNTRUSTED boundary and the only one: `LoopFilter(...)` hands
     a Python caller's arbitrary doubles straight here, where t = 0 used to
     yield kp = ki = 0 — a dead loop indistinguishable from the legitimate
     frozen bn = 0 — and t = inf or a NaN argument yielded NaN gains, which
     poison every subsequent update permanently. loop_filter_init() is
     deliberately NOT guarded: it is the by-value path, its seven embedders
     all validate upstream, and guarding an internal guarantee is the error
     handling this project does not write (gh-740).

     Validating here also makes the arithmetic TOTAL. With bn >= 0 and
     zeta > 0 the intermediate th is non-negative, so
     den = 4 + 4*zeta*th + th^2 >= 4 and can no longer pass through zero —
     which it can for zeta >= 1 with a sufficiently negative bn. */
  if (!(bn >= 0.0) || !(zeta > 0.0) || !(t > 0.0) || !isfinite (bn)
      || !isfinite (zeta) || !isfinite (t))
    return NULL; /* NaN fails every comparison above, by construction */

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
loop_filter_steps (loop_filter_state_t *state, const double *x, double *out,
                   size_t n)
{
  for (size_t i = 0; i < n; i++)
    out[i] = loop_filter_step (state, x[i]);
}
