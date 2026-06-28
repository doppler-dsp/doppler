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

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * The struct is pointer-free POD, so a whole-struct snapshot is exact; the
 * derived gains / config restore identically into an identical instance. */

size_t
loop_filter_state_bytes (const loop_filter_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (loop_filter_state_t);
}

void
loop_filter_get_state (const loop_filter_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, loop_filter_state_bytes (state));
  dp_w_hdr (&w, LOOP_FILTER_STATE_MAGIC, LOOP_FILTER_STATE_VERSION,
            loop_filter_state_bytes (state));
  dp_w_bytes (&w, state, sizeof *state);
}

int
loop_filter_set_state (loop_filter_state_t *state, const void *blob)
{
  int rc
      = dp_state_validate (blob, loop_filter_state_bytes (state),
                           LOOP_FILTER_STATE_MAGIC, LOOP_FILTER_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, loop_filter_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  dp_r_bytes (&r, state, sizeof *state);
  return DP_OK;
}

void
loop_filter_steps (loop_filter_state_t *state, const double *input,
                   double *output, size_t n)
{
  for (size_t i = 0; i < n; i++)
    output[i] = loop_filter_step (state, input[i]);
}
