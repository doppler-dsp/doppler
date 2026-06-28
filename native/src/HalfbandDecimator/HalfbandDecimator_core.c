#include "HalfbandDecimator/HalfbandDecimator_core.h"

HalfbandDecimator_state_t *
HalfbandDecimator_create (size_t num_taps, const float *h)
{
  return hbdecim_create (num_taps, h);
}

void
HalfbandDecimator_destroy (HalfbandDecimator_state_t *state)
{
  hbdecim_destroy (state);
}

void
HalfbandDecimator_reset (HalfbandDecimator_state_t *state)
{
  hbdecim_reset (state);
}

/* Serializable state — forwarded to the hbdecim leaf (this type is a typedef
 * alias for hbdecim_state_t), so the blob carries the leaf's HBDC envelope. */

size_t
HalfbandDecimator_state_bytes (const HalfbandDecimator_state_t *state)
{
  return hbdecim_state_bytes (state);
}

void
HalfbandDecimator_get_state (const HalfbandDecimator_state_t *state,
                             void                            *blob)
{
  hbdecim_get_state (state, blob);
}

int
HalfbandDecimator_set_state (HalfbandDecimator_state_t *state,
                             const void                *blob)
{
  return hbdecim_set_state (state, blob);
}

size_t
HalfbandDecimator_execute_max_out (HalfbandDecimator_state_t *state)
{
  (void)state;
  return HBDECIM_MAX_OUT;
}

size_t
HalfbandDecimator_execute (HalfbandDecimator_state_t *state,
                           const float complex *x, size_t x_len,
                           float complex *out)
{
  return hbdecim_execute (state, x, x_len, out, HBDECIM_MAX_OUT);
}

double
HalfbandDecimator_get_rate (const HalfbandDecimator_state_t *state)
{
  return hbdecim_get_rate (state);
}

size_t
HalfbandDecimator_get_num_taps (const HalfbandDecimator_state_t *state)
{
  return hbdecim_get_num_taps (state);
}
