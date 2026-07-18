#include "burst_acq/burst_acq_core.h"

#include <stdlib.h>

burst_acq_state_t *
burst_acq_create (const uint8_t *code, size_t code_len, size_t reps,
                  size_t spc, double chip_rate, double cn0_dbhz,
                  double doppler_uncertainty, double pfa, double pd,
                  int noise_mode)
{
  burst_acq_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->engine
      = acq_create_burst (code, code_len, reps, spc, chip_rate, cn0_dbhz,
                          doppler_uncertainty, pfa, pd, noise_mode);
  if (!obj->engine)
    {
      free (obj);
      return NULL;
    }
  return obj;
}

void
burst_acq_destroy (burst_acq_state_t *state)
{
  if (!state)
    return;
  acq_destroy (state->engine);
  free (state);
}

void
burst_acq_reset (burst_acq_state_t *state)
{
  acq_reset (state->engine);
}

size_t
burst_acq_push (burst_acq_state_t *state, const float complex *in, size_t n_in,
                acq_result_t *result, size_t max_results)
{
  return acq_push (state->engine, in, n_in, result, max_results);
}

int
burst_acq_configure_search_raw (burst_acq_state_t *state, size_t doppler_bins,
                                size_t n_noncoh)
{
  return acq_configure_search_raw (state->engine, doppler_bins, n_noncoh);
}

size_t
burst_acq_state_bytes (const burst_acq_state_t *state)
{
  return acq_state_bytes (state->engine);
}

void
burst_acq_get_state (const burst_acq_state_t *state, void *blob)
{
  acq_get_state (state->engine, blob);
}

int
burst_acq_set_state (burst_acq_state_t *state, const void *blob)
{
  return acq_set_state (state->engine, blob);
}
