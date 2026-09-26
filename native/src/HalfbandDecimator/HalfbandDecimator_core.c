#include "doppler/HalfbandDecimator/HalfbandDecimator_core.h"

dp_HalfbandDecimator_state_t *
dp_HalfbandDecimator_create (const float *h, size_t h_len)
{
  /* hbdecim, the primitive underneath, still takes (len, ptr) -- it and its
     two siblings are the only creates in the tree that do. Swapping here
     rather than there keeps this change to the jm object's own face; the
     types differ, so a call that gets the order wrong does not compile. */
  return hbdecim_create (h_len, h);
}

void
dp_HalfbandDecimator_destroy (dp_HalfbandDecimator_state_t *state)
{
  hbdecim_destroy (state);
}

void
dp_HalfbandDecimator_reset (dp_HalfbandDecimator_state_t *state)
{
  hbdecim_reset (state);
}

/* Serializable state — forwarded to the hbdecim leaf (this type is a typedef
 * alias for hbdecim_state_t), so the blob carries the leaf's HBDC envelope. */

size_t
dp_HalfbandDecimator_state_bytes (const dp_HalfbandDecimator_state_t *state)
{
  return hbdecim_state_bytes (state);
}

void
dp_HalfbandDecimator_get_state (const dp_HalfbandDecimator_state_t *state,
                                void                               *blob)
{
  hbdecim_get_state (state, blob);
}

int
dp_HalfbandDecimator_set_state (dp_HalfbandDecimator_state_t *state,
                                const void                   *blob)
{
  return hbdecim_set_state (state, blob);
}

size_t
dp_HalfbandDecimator_execute_max_out (dp_HalfbandDecimator_state_t *state)
{
  (void)state;
  return HBDECIM_MAX_OUT;
}

size_t
dp_HalfbandDecimator_execute (dp_HalfbandDecimator_state_t *state,
                              const float _Complex *x, size_t x_len,
                              float _Complex *out, size_t max_out)
{
  /* The leaf already clamps; hand it the caller's real capacity instead
     of the fixed cap (jm gh-138). */
  return hbdecim_execute (state, x, x_len, out, max_out);
}

double
dp_HalfbandDecimator_get_rate (const dp_HalfbandDecimator_state_t *state)
{
  return hbdecim_get_rate (state);
}

size_t
dp_HalfbandDecimator_get_num_taps (const dp_HalfbandDecimator_state_t *state)
{
  return hbdecim_get_num_taps (state);
}
