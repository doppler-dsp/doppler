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
 *   resamp_execute_ctrl — unified, and it rides the INTERPOLATOR at every
 *     rate: emit at every tick, and load an input when the accumulator
 *     fails to advance (`u(k) <= u(k-1)`). The steered rate `rate +
 *     ctrl(i)` sets the step; whole input intervals per output are owed
 *     as `ctrl_debt` and the fractional remainder selects the arm.
 *
 *     This paragraph used to describe the other structure — "each input
 *     advances the accumulator by (rate + ctrl(i)); every time the
 *     accumulator crosses 1.0 an output is emitted" — which is the
 *     DECIMATOR's recurrence, exact only at rate 1 where the two coincide.
 *     Running it here cost 55-60 dB of tone purity at every other rate.
 *     The description is kept accurate rather than deleted because it is
 *     the mental model under which someone writes a private `(uint32_t)
 *     (frac * 2^32 + 0.5)` and believes it correct; one did, and it
 *     stalled the interpolator for composite rates just above unity.
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

    /* execute_ctrl state: the INTERPOLATOR's accumulator.  ctrl_phase is
       the position within the current input interval -- a uint32 fraction
       of one interval, which is exactly what a polyphase arm indexes.
       ctrl_debt is the whole intervals still owed before the next output
       may fire; it is non-zero only when the steered rate drops below
       unity, where one output spans more than one input. */
    uint32_t ctrl_phase;
    /* Inputs the accumulator has REQUESTED and not yet been given.  No
       sample enters the delay line without one of these: the push form
       holds the offered sample until a tick asks for it, exactly as the
       block form only ever loads inside its load branch. */
    uint32_t ctrl_debt;
    /* Inputs given WITHOUT a request, because max_out ended the call before
       a tick could ask.  The caller's sample has to go somewhere -- the API
       has no way to decline it -- so it is loaded and remembered here, and
       the next request is satisfied from it instead of from a new sample. */
    uint32_t ctrl_ahead;
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
   * decim_tfd).  Rate, bank, phase increment and sizes are config.
   *
   * VERSION 2: the ctrl accumulator is a uint32 phase word plus a uint32
   * load debt, not a float64 in [0, 1) -- a different width and encoding,
   * so a v1 blob is rejected by the envelope rather than misread. */
  /* Floor on the composite rate `rate + ctrl`.  Not a policy about what the
   * bank filters well -- it is what keeps the reciprocal in
   * resamp_execute_ctrl_push() defined.  Small enough that no real steer
   * reaches it. */
#define RESAMP_CTRL_RATE_MIN 1e-6

