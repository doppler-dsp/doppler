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
 *     Each input advances the accumulator by (rate + ctrl(i)); every
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
#include "dp_state.h"

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

    float *bank; /* num_phases × num_taps, row-major  */

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

  /** User-supplied bank, shape num_phases × num_taps, row-major.
   *  num_phases must be a power of two. */
  resamp_state_t *resamp_create_custom (size_t num_phases, size_t num_taps,
                                        const float *bank, double rate);

  /** Free all resources.  NULL is a no-op. */
  void resamp_destroy (resamp_state_t *state);

  /** Zero phase accumulator, ctrl accumulator, and delay line.
   *  Rate and bank are preserved. */
  void resamp_reset (resamp_state_t *state);

  /* Serializable state (standard bytes interface; see dp_state.h): after the
   * envelope, the polyphase phase, the fractional ctrl accumulator, the
   * delay-line write head, and the three delay buffers (delay_buf, decim_iad,
   * decim_tfd).  Rate, bank, phase increment and sizes are config. */
#define RESAMP_STATE_MAGIC DP_FOURCC ('R', 'S', 'M', 'P')
#define RESAMP_STATE_VERSION 1u

  /** @brief Bytes resamp_get_state() writes for @p state (envelope + payload). */
  size_t resamp_state_bytes (const resamp_state_t *state);
  /** @brief Serialize @p state's mutable state into @p blob. */
  void resamp_get_state (const resamp_state_t *state, void *blob);
  /** @brief Restore mutable state from @p blob (same rate).
   *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
  int resamp_set_state (resamp_state_t *state, const void *blob);

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
   * rate_i = base_rate + crealf(ctrl(i)).  ctrl is treated as
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
   * Streaming interpolation (fixed integer rate, output-count driven)
   * ------------------------------------------------------------------ */

  /**
   * @brief Input samples an interpolating fill of @p max_out outputs consumes.
   *
   * The exact number of delay-line pushes producing @p max_out outputs from
   * the current phase: `((uint64_t)phase + max_out * phase_inc) >> 32`. For an
   * integer interpolation factor (phase_inc = 2^32 / rate divides evenly, i.e.
   * a power-of-two `num_phases` bank at rate == num_phases) this is exact, so a
   * caller can generate precisely this many inputs — no over- or
   * under-production. Meaningful only for an upsampling resampler (rate >= 1).
   *
   * @param state    Must be non-NULL, upsampling.
   * @param max_out  Number of outputs the following resamp_interp_fill() call
   *                 will request.
   * @return Inputs that call will consume.
   */
  size_t resamp_interp_inputs_needed (const resamp_state_t *state,
                                      size_t max_out);

  /**
   * @brief Emit exactly @p max_out interpolated outputs, pulling inputs on
   * overflow.
   *
   * The output-count-driven twin of the interpolation branch of
   * resamp_execute(): it emits one output per phase tick and pushes the next
   * input on each NCO overflow, but — unlike resamp_execute(), whose loop halts
   * as soon as the input is exhausted even with output capacity left — it always
   * writes @p max_out outputs. The caller must therefore supply at least
   * resamp_interp_inputs_needed(state, max_out) inputs in @p in; supplying
   * exactly that many (the common case) consumes them all. This is what lets a
   * streaming producer (e.g. a pulse-shaping synth) feed symbols on demand and
   * get a bit-exact match between a single call for @p max_out outputs and
   * @p max_out single-output calls (the resampler is block-boundary invariant).
   *
   * @param state    Must be non-NULL, upsampling.
   * @param in       Inputs to push on overflow (>= inputs_needed available).
   * @param out      Output buffer, capacity >= @p max_out.
   * @param max_out  Number of outputs to emit.
   * @return Inputs consumed (== resamp_interp_inputs_needed(state, max_out)).
   */
  size_t resamp_interp_fill (resamp_state_t *state, const float _Complex *in,
                             float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Streaming control port (closed-loop timing / arbitrary rate)
   * ------------------------------------------------------------------ */

  /**
   * @brief Push one input at an instantaneous rate deviation; emit any outputs.
   *
   * The single-input streaming form of resamp_execute_ctrl(): pushes @p x into
   * the delay line, advances the double-precision accumulator by
   * `rate + ctrl`, and emits every output whose accumulator period completes
   * (0 for a decimator between strobes, 1 typically, or several for an
   * interpolator) at the polyphase arm the fractional remainder selects.
   * Feeding a stream of `(x, ctrl)` through this one input at a time reproduces
   * resamp_execute_ctrl() on the same `(in, ctrl[])` bit-for-bit — but, unlike
   * the block form's precomputed `ctrl[]`, `ctrl` here can depend on the
   * outputs already emitted. That closes the loop: a timing-recovery or
   * rate-tracking loop reads each emitted output, computes its correction, and
   * feeds it back as the next call's @p ctrl to steer the strobe. This is the
   * per-output feedback a matched-filter timing loop (track.RrcSync) needs and
   * the block `execute_ctrl` cannot provide.
   *
   * @param state    Must be non-NULL.
   * @param x        One input sample.
   * @param ctrl     Rate deviation added to the base rate for this input
   *                 (real-valued; the effective rate is `rate + ctrl`).
   * @param out      Output buffer for any emitted samples.
   * @param max_out  Capacity of @p out (emission stops at this bound).
   * @return Number of outputs emitted into @p out (0, 1, or more).
   */
  size_t resamp_execute_ctrl_push (resamp_state_t *state, float _Complex x,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

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

  /** @brief The control accumulator's fractional phase, in [0, 1).
   *
   *  This is the timing NCO's state, and observing it is the only way to see
   *  what a closed timing loop is actually doing to the sampling instant: the
   *  arm the last output read is `floor(mu * num_phases)`, so `mu` IS the
   *  fractional delay applied to the stream, in output periods. A steady `mu`
   *  means the loop has settled on a sampling phase; a `mu` that slews and
   *  wraps means a residual RATE error the loop has not absorbed, and one
   *  cycle of wrap is one output period of slip.
   *
   *  Reported after the last output this stage emitted, which for a decimating
   *  terminal stage (`rate <= 1`) is the phase the next output will read from.
   */
  double resamp_get_ctrl_acc (const resamp_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_CORE_H */
