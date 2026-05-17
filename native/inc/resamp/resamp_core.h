/**
 * @file resamp_core.h
 * @brief Continuously-variable polyphase resampler for CF32 IQ.
 *
 * Two execute paths:
 *
 *   resamp_execute — dual-mode:
 *     - Interpolation (rate >= 1): output-driven, one NCO tick per
 *       output sample, overflow pushes the next input into the delay
 *       line.
 *     - Decimation (rate < 1): input-driven transposed-form polyphase.
 *       Each input is multiplied by the current polyphase arm and
 *       accumulated into N integrate-and-dump registers; on NCO
 *       overflow the I&D dump through a transposed tapped delay line
 *       to produce one output.  Bank coefficients are pre-scaled by
 *       rate so the passband gain is unity.
 *
 *   resamp_execute_ctrl — unified input-driven with a double-precision
 *     accumulator that handles all rates and per-sample deviations.
 *     Each input advances the accumulator by (rate + ctrl[i]); every
 *     time the accumulator crosses 1.0 an output is emitted.
 *
 * Phase accumulator (execute): upper log2(num_phases) bits of the
 * 32-bit NCO word index the polyphase bank — nearest-neighbor,
 * no interpolation between branches.
 *
 * Default constructor builds a 4096-phase × 19-tap Kaiser bank
 * (60 dB rejection, 0.4/0.6 pass/stop) at first call.  Use
 * resamp_create_custom() to supply your own bank.
 *
 * Lifecycle:
 * @code
 *   resamp_state_t *r = resamp_create(0.5);
 *   float _Complex out[64];
 *   size_t n = resamp_execute(r, in, 128, out, 64);
 *   resamp_destroy(r);
 * @endcode
 */
#ifndef RESAMP_CORE_H
#define RESAMP_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double rate;
    size_t num_phases;
    size_t num_taps;
    unsigned log2_phases;
    int upsample; /* 1 = rate >= 1.0, 0 = rate < 1.0 */

    float *bank; /* [num_phases][num_taps], row-major  */

    /* execute state */
    uint32_t phase;
    uint32_t phase_inc;

    /* interpolator / execute_ctrl: dual-buffer delay line */
    float _Complex *delay_buf; /* 2 × delay_cap elements         */
    size_t delay_cap;
    size_t delay_mask;
    size_t delay_head;

    /* decimator transposed-form state (execute, rate < 1) */
    float _Complex *decim_iad; /* integrate-and-dump: num_taps   */
    float _Complex *decim_tfd; /* transposed delay line: num_taps-1 */

    /* execute_ctrl state: double-precision fractional accumulator */
    double ctrl_acc;
  } resamp_state_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /** Built-in 4096×19 Kaiser bank (60 dB, 0.4/0.6 pass/stop). */
  resamp_state_t *resamp_create (double rate);

  /** User-supplied bank, shape [num_phases][num_taps], row-major.
   *  num_phases must be a power of two. */
  resamp_state_t *resamp_create_custom (size_t num_phases, size_t num_taps,
                                        const float *bank, double rate);

  /** Free all resources.  NULL is a no-op. */
  void resamp_destroy (resamp_state_t *state);

  /** Zero phase accumulator, ctrl accumulator, and delay line.
   *  Rate and bank are preserved. */
  void resamp_reset (resamp_state_t *state);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  /**
   * @brief Resample a block of CF32 samples (fixed rate).
   *
   * @param state    Must be non-NULL.
   * @param in       Input samples.
   * @param num_in   Number of input samples.
   * @param out      Output buffer.
   * @param max_out  Capacity of out in samples.
   * @return Number of output samples written.
   */
  size_t resamp_execute (resamp_state_t *state, const float _Complex *in,
                         size_t num_in, float _Complex *out, size_t max_out);

  /**
   * @brief Resample with per-sample additive rate deviation.
   *
   * rate_i = base_rate + crealf(ctrl[i]).  ctrl is treated as
   * real-valued; only the real part of each element is used.
   *
   * Output buffer: allocate ceil(num_in × (rate + max_ctrl)) samples.
   *
   * @param state    Must be non-NULL.
   * @param in       Input CF32 samples (length num_in).
   * @param ctrl     Rate deviations, parallel to in (float _Complex,
   *                 real part only, length num_in).
   * @param num_in   Number of input samples (= length of ctrl).
   * @param out      Output buffer.
   * @param max_out  Capacity of out in samples.
   * @return Number of output samples written.
   */
  size_t resamp_execute_ctrl (resamp_state_t *state, const float _Complex *in,
                              const float _Complex *ctrl, size_t num_in,
                              float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double resamp_get_rate (const resamp_state_t *state);

  /** Update rate and recompute phase_inc.  Accumulator phase and delay
   *  line are preserved.  Switching between interp and decim modes
   *  requires a new create() + destroy() pair. */
  void resamp_set_rate (resamp_state_t *state, double rate);

  size_t resamp_get_num_phases (const resamp_state_t *state);
  size_t resamp_get_num_taps (const resamp_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_CORE_H */
