/**
 * @file Resampler_core.h
 * @brief Continuously-variable polyphase resampler, CF32 IQ.
 *
 * Thin adapter over resamp_core (resamp_state_t).  Exposes a
 * Resampler-prefixed API so the generated ext.c compiles without changes.
 *
 * Lifecycle:
 * @code
 *   Resampler_state_t *r = Resampler_create(0.5);
 *   float complex out[4096];
 *   size_t n = Resampler_execute(r, in, 1024, out);
 *   Resampler_destroy(r);
 * @endcode
 *
 * Output buffer sizing:
 *   execute: allocate Resampler_execute_max_out() samples.
 *   execute_ctrl: same.
 */
#ifndef RESAMPLER_CORE_H
#define RESAMPLER_CORE_H

#include "resamp/resamp_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef resamp_state_t Resampler_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define RESAMPLER_MAX_OUT 65536

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Create a Resampler with the built-in 4096×19 Kaiser bank.
   *
   * @param rate  Resample ratio (out/in).  Values >= 1.0 interpolate;
   *              values < 1.0 decimate.
   * @return Non-NULL on success, NULL on OOM.
   */
  Resampler_state_t *Resampler_create (double rate);

  /** Free all resources.  NULL is a no-op. */
  void Resampler_destroy (Resampler_state_t *state);

  /** Zero delay line and phase accumulator.  Rate and bank preserved. */
  void Resampler_reset (Resampler_state_t *state);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /** Always returns RESAMPLER_MAX_OUT. */
  size_t Resampler_execute_max_out (Resampler_state_t *state);

  /**
   * @brief Resample x(0..x_len-1) into out(0..n_out-1).
   *
   * out must be at least Resampler_execute_max_out() samples wide.
   * Returns the number of output samples written.
   */
  size_t Resampler_execute (Resampler_state_t *state, const float complex *x,
                            size_t x_len, float complex *out);

  /** Always returns RESAMPLER_MAX_OUT. */
  size_t Resampler_execute_ctrl_max_out (Resampler_state_t *state);

  /**
   * @brief Resample with per-sample rate deviations.
   *
   * rate_i = base_rate + crealf(ctrl(i)).  ctrl and x must be the
   * same length.  Returns number of output samples written.
   */
  size_t Resampler_execute_ctrl (Resampler_state_t *state,
                                 const float complex *x, size_t x_len,
                                 const float complex *ctrl, size_t ctrl_len,
                                 float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  double Resampler_get_rate (const Resampler_state_t *state);
  void Resampler_set_rate (Resampler_state_t *state, double rate);
  size_t Resampler_get_num_phases (const Resampler_state_t *state);
  size_t Resampler_get_num_taps (const Resampler_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLER_CORE_H */
