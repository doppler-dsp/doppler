#include "Resampler/Resampler_core.h"

Resampler_state_t *
Resampler_create (double rate)
{
  return resamp_create (rate);
}

Resampler_state_t *
Resampler_create_custom (size_t num_phases, size_t num_taps, const float *bank,
                         double rate)
{
  return resamp_create_custom (num_phases, num_taps, bank, rate);
}

void
Resampler_destroy (Resampler_state_t *state)
{
  resamp_destroy (state);
}

void
Resampler_reset (Resampler_state_t *state)
{
  resamp_reset (state);
}

/* Serializable state — forwarded to the resamp leaf (this type is a typedef
 * alias for resamp_state_t), so the blob carries the leaf's RSMP envelope. */

size_t
Resampler_state_bytes (const Resampler_state_t *state)
{
  return resamp_state_bytes (state);
}

void
Resampler_get_state (const Resampler_state_t *state, void *blob)
{
  resamp_get_state (state, blob);
}

int
Resampler_set_state (Resampler_state_t *state, const void *blob)
{
  return resamp_set_state (state, blob);
}

size_t
Resampler_execute_max_out (Resampler_state_t *state)
{
  (void)state;
  return RESAMPLER_MAX_OUT;
}

size_t
Resampler_execute (Resampler_state_t *state, const float complex *x,
                   size_t x_len, float complex *out)
{
  return resamp_execute (state, x, x_len, out, RESAMPLER_MAX_OUT);
}

size_t
Resampler_execute_ctrl_max_out (Resampler_state_t *state)
{
  (void)state;
  return RESAMPLER_MAX_OUT;
}

size_t
Resampler_execute_ctrl (Resampler_state_t *state, const float complex *x,
                        size_t x_len, const float complex *ctrl,
                        size_t ctrl_len, float complex *out)
{
  size_t n = x_len < ctrl_len ? x_len : ctrl_len;
  return resamp_execute_ctrl (state, x, ctrl, n, out, RESAMPLER_MAX_OUT);
}

double
Resampler_get_rate (const Resampler_state_t *state)
{
  return resamp_get_rate (state);
}

void
Resampler_set_rate (Resampler_state_t *state, double rate)
{
  resamp_set_rate (state, rate);
}

size_t
Resampler_get_num_phases (const Resampler_state_t *state)
{
  return resamp_get_num_phases (state);
}

size_t
Resampler_get_num_taps (const Resampler_state_t *state)
{
  return resamp_get_num_taps (state);
}