#define RESAMP_STATE_MAGIC DP_FOURCC ('R', 'S', 'M', 'P')
#define RESAMP_STATE_VERSION 2u

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
   * rate_i = base_rate + `ctrl[i]`.    The control is real-valued and
   * double-precision, matching resamp_execute_ctrl_push()'s scalar `ctrl`
   * and the `double` the base rate itself is configured in.
   *
   * Output buffer: allocate ceil(num_in × (rate + max_ctrl)) samples.
   *
   * @param state    Must be non-NULL.
   * @param in       Input CF32 samples (length num_in).
   * @param ctrl     Rate deviations, parallel to in (length num_in).
   * @param num_in   Number of input samples (= length of ctrl).
   * @param out      Output buffer.
   * @param max_out  Capacity of out in samples.
   * @return Number of output samples written.
   */
  size_t resamp_execute_ctrl (resamp_state_t *state, const float _Complex *in,
                              const double *ctrl, size_t num_in,
                              float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Streaming interpolation (fixed integer rate, output-count driven)
   * ------------------------------------------------------------------ */

  /**
   * @brief Input samples an interpolating fill of @p max_out outputs consumes.
   *
   * The exact number of delay-line pushes producing @p max_out outputs from
   * the current phase: `((uint64_t)phase + max_out * phase_inc) >> 32`.
   *
   * Exact at EVERY rate, not only at an integer interpolation factor, and a
   * caller may rely on it: generate precisely this many inputs, hand them to
   * resamp_interp_fill(), and there is no over- or under-production. The
   * guarantee is structural rather than numeric — this closed form and the
   * fill loop advance the same accumulator by the same `phase_inc`, so they
   * cannot disagree whatever the rate, and mid-stream phase is carried in
   * the formula. Measured across 1800 mid-stream calls at nine rates with
   * randomised @p max_out, worst deviation zero (test_resamp_core.c §20).
   *
   * (This paragraph used to scope the promise to "an integer interpolation
   * factor". That is where phase_inc divides evenly and therefore represents
   * the rate exactly — a real property, but a different one: the prediction
   * matches the fill because both USE phase_inc, not because it is exact.)
   *
   * Meaningful only for an upsampling resampler (rate >= 1) — the prediction
   * still matches what resamp_interp_fill() consumes below unity, but that
   * entry point interpolates, so a decimating caller wants resamp_execute().
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
   * The single-input streaming form of resamp_execute_ctrl(): OFFERS @p x to
   * the delay line, advances the accumulator by `rate + ctrl`, and emits
   * every output whose period completes (0 for a decimator between strobes,
   * 1 typically, or several for an interpolator) at the polyphase arm the
   * fractional remainder selects.
   *
   * **The scalar and block forms are INDISTINGUISHABLE.** Feeding a stream
   * one sample at a time through here yields the same outputs, in the same
   * number, bit-for-bit, as one resamp_execute_ctrl() over the same
   * `(in, ctrl[])`. Not "close" and not "one sample of delay apart": the
   * same. A caller chooses between them for control flow — the block form
   * when `ctrl[]` is known in advance, this one when each correction
   * depends on the outputs already emitted — never for a difference in what
   * comes out.
   *
   * That is what "offers" buys, and why @p x is NOT pushed on entry:
   * nothing enters an interpolator's delay line without a load REQUEST — a
   * tick emits, the accumulator fails to advance, and only then is an input
   * consumed. @p x is held until a tick asks. Pushing on entry is an
   * unrequested load, and it is precisely what broke the invariant, costing
   * exactly one sample of group delay against the block form. Fixed, and
   * gated: `eq_ctrl_push` in test_resamp_core.c asserts bit-exact equality
   * across decimating, unity-neighbourhood and interpolating rates, at zero
   * and at both signs of steer.
   *
   * `ctrl_ahead` covers the one case the API cannot decline — @p max_out
   * ending the call before any tick could ask for the offered sample.
   * Feeding a stream of `(x, ctrl)` through this one input at a time reproduces
   * resamp_execute_ctrl() on the same `(in, ctrl[])` bit-for-bit — but, unlike
   * the block form's precomputed `ctrl[]`, `ctrl` here can depend on the
   * outputs already emitted. That closes the loop: a timing-recovery or
   * rate-tracking loop reads each emitted output, computes its correction, and
   * feeds it back as the next call's @p ctrl to steer the strobe. This is the
   * per-output feedback a matched-filter timing loop (track.RateSync) needs and
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
   *  what a closed timing loop is actually doing to the sampling instant.
   *  `mu` IS the fractional delay applied to the stream, and
   *  `floor(mu * num_phases)` is the polyphase arm the **next** output will
   *  read — the accumulator advances after the emit, so on return it already
   *  describes the output still to come. That holds at EVERY rate, because
   *  the control port rides the interpolating structure at every rate (see
   *  resamp_execute_ctrl); it is not a peculiarity of a decimating stage.
   *
   *  A steady `mu` means the loop has settled on a sampling phase. A `mu`
   *  that slews and wraps means a residual RATE error the loop has not
   *  absorbed, and one cycle of wrap is one INPUT interval of slip — an
   *  output period only at rate 1, where the two coincide.
   *
   *  Reports the CONTROL accumulator, so it stays 0.0 for a caller driving
   *  this object through resamp_execute(): the free-running phase is a
   *  separate accumulator with no accessor.
   *
   *  Pinned by test_resamp_core.c §10 (the arm, read off the output at four
   *  rates), §11 (the wrap's unit, against the counting law) and §12.
   */
  double resamp_get_ctrl_acc (const resamp_state_t *state);

  /**
   * @brief The resampler's response to a constant input, from its own bank.
   *
   * Every arm of a polyphase bank is the same filter at a different
   * fractional delay, so they share one DC gain and arm 0 answers for all of
   * them: the sum of its taps. Computed, not measured — a caller (or a gate)
   * can ask what gain this stage contributes without running a signal
   * through it.
   *
   * The decimating path pre-scales by `rate` and integrates over the whole
   * bank between outputs, which cancels: `rate` inputs' worth of taps per
   * output, so the tap sum is the answer on both paths.
   *
   * ARM 0's gain, which is the realised gain only where arm 0 is the arm
   * being used — at rate 1, where the fraction is zero and one arm is
   * selected forever. A non-unity rate visits every arm, so what a caller
   * measures is the arm AVERAGE: 1.000586 computed against 1.000293 at
   * rate 0.5 and 2, and 1.000249 at 0.25 and 4. The 3.4e-4 spread is the
   * bank's arm-to-arm ripple and of no practical consequence, but this
   * returns the computed number, not the one a measurement will find.
   * Pinned by test_resamp_core.c §13, which checks both.
   *
   * @param state State. Must be non-NULL.
   * @return The DC gain. 1.0 for the default Kaiser bank; a matched pulse
   *         bank is a matched filter, not a flat one, so its DC gain is the
   *         pulse's own `sum(h)/sum(h^2)` and is not expected to be 1.
   *
   * @code
   * resamp_state_t *r = resamp_create (0.5);
   * printf ("%.3f\n", resamp_dc_gain (r));   // 1.000
   * resamp_destroy (r);
   * @endcode
   */
  double resamp_dc_gain (const resamp_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_CORE_H */
