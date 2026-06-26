#include "farrow/farrow_core.h"

#include <stdlib.h>

farrow_state_t *
farrow_create (int order)
{
  farrow_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  farrow_init (obj, order);
  return obj;
}

void
farrow_destroy (farrow_state_t *state)
{
  free (state);
}

void
farrow_reset (farrow_state_t *state)
{
  state->d[0] = state->d[1] = state->d[2] = state->d[3] = 0.0f;
}

size_t
farrow_get_group_delay (const farrow_state_t *state)
{
  (void)state;
  return FARROW_GROUP_DELAY;
}

/* Output is one sample per input (same length); the binding sizes the buffer
 * to the input length, so 0 (== "caller sizes") is the right sentinel. */
size_t
farrow_delay_max_out (farrow_state_t *state)
{
  (void)state;
  return 0;
}

/* Apply a constant fractional delay of `mu` samples: push each input and
 * evaluate at `mu`.  out[i] is the input interpolated at i - group_delay + mu;
 * the first group_delay samples are the delay-line filling transient. */
size_t
farrow_delay (farrow_state_t *state, const float complex *x, size_t x_len,
              double mu, float complex *out, size_t max_out)
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
