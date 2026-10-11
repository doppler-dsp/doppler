#include "doppler/farrow/farrow_core.h"

#include <stdlib.h>

dp_farrow_state_t *
dp_farrow_create (int order)
{
  if (order < FARROW_LINEAR || order > FARROW_CUBIC)
    return NULL; /* farrow_eval would read any other order as cubic */
  dp_farrow_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  farrow_init (obj, order);
  return obj;
}

void
dp_farrow_destroy (dp_farrow_state_t *state)
{
  free (state);
}

void
dp_farrow_reset (dp_farrow_state_t *state)
{
  state->d[0] = state->d[1] = state->d[2] = state->d[3] = 0.0f;
}

/* Serializable state — whole-struct POD snapshot, pointer-free, refusing an
 * order create() would refuse (see DP_DEFINE_POD_STATE_CHECKED in
 * dp_state.h). */
DP_DEFINE_POD_STATE_CHECKED (dp_farrow, dp_farrow_state_t, FARROW_STATE_MAGIC,
                             FARROW_STATE_VERSION, dp_farrow_state_ok)

size_t
dp_farrow_get_group_delay (const dp_farrow_state_t *state)
{
  (void)state;
  return FARROW_GROUP_DELAY;
}

/* Output is one sample per input (same length); the binding sizes the buffer
 * to the input length, so 0 (== "caller sizes") is the right sentinel. */
size_t
dp_farrow_delay_max_out (dp_farrow_state_t *state)
{
  (void)state;
  return 0; /* delay() emits one sample per input sample */
}

/* Apply a constant fractional delay of `mu` samples: push each input and
 * evaluate at `mu`.  out[i] is the input interpolated at i - group_delay + mu;
 * the first group_delay samples are the delay-line filling transient. */
size_t
dp_farrow_delay (dp_farrow_state_t *state, const float _Complex *x,
                 size_t x_len, double mu, float _Complex *out, size_t max_out)
{
  float  m = (float)mu;
  size_t k = 0;
  for (size_t n = 0; n < x_len && k < max_out; n++)
    {
      farrow_push (state, x[n]);
      out[k++] = farrow_eval (state, m);
    }
  return k;
}
