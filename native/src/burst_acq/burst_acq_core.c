#include "burst_acq/burst_acq_core.h"

#include <stdlib.h>

/* Wrap an engine, or pass its NULL through: the one place a
   burst_acq_state_t is built, whichever constructor made the engine. */
static burst_acq_state_t *
burst_acq_wrap (acq_state_t *engine)
{
  if (!engine)
    return NULL;
  burst_acq_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    {
      acq_destroy (engine);
      return NULL;
    }
  obj->engine       = engine;
  obj->underpowered = engine->underpowered;
  return obj;
}

burst_acq_state_t *
burst_acq_create (const uint8_t *code, size_t code_len, size_t reps,
                  size_t spc, double chip_rate, double cn0_dbhz,
                  double doppler_uncertainty, double pfa, double pd,
                  int noise_mode, double doppler_rate)
{
  return burst_acq_wrap (acq_create_burst (
      code, code_len, reps, spc, chip_rate, cn0_dbhz, doppler_uncertainty, pfa,
      pd, noise_mode, doppler_rate));
}

burst_acq_state_t *
burst_acq_create_template (const float _Complex *tmpl, size_t n, size_t reps,
                           double fs, double cn0_dbhz,
                           double doppler_uncertainty, double pfa, double pd,
                           int noise_mode, double doppler_rate)
{
  return burst_acq_wrap (acq_create_burst_template (
      tmpl, n, reps, fs, cn0_dbhz, doppler_uncertainty, pfa, pd, noise_mode,
      doppler_rate));
}

burst_acq_state_t *
burst_acq_bind_code (const uint8_t *code, size_t code_len, size_t reps,
                     size_t spc, double chip_rate, double cn0_dbhz,
                     double doppler_uncertainty, double pfa, double pd,
                     int noise_mode, double fs, double doppler_rate)
{
  (void)fs; /* the template branch's argument */
  return burst_acq_create (code, code_len, reps, spc, chip_rate, cn0_dbhz,
                           doppler_uncertainty, pfa, pd, noise_mode,
                           doppler_rate);
}

burst_acq_state_t *
burst_acq_bind_template (const float _Complex *tmpl, size_t n, size_t reps,
                         size_t spc, double chip_rate, double cn0_dbhz,
                         double doppler_uncertainty, double pfa, double pd,
                         int noise_mode, double fs, double doppler_rate)
{
  (void)spc; /* the code branch's arguments */
  (void)chip_rate;
  return burst_acq_create_template (tmpl, n, reps, fs, cn0_dbhz,
                                    doppler_uncertainty, pfa, pd, noise_mode,
                                    doppler_rate);
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
burst_acq_push (burst_acq_state_t *state, const float _Complex *x, size_t n_in,
                acq_result_t *result, size_t max_results)
{
  return acq_push (state->engine, x, n_in, result, max_results);
}

int
burst_acq_configure_search_raw (burst_acq_state_t *state, size_t doppler_bins,
                                size_t n_noncoh)
{
  return acq_configure_search_raw (state->engine, doppler_bins, n_noncoh);
}

int
burst_acq_set_max_peaks (burst_acq_state_t *state, size_t n)
{
  return acq_set_max_peaks (state->engine, n);
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
