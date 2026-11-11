#include "doppler/HalfbandDecimatorR2C/HalfbandDecimatorR2C_core.h"

/* A pure forwarder: every call is the hbdecim_r2c leaf's, so the object
   owns no algorithm of its own (the C-first rule), the same shape
   HalfbandDecimator is over hbdecim. */

dp_HalfbandDecimatorR2C_state_t *
dp_HalfbandDecimatorR2C_create (const float *h, size_t h_len)
{
  /* The leaf takes (len, ptr), like hbdecim's create. */
  return dp_hbdecim_r2c_create (h_len, h);
}

void
dp_HalfbandDecimatorR2C_destroy (dp_HalfbandDecimatorR2C_state_t *state)
{
  dp_hbdecim_r2c_destroy (state);
}

void
dp_HalfbandDecimatorR2C_reset (dp_HalfbandDecimatorR2C_state_t *state)
{
  dp_hbdecim_r2c_reset (state);
}

size_t
dp_HalfbandDecimatorR2C_state_bytes (
    const dp_HalfbandDecimatorR2C_state_t *state)
{
  return dp_hbdecim_r2c_state_bytes (state);
}

void
dp_HalfbandDecimatorR2C_get_state (
    const dp_HalfbandDecimatorR2C_state_t *state, void *blob)
{
  dp_hbdecim_r2c_get_state (state, blob);
}

int
dp_HalfbandDecimatorR2C_set_state (dp_HalfbandDecimatorR2C_state_t *state,
                                   const void                      *blob)
{
  return dp_hbdecim_r2c_set_state (state, blob);
}

size_t
dp_HalfbandDecimatorR2C_execute_max_out (
    dp_HalfbandDecimatorR2C_state_t *state, size_t x_len)
{
  /* One output per input pair, plus at most one sample held from the last
     call: (x_len + held) / 2 <= (x_len + 1) / 2. */
  (void)state;
  return (x_len + 1) / 2;
}

size_t
dp_HalfbandDecimatorR2C_execute (dp_HalfbandDecimatorR2C_state_t *state,
                                 const float *x, size_t x_len,
                                 float _Complex *out, size_t max_out)
{
  return dp_hbdecim_r2c_execute (state, x, x_len, out, max_out);
}

double
dp_HalfbandDecimatorR2C_get_rate (const dp_HalfbandDecimatorR2C_state_t *state)
{
  return dp_hbdecim_r2c_get_rate (state);
}

size_t
dp_HalfbandDecimatorR2C_get_num_taps (
    const dp_HalfbandDecimatorR2C_state_t *state)
{
  return dp_hbdecim_r2c_get_num_taps (state);
}
