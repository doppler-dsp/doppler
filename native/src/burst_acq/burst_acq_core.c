#include "doppler/burst_acq/burst_acq_core.h"

#include <stdlib.h>

/* Wrap an engine, or pass its NULL through: the one place a
   dp_burst_acq_state_t is built, whichever constructor made the engine. */
static dp_burst_acq_state_t *
burst_acq_wrap (dp_correlator_state_t *engine)
{
  if (!engine)
    return NULL;
  dp_burst_acq_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    {
      dp_correlator_destroy (engine);
      return NULL;
    }
  obj->engine       = engine;
  obj->underpowered = engine->search->underpowered;
  return obj;
}

dp_burst_acq_state_t *
dp_burst_acq_create (const float _Complex *preamble, size_t preamble_len,
                     size_t reps, double fs, double cn0_dbhz,
                     double doppler_uncertainty, double pfa, double pd,
                     int noise_mode, double doppler_rate,
                     size_t rate_hypotheses)
{
  return burst_acq_wrap (dp_correlator_create_burst (
      preamble, preamble_len, reps, fs, cn0_dbhz, doppler_uncertainty, pfa, pd,
      noise_mode, doppler_rate, rate_hypotheses));
}

void
dp_burst_acq_destroy (dp_burst_acq_state_t *state)
{
  if (!state)
    return;
  dp_correlator_destroy (state->engine);
  free (state);
}

void
dp_burst_acq_reset (dp_burst_acq_state_t *state)
{
  dp_correlator_reset (state->engine);
}

size_t
dp_burst_acq_push (dp_burst_acq_state_t *state, const float _Complex *x,
                   size_t n_in, dp_correlator_hit_t *result,
                   size_t max_results)
{
  return dp_correlator_push (state->engine, x, n_in, result, max_results);
}

size_t
dp_burst_acq_consumed (const dp_burst_acq_state_t *state)
{
  return dp_correlator_consumed (state->engine);
}

int
dp_burst_acq_configure_search_raw (dp_burst_acq_state_t *state,
                                   size_t doppler_bins, size_t n_noncoh)
{
  return dp_correlator_configure_search_raw (state->engine, doppler_bins,
                                             n_noncoh);
}

int
dp_burst_acq_set_max_peaks (dp_burst_acq_state_t *state, size_t n)
{
  return dp_correlator_set_max_peaks (state->engine, n);
}

size_t
dp_burst_acq_state_bytes (const dp_burst_acq_state_t *state)
{
  return dp_correlator_state_bytes (state->engine);
}

void
dp_burst_acq_get_state (const dp_burst_acq_state_t *state, void *blob)
{
  dp_correlator_get_state (state->engine, blob);
}

int
dp_burst_acq_set_state (dp_burst_acq_state_t *state, const void *blob)
{
  return dp_correlator_set_state (state->engine, blob);
}
