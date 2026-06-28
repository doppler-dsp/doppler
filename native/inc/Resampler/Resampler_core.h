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
   * The bank provides ~60 dB alias rejection with 0.4/0.6 pass/stop
   * normalised cutoffs. Pass rate >= 1.0 to interpolate (upsample);
   * pass rate < 1.0 to decimate (downsample). For a custom bank use
   * Resampler_create_custom() instead.
   *
   * @param rate  Output-to-input sample rate ratio (any positive float).
   *              Values >= 1.0 interpolate; values < 1.0 decimate.
   * @return Non-NULL on success, NULL on OOM.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=2.0)
   * >>> r.num_phases, r.num_taps
   * (4096, 19)
   * >>> r.rate
   * 2.0
   * @endcode
   */
  Resampler_state_t *Resampler_create (double rate);

  /**
   * @brief Create a Resampler with a user-supplied polyphase bank.
   *
   * @param num_phases  Number of polyphase branches (must be power of two).
   * @param num_taps    Taps per branch.
   * @param bank        Row-major float32 array, shape num_phases × num_taps.
   * @param rate        Initial resample ratio.
   * @return Non-NULL on success, NULL on invalid args or OOM.
   */
  Resampler_state_t *Resampler_create_custom (size_t num_phases,
                                              size_t num_taps,
                                              const float *bank,
                                              double rate);

  /** Free all resources.  NULL is a no-op. */
  void Resampler_destroy (Resampler_state_t *state);

  /**
   * @brief Zero the delay line and phase accumulator.
   * Rate and polyphase bank are preserved so the resampler can be
   * resumed at the same ratio. Zeroing state eliminates transient
   * artefacts when starting a new signal burst.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=2.0)
   * >>> _ = r.execute(np.ones(64, dtype=np.complex64))
   * >>> r.reset()
   * >>> r.rate
   * 2.0
   * @endcode
   */
  void Resampler_reset (Resampler_state_t *state);

  /** @brief Serialized-state byte size (forwarded to the resamp leaf). */
  size_t Resampler_state_bytes (const Resampler_state_t *state);
  /** @brief Serialize the resampler's phase + delay-line state into @p blob. */
  void Resampler_get_state (const Resampler_state_t *state, void *blob);
  /** @brief Restore state from @p blob; DP_OK, or DP_ERR_INVALID if rejected. */
  int Resampler_set_state (Resampler_state_t *state, const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /** Always returns RESAMPLER_MAX_OUT. */
  size_t Resampler_execute_max_out (Resampler_state_t *state);

  /**
   * @brief Resample a block of CF32 samples at the fixed base rate.
   * Uses the dual-mode polyphase engine: output-driven for rate >= 1
   * (interpolation), input-driven transposed-form for rate < 1
   * (decimation). State carries over between calls, so contiguous
   * blocks produce the same result as one large block.
   *
   * @param state  Pointer to a valid Resampler_state_t.
   * @param x      CF32 input samples.
   * @param x_len  Number of input samples.
   * @param out    Output buffer; must hold at least RESAMPLER_MAX_OUT samples.
   * @return       CF32 output array; length is approximately x_len * rate.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=2.0)
   * >>> y = r.execute(np.zeros(128, dtype=np.complex64))
   * >>> y.shape, y.dtype
   * ((256,), dtype('complex64'))
   * @endcode
   */
  size_t Resampler_execute (Resampler_state_t *state, const float complex *x,
                            size_t x_len, float complex *out);

  /** Always returns RESAMPLER_MAX_OUT. */
  size_t Resampler_execute_ctrl_max_out (Resampler_state_t *state);

  /**
   * @brief Resample with per-sample additive rate deviations.
   * Effective rate for sample i is base_rate + real(`ctrl[i]`).
   * Uses a unified double-precision accumulator that handles both
   * interpolation and decimation in a single code path — suitable for
   * Doppler-shift simulation and fractional-sample timing correction.
   * ctrl and x must have the same length.
   *
   * @param state     Pointer to a valid Resampler_state_t.
   * @param x         CF32 input samples.
   * @param x_len     Number of input samples.
   * @param ctrl      CF32 array, same length as x; only the real part is
   *                  used as a per-sample rate addend.
   * @param ctrl_len  Number of control samples; must equal x_len.
   * @param out       Output buffer; must hold at least RESAMPLER_MAX_OUT samples.
   * @return          CF32 output array; length depends on accumulated
   *                  rate deviations.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=1.0)
   * >>> x = np.zeros(64, dtype=np.complex64)
   * >>> ctrl = np.zeros(64, dtype=np.complex64)
   * >>> y = r.execute_ctrl(x, ctrl)
   * >>> y.shape, y.dtype
   * ((64,), dtype('complex64'))
   * @endcode
   */
  size_t Resampler_execute_ctrl (Resampler_state_t *state,
                                 const float complex *x, size_t x_len,
                                 const float complex *ctrl, size_t ctrl_len,
                                 float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Get / set the output-to-input sample rate ratio.
   * The setter recomputes the phase increment immediately; the delay
   * line and phase accumulator are preserved so in-stream rate changes
   * are glitch-free. Switching sign of (rate - 1) (i.e. crossing the
   * boundary between interp and decim modes) requires a fresh create().
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> r = Resampler(rate=0.5)
   * >>> r.rate
   * 0.5
   * >>> r.rate = 1.5
   * >>> r.rate
   * 1.5
   * @endcode
   */
  double Resampler_get_rate (const Resampler_state_t *state);
  void Resampler_set_rate (Resampler_state_t *state, double rate);

  /**
   * @brief Number of polyphase branches in the filter bank.
   * Always a power of two. The built-in bank has 4096 phases giving
   * sub-sample timing resolution of 1/4096 of an input sample period.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> Resampler(rate=1.0).num_phases
   * 4096
   * @endcode
   */
  size_t Resampler_get_num_phases (const Resampler_state_t *state);

  /**
   * @brief Taps per polyphase branch.
   * Total prototype filter length is num_phases * num_taps - 1.
   * The built-in bank uses 19 taps per branch.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> Resampler(rate=1.0).num_taps
   * 19
   * @endcode
   */
  size_t Resampler_get_num_taps (const Resampler_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLER_CORE_H */
