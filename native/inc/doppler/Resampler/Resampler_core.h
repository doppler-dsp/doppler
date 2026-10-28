/**
 * @file Resampler_core.h
 * @brief Continuously-variable polyphase resampler, CF32 IQ.
 *
 * Thin adapter over resamp_core (resamp_state_t).  Exposes a
 * Resampler-prefixed API so the generated ext.c compiles without changes.
 *
 * Lifecycle:
 * @code
 *   dp_Resampler_state_t *r = dp_Resampler_create(0.5);
 *   float _Complex out[4096];
 *   size_t n = dp_Resampler_execute(r, in, 1024, out, 1024);
 *   dp_Resampler_destroy(r);
 * @endcode
 *
 * Output buffer sizing:
 *   execute: allocate dp_Resampler_execute_max_out() samples.
 *   execute_ctrl: same.
 */
#ifndef DP_RESAMPLER_CORE_H
#define DP_RESAMPLER_CORE_H

#include <stdint.h>
#include "doppler/resamp/resamp_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** Resampler state: an alias of the resamp leaf's resamp_state_t, so
   *  every dp_Resampler_* call forwards to the matching dp_resamp_* one. */
  typedef resamp_state_t dp_Resampler_state_t;

/** Output capacity, in samples, the binding pre-allocates for one call
 *  (returned by both *_max_out() hints). */
