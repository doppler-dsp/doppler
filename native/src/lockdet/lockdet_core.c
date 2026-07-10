#include "lockdet/lockdet_core.h"
#include <stdlib.h>

void
lockdet_init (lockdet_state_t *state, double up_thresh, double down_thresh,
              uint32_t n_up, uint32_t n_down)
{
  /* Clamp the verify counts: 0 would make the >= comparison in
   * lockdet_step unreachable-by-increment on the first look and is never a
   * meaningful config; 1 means "no time hysteresis on that side". cnt and
   * locked are left untouched so init doubles as a reconfigure. */
  state->up_thresh   = up_thresh;
  state->down_thresh = down_thresh;
  state->n_up        = n_up ? n_up : 1;
  state->n_down      = n_down ? n_down : 1;
}

lockdet_state_t *
lockdet_create (double up_thresh, double down_thresh, uint32_t n_up,
                uint32_t n_down)
{
  lockdet_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  /* cnt/locked already zeroed by calloc */
  lockdet_init (obj, up_thresh, down_thresh, n_up, n_down);
  return obj;
}

void
lockdet_destroy (lockdet_state_t *state)
{
  free (state);
}

void
lockdet_configure (lockdet_state_t *state, double up_thresh,
                   double down_thresh, uint32_t n_up, uint32_t n_down)
{
  lockdet_init (state, up_thresh, down_thresh, n_up, n_down);
  /* A live lock survives a re-tune, but the in-flight verify run was
   * counted against the old thresholds — restart it under the new ones. */
  state->cnt = 0;
}

void
lockdet_reset (lockdet_state_t *state)
{
  state->cnt    = 0;
  state->locked = 0;
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (lockdet, lockdet_state_t, LOCKDET_STATE_MAGIC,
                     LOCKDET_STATE_VERSION)

void
lockdet_steps (lockdet_state_t *state, const double *input, int *output,
               size_t n)
{
  for (size_t i = 0; i < n; i++)
    output[i] = lockdet_step (state, input[i]);
}