#define RESAMPLER_MAX_OUT 65536

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Create a Resampler with the built-in 4096×19 Kaiser bank.
   * The bank provides ~60 dB alias rejection with 0.4/0.6 pass/stop
   * normalised cutoffs. Pass rate >= 1.0 to interpolate (upsample);
   * pass rate < 1.0 to decimate (downsample). For a custom bank use
   * dp_Resampler_create_custom() instead (from Python, pass bank=).
   *
   * @param rate  Output-to-input sample rate ratio: finite and > 0
   *              (dp_resamp_rate_ok()). Values >= 1.0 interpolate; values
   *              < 1.0 decimate. From Python it defaults to 1.0.
   * @return Non-NULL on success, owned by the caller and released with
   *         dp_Resampler_destroy(); NULL if @p rate is outside
   *         dp_resamp_rate_ok(), or on OOM.
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
  dp_Resampler_state_t *dp_Resampler_create (double rate);

  /**
   * @brief Create a Resampler with a user-supplied polyphase bank.
   *
   * The bank is copied, so the caller keeps ownership of @p bank.
   *
   * @param num_phases  Number of polyphase branches; a power of two (any
   *                    other count is refused, since the branch select would
   *                    read past the bank).
   * @param num_taps    Taps per branch; > 0.
   * @param bank        Row-major float32 array, shape num_phases × num_taps.
   * @param rate        Initial output-to-input sample rate ratio; finite
   *                    and > 0 (dp_resamp_rate_ok()).
   * @return Non-NULL on success, owned by the caller and released with
   *         dp_Resampler_destroy(); NULL if @p num_phases is 0 or not a
   *         power of two, @p num_taps is 0, @p bank is NULL, @p rate is
   *         outside dp_resamp_rate_ok(), or on OOM.
   */
  dp_Resampler_state_t *dp_Resampler_create_custom (size_t num_phases,
                                              size_t num_taps,
                                              const float *bank,
                                              double rate);

  /**
   * @brief Free the bank, the delay lines and the state itself.
   *
   * @param state  Resampler to free; NULL is a no-op.
   */
  void dp_Resampler_destroy (dp_Resampler_state_t *state);

  /**
   * @brief Zero the delay line and phase accumulator.
   * Rate and polyphase bank are preserved so the resampler can be
   * resumed at the same ratio. Zeroing state eliminates transient
   * artefacts when starting a new signal burst. The control accumulator
   * read by dp_Resampler_get_ctrl_acc() returns to 0.0 as well.
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
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
  void dp_Resampler_reset (dp_Resampler_state_t *state);

  /**
   * @brief Serialized-state byte size (forwarded to the resamp leaf).
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Size in bytes of the blob dp_Resampler_get_state() writes; the
   *         blob carries the resamp leaf's own envelope.
   */
  size_t dp_Resampler_state_bytes (const dp_Resampler_state_t *state);
  /**
   * @brief Serialize the resampler's phase + delay-line state into @p blob.
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @param blob   Destination; must hold dp_Resampler_state_bytes() bytes.
   */
  void dp_Resampler_get_state (const dp_Resampler_state_t *state, void *blob);
  /**
   * @brief Restore state from @p blob.
   *
   * @param state  Resampler built with the same bank shape as the writer.
   * @param blob   Blob from dp_Resampler_get_state().
   * @return DP_OK, or DP_ERR_INVALID if the blob is rejected.
   */
  int dp_Resampler_set_state (dp_Resampler_state_t *state, const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Output-capacity hint for dp_Resampler_execute().
   *
   * @param state  Unused.
   * @return Always RESAMPLER_MAX_OUT.
   */
  size_t dp_Resampler_execute_max_out (dp_Resampler_state_t *state);

  /**
   * @brief Resample a block of CF32 samples at the fixed base rate.
   * Uses the dual-mode polyphase engine: output-driven for rate >= 1
   * (interpolation), input-driven transposed-form for rate < 1
   * (decimation). State carries over between calls, so contiguous
   * blocks produce the same result as one large block.
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @param x      CF32 input samples.
   * @param x_len  Number of input samples.
   * @param out    Output buffer; must hold at least RESAMPLER_MAX_OUT samples.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return       CF32 output array; length is approximately x_len * rate,
   *               capped at max_out. A full @p out stops the call with
   *               input unread, never dropped:
   *               dp_Resampler_get_consumed() (Python: `consumed`) says
   *               where to resume.
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
  size_t dp_Resampler_execute (dp_Resampler_state_t *state, const float _Complex *x,
                            size_t x_len, float _Complex *out,
                            size_t max_out);

  /**
   * @brief Output-capacity hint for dp_Resampler_execute_ctrl().
   *
   * @param state  Unused.
   * @return Always RESAMPLER_MAX_OUT.
   */
  size_t dp_Resampler_execute_ctrl_max_out (dp_Resampler_state_t *state);

  /**
   * @brief Resample with per-sample additive rate deviations.
   * Effective rate for sample i is base_rate + `ctrl[i]`.
   * Uses a unified double-precision accumulator that handles both
   * interpolation and decimation in a single code path — suitable for
   * Doppler-shift simulation and fractional-sample timing correction.
   * ctrl and x must have the same length.
   *
   * @param state     Pointer to a valid dp_Resampler_state_t.
   * @param x         CF32 input samples.
   * @param x_len     Number of input samples.
   * @param ctrl      Real float64 array, same length as x; the per-sample
   *                  rate addend. Anything numpy can safely widen to
   *                  float64 is accepted (float32, a plain list); a
   *                  complex array is refused rather than truncated.
   * @param ctrl_len  Number of control samples; at least x_len.
   * @param out       Output buffer; must hold at least RESAMPLER_MAX_OUT samples.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return          CF32 output array; length depends on accumulated
   *                  rate deviations, capped at max_out. In C, the count
   *                  written (>= 0), or DP_ERR_INVALID (negative) when
   *                  @p ctrl_len is shorter than @p x_len: nothing is
   *                  written. 0 stays a valid, empty result, so the sign
   *                  alone tells a refusal from it (doppler#1869). A
   *                  full @p out stops the call with input unread, never
   *                  dropped: dp_Resampler_get_consumed() (Python:
   *                  `consumed`) says where to resume.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=1.0)
   * >>> x = np.zeros(64, dtype=np.complex64)
   * >>> ctrl = np.zeros(64)
   * >>> y = r.execute_ctrl(x, ctrl)
   * >>> y.shape, y.dtype
   * ((64,), dtype('complex64'))
   * @endcode
   */
  int64_t dp_Resampler_execute_ctrl (dp_Resampler_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 const double *ctrl, size_t ctrl_len,
                                 float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Get / set the output-to-input sample rate ratio.
   * The setter recomputes the phase increment immediately. On the same
   * side of 1.0 the delay line and phase accumulator are preserved, so
   * in-stream rate changes are glitch-free. Crossing 1.0 switches the
   * engine between its interpolation and decimation modes (the doctest
   * below goes 0.5 -> 1.5); the mode taking over was never advanced, so
   * the crossing resets the state as reset() does, and the output from
   * there on is what a fresh Resampler at the new rate gives. A rate that
   * is not finite and > 0 raises ValueError and leaves the rate as it was.
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
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Current output-to-input sample rate ratio.
   */
  double dp_Resampler_get_rate (const dp_Resampler_state_t *state);
  /**
   * @brief Set the output-to-input sample rate ratio (see
   *        dp_Resampler_get_rate() for the mid-stream semantics).
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @param rate   New ratio; finite and > 0 (dp_resamp_rate_ok()).
   * @return DP_OK, or DP_ERR_INVALID if @p rate is refused (nothing
   *         changes).
   */
  int dp_Resampler_set_rate (dp_Resampler_state_t *state, double rate);

  /**
   * @brief The control accumulator's fractional phase, in [0, 1).
   *
   * The timing NCO's state, and the only way to see what a closed timing
   * loop is doing to the sampling instant. `floor(mu * num_phases)` is the
   * polyphase arm the NEXT output will read; a steady value means the loop
   * has settled on a sampling phase, while one that slews and wraps means
   * a residual RATE error it has not absorbed, one input interval per wrap.
   *
   * Reports the CONTROL accumulator, so it stays 0.0 unless you are driving
   * this object through execute_ctrl(): the free-running phase used by
   * execute() is a separate accumulator with no accessor.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> Resampler(rate=1.0).ctrl_acc
   * 0.0
   * @endcode
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Control-accumulator phase as a fraction of one input interval.
   */
  double dp_Resampler_get_ctrl_acc (const dp_Resampler_state_t *state);

  /**
   * @brief Input samples the last execute() or execute_ctrl() took.
   *
   * Both calls stop when @p out is full and leave the rest of the input
   * unread rather than dropping it. Python's default buffer is
   * RESAMPLER_MAX_OUT, or len(x) if larger, so at a rate above 1 a long
   * block -- or any block a steered rate stretches -- fills it before the
   * input runs out. Equal to the input length when everything fit;
   * otherwise offer `x[consumed:]` (and `ctrl[consumed:]`) to the next
   * call and the output resumes bit-exact (doppler#2643). 0 after create
   * and reset; a call refused for a short @c ctrl leaves it unchanged.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> import numpy as np
   * >>> r = Resampler(rate=2.0)
   * >>> x = np.ones(50000, dtype=np.complex64)
   * >>> y = r.execute_ctrl(x, np.zeros(50000))
   * >>> r.consumed < len(x)
   * True
   * >>> rest = r.consumed
   * >>> y2 = r.execute_ctrl(x[rest:], np.zeros(len(x) - rest))
   * >>> r.consumed == len(x) - rest
   * True
   * @endcode
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Input samples consumed by the last execute() or execute_ctrl().
   */
  size_t dp_Resampler_get_consumed (const dp_Resampler_state_t *state);

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
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Number of polyphase arms.
   */
  size_t dp_Resampler_get_num_phases (const dp_Resampler_state_t *state);

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
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Taps per polyphase arm.
   */
  size_t dp_Resampler_get_num_taps (const dp_Resampler_state_t *state);

  /**
   * @brief Group delay of the interpolator, in input samples.
   *
   * The prototype's centre plus the one input the pipeline holds back
   * (dp_resamp_get_delay()): output `k` carries the input at
   * `k / rate + (accumulated ctrl) - delay`. 10.5 for the built-in bank.
   *
   * @code
   * >>> from doppler.resample import Resampler
   * >>> Resampler(rate=1.0).delay
   * 10.5
   * @endcode
   *
   * @param state  Pointer to a valid dp_Resampler_state_t.
   * @return Group delay in input samples.
   */
  double dp_Resampler_get_delay (const dp_Resampler_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLER_CORE_H */
