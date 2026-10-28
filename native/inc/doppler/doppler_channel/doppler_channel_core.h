/**
 * @file doppler_channel_core.h
 * @brief Clock Doppler as a propagation impairment: dilate the time base and
 *        shift the carrier, coherently, from one physical parameter.
 *
 * A real Doppler shift is not a frequency offset. Relative motion rescales the
 * whole received time base, so *every* clock in the signal changes together —
 * carrier, chip rate, symbol rate, frame rate. Modelling only the carrier is
 * the classic unphysical shortcut, and it silently hides the code-rate error
 * that a receiver's delay-lock loop exists to track.
 *
 * This object takes any complex baseband stream and applies both halves of the
 * effect from a single parameter, so they cannot disagree:
 *
 *   - **Time-base dilation** — the input is resampled at output/input ratio
 *     `1/(1+d)`, which makes a stream carrying `Rc` chips/s and `Rs` symbols/s
 *     come out at `Rc(1+d)` and `Rs(1+d)`. One resampling on the whole stream,
 *     rather than a per-clock adjustment, is what keeps the clocks consistent.
 *   - **Carrier offset** — multiplication by `exp(j2*pi*fc*excess(t))`, whose
 *     instantaneous frequency is `fc*d(t)`.
 *
 * Doppler is specified in **ppm of the nominal time base**, which makes it
 * carrier-frequency agnostic: 20 ppm is +50 kHz at 2.5 GHz and +61.4 chip/s at
 * 3.069 Mcps at the same time, and no caller converts between the two by hand.
 * `doppler_rate_ppm_s` ramps it linearly for a pass-like geometry (0.2 ppm/s is
 * 500 Hz/s at 2.5 GHz).
 *
 * **`carrier_hz` is load-bearing, not metadata.** It is the only thing that
 * converts a dimensionless ppm into a carrier offset in Hz. Leave it 0 and the
 * clocks still dilate correctly but the carrier never moves — a physically
 * inconsistent capture whose code rate runs fast while its carrier sits exactly
 * on frequency. That combination is occasionally useful for isolating a code
 * loop under test, so it is permitted rather than rejected, but it is not what
 * a real channel does.
 *
 * The dilation is `dp_resamp_execute_ctrl` (see `resamp_core.h`), whose per-sample
 * rate deviation tracks the ramp exactly instead of approximating it with a
 * piecewise-constant ratio re-set once per block. No resampling math is
 * implemented here.
 *
 * **The output is delayed by the resampler's group delay** --
 * dp_doppler_channel_get_delay_samples(), 10.5 samples for the built-in bank --
 * on top of the dilation. Output sample `k`, at receive time `t = k/fs`,
 * carries the input at time `t + excess(t) - delay/fs`. A receiver started at
 * the INPUT's phase is that far from the peak: at two samples per chip, five
 * chips, outside a DLL's pull-in and onto a Gold code's sidelobe
 * (doppler-dsp/doppler#1189). Subtract it, or start the loop there.
 *
 * Lifecycle: create -> `[execute / reset]*` -> destroy
 *
 * Example — a 2.5 GHz carrier seen at +20 ppm, ramping at 0.2 ppm/s:
 * @code
 * dp_doppler_channel_state_t *ch =
 *     dp_doppler_channel_create (6.138e6, 2.5e9, 20.0, 0.2);
 * size_t         cap = dp_doppler_channel_execute_max_out (ch);
 * float _Complex *out = malloc (cap * sizeof *out);
 * size_t         n   = dp_doppler_channel_execute (ch, in, 65536, out, cap);
 * // n ~= 65536/(1+20e-6); dp_doppler_channel_get_offset_hz (ch) ~= 50000.0
 * free (out);
 * dp_doppler_channel_destroy (ch);
 * @endcode
 */
#ifndef DP_DOPPLER_CHANNEL_CORE_H
#define DP_DOPPLER_CHANNEL_CORE_H

#include <stdint.h>

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/resamp/resamp_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief State-blob magic ('DPCH') and layout version. */
#define DOPPLER_CHANNEL_STATE_MAGIC DP_FOURCC('D', 'P', 'C', 'H')
/** @brief State-blob layout version (2 added the profile's last d). */
#define DOPPLER_CHANNEL_STATE_VERSION 2u

/**
 * @brief Largest input block one `dp_doppler_channel_execute()` call accepts.
 *
 * `dp_doppler_channel_execute_max_out()` reports a bound for the output buffer,
 * and the generated Python binding sizes its buffer from that alone — it never
 * sees the input length. So the bound has to assume a worst-case input, and
 * this is that assumption (the same convention, and the same value, as
 * `dp_RateConverter_execute_max_out`). Longer inputs are processed up to the
 * caller's `max_out` and the remainder is *not* consumed; feed large streams in
 * blocks of at most this many samples.
 */
#define DOPPLER_CHANNEL_MAX_BLOCK 65536u

/**
 * @brief DopplerChannel state.
 *
 * Allocate with dp_doppler_channel_create().
 */
typedef struct {
    double fs;                 /**< receive sample rate, Hz (config)       */
    double carrier_hz;         /**< RF carrier fc, Hz — drives the offset
                                    (config; 0 = clocks only)            */
    double doppler_ppm;        /**< d0, ppm of nominal (config)            */
    double doppler_rate_ppm_s; /**< d-dot, ppm/s (config)                  */

    resamp_state_t *rs; /**< owned resampler doing the dilation — never
                             hand-rolled here                      */

    /* Two separate clocks, on purpose. The resampler's per-sample rate
       deviation is indexed by INPUT sample; the carrier phase is a function of
       receive time, which is the OUTPUT sample index. They differ by the
       dilation itself, so conflating them would fold a second copy of the
       Doppler into the carrier. */
    uint64_t n_in;  /**< running: input samples consumed           */
    uint64_t n_out; /**< running: output samples produced          */

    double *ctrl;         /**< owned per-sample rate deviation scratch
                               (not state)                          */
    size_t ctrl_cap;      /**< length of @c ctrl, samples
                               (DOPPLER_CHANNEL_MAX_BLOCK)           */

    /* Profile mode (dp_doppler_channel_execute_profile). The carrier needs no
       state of its own there: it is read off the resampler's position (see
       dp_resamp_execute_ctrl_pos), which the resampler blob and the two
       clocks already carry. What a profile does leave behind is the most
       recent instantaneous d, the one thing the closed form can no longer
       report -- hence these two, and the layout version. */
    double  prof_d;   /**< running: most recent instantaneous d from a
                           profile (dimensionless, not ppm)         */
    uint8_t profiled; /**< running: set by an execute_profile() call
                           that consumes input; cleared by an
                           execute() that consumes input, or reset */

    double *pos;      /**< owned per-output input position scratch
                           (not state); grown on demand          */
    size_t  pos_cap;  /**< length of @c pos, samples              */
    size_t  consumed; /**< input samples the last execute() or
                           execute_profile() took (per-call, not
                           state)                                 */
} dp_doppler_channel_state_t;

/**
 * @brief Excess delay (seconds) accumulated by receive time @p t: `tau(t)-t`.
 *
 * The one place the dilation integral is evaluated; the scale and the carrier
 * phase are both derived from it, so the clocks cannot drift apart — there is
 * only ever one number to drift.
 *
 *   `tau(t) = integral_0^t (1 + d(u)) du`, `d(u) = (d0 + d_dot*u) * 1e-6`
 *   `tau(t) - t = d0*t + 0.5*d_dot*t^2`
 *
 * Note this is the *integral*, not `t*d(t)`: the latter double-counts the ramp
 * and puts the instantaneous offset at `fc*(d0 + 2*d_dot*t)`, exactly twice the
 * intended Doppler rate.
 *
 * @param s  channel state.
 * @param t  receive time in seconds (>= 0).
 * @return Excess delay in seconds (negative for an opening-range geometry).
 */
static inline double
doppler_channel_excess(const dp_doppler_channel_state_t *s, double t)
{
    return (s->doppler_ppm * t + 0.5 * s->doppler_rate_ppm_s * t * t) * 1e-6;
}

/**
 * @brief Instantaneous time-base scale `1 + d(t)` at receive time @p t.
 *
 * The reciprocal of the resampler's output/input ratio: a stream resampled at
 * `1/(1+d)` comes out with every one of its clocks running `(1+d)` times
 * faster.
 *
 * @param s  channel state.
 * @param t  receive time in seconds (>= 0).
 * @return `1 + d(t)`, a number very close to 1.
 */
static inline double
doppler_channel_scale(const dp_doppler_channel_state_t *s, double t)
{
    return 1.0 + (s->doppler_ppm + s->doppler_rate_ppm_s * t) * 1e-6;
}

/**
 * @brief Carrier phase in CYCLES at receive time @p t: `fc * excess(t)`.
 *
 * Its derivative is `fc * d(t)`, the instantaneous offset — so the carrier is
 * driven by the same dilation the clocks are, not by a separately-specified
 * frequency that could be set inconsistently with them.
 *
 * Evaluated closed-form from @p t rather than accumulated per sample: an
 * incremental accumulator would drift, and the closed form keeps a long capture
 * phase-exact (a 1000 s run at 20 ppm on a 2.5 GHz carrier is ~5e7 cycles, ~1e-8
 * cycles of representation error in double).
 *
 * @param s  channel state.
 * @param t  receive time in seconds (>= 0).
 * @return Phase in cycles; multiply by 2*pi for radians.
 */
static inline double
doppler_channel_phase(const dp_doppler_channel_state_t *s, double t)
{
    return s->carrier_hz * doppler_channel_excess(s, t);
}

/**
 * @brief Create a doppler_channel instance.
 *
 * @param fs  Receive sample rate in Hz (> 0).
 * @param carrier_hz  RF carrier in Hz (>= 0). Load-bearing: converts ppm into
 *              a carrier offset. 0 dilates the clocks but never moves the
 *              carrier (default: 0.0).
 * @param doppler_ppm  Doppler d0 in ppm of the nominal time base; positive is
 *              a closing range — clocks run fast, carrier shifts up
 *              (default: 0.0).
 * @param doppler_rate_ppm_s  Linear ramp of d in ppm per second
 *              (default: 0.0).
 * @return Heap-allocated state, or NULL when `fs` is not > 0, the time
 *         scale `1 + doppler_ppm * 1e-6` is not > 0, or the initial ratio is
 *         one dp_resamp_create() refuses (a NaN or infinite @p doppler_ppm
 *         lands there).  Python raises ValueError.
 * @note Caller must call dp_doppler_channel_destroy() when done.
 */
dp_doppler_channel_state_t *dp_doppler_channel_create(double fs, double carrier_hz, double doppler_ppm, double doppler_rate_ppm_s);

/**
 * @brief Destroy a doppler_channel instance and release all memory.
 * @param state  May be NULL.
 */
void dp_doppler_channel_destroy(dp_doppler_channel_state_t *state);

/**
 * @brief Reset DopplerChannel to its post-create state.
 *
 * Zeroes both sample clocks (so `elapsed_s` and the carrier phase restart at
 * zero) and clears the resampler's delay line and fractional accumulator. The
 * configured `fs`/`carrier_hz`/`doppler_ppm`/`doppler_rate_ppm_s` are kept.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> import numpy as np
 * >>> from doppler.impairment import DopplerChannel
 * >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
 * >>> _ = ch.execute(np.ones(1000, dtype=np.complex64))
 * >>> round(ch.elapsed_s, 6)    # receive time consumed: 1000 / 1e6
 * 0.001
 * >>> ch.reset()                # both sample clocks back to zero
 * >>> ch.elapsed_s
 * 0.0
 *
 * @endcode
 */
void dp_doppler_channel_reset(dp_doppler_channel_state_t *state);

/**
 * @brief Bytes dp_doppler_channel_get_state() writes (envelope + payload).
 * @param state  Must be non-NULL.
 * @return Envelope + 3 uint64 + 1 double + the resampler's blob size.
 */
size_t dp_doppler_channel_state_bytes(const dp_doppler_channel_state_t *state);

/** @brief Serialize the running state: both sample clocks, the profile's last
 *         `d` and whether a profile has driven the stream, then the
 *         resampler's.
 *  @param state  Must be non-NULL.
 *  @param[out] blob  Caller-owned buffer of at least
 *                    dp_doppler_channel_state_bytes() bytes. */
void dp_doppler_channel_get_state(const dp_doppler_channel_state_t *state, void *blob);

/**
 * @brief Restore a blob written by dp_doppler_channel_get_state().
 *
 * A refused blob leaves the channel unchanged: the channel's own fields are
 * decoded first and written only once the resampler has accepted its part
 * (doppler#2104).
 * @param state  Must be non-NULL; built with the same configuration as the
 *               blob's source (configuration is not in the blob).
 * @param blob   A blob produced by dp_doppler_channel_get_state().
 * @return DP_OK, or DP_ERR_INVALID if the envelope or a child blob is rejected.
 */
int dp_doppler_channel_set_state(dp_doppler_channel_state_t *state, const void *blob);

/**
 * @brief Upper bound on the output of one execute() call.
 *
 * Assumes an input of at most `DOPPLER_CHANNEL_MAX_BLOCK` samples — see that
 * macro for why the bound cannot depend on the actual input length.  The
 * bound uses the smallest time-base scale over the next block, so with a
 * ramp it changes from call to call; re-query it before each call.
 *
 * @param state  Must be non-NULL.
 * @return `DOPPLER_CHANNEL_MAX_BLOCK / min(1+d) + 2` samples.
 */
size_t dp_doppler_channel_execute_max_out(dp_doppler_channel_state_t *state);

/**
 * @brief Apply clock Doppler to a block of complex baseband.
 *
 * Resamples @p x by `1/(1+d(t))` and multiplies the result by the coherent
 * carrier `exp(j*2*pi*fc*excess(t))`. State persists across calls, so feeding
 * a stream in blocks gives the same samples as one large call (subject to
 * `DOPPLER_CHANNEL_MAX_BLOCK`).
 *
 * Output length is approximately `x_len/(1+d)` and varies by a sample from call
 * to call as the fractional resampling accumulator crosses — that variation is
 * the dilation itself, not a defect.
 *
 * @param state    Must be non-NULL.
 * @param x        Input block.
 * @param x_len    Input length in samples.
 * @param out      Output buffer.
 * @param max_out  Capacity of @p out; production stops there, and the input
 *                 after the last sample taken is not consumed: the input
 *                 clock advances by dp_doppler_channel_consumed(), not by
 *                 @p x_len, so the caller resumes at x + consumed.
 * @return Samples written to @p out.
 * @code
 * >>> import numpy as np
 * >>> from doppler.impairment import DopplerChannel
 * >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9, doppler_ppm=20.0)
 * >>> y = ch.execute(np.ones(1000, dtype=np.complex64))
 * >>> y.shape                   # 20 ppm is 0.02 samples over this block
 * (1000,)
 * >>> round(ch.offset_hz, 1)    # fc * d = 2.5e9 * 20e-6, in Hz
 * 50000.0
 *
 * @endcode
 */
size_t dp_doppler_channel_execute(dp_doppler_channel_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

/**
 * @brief The BINDING's output bound for execute_profile() (jm pass_capacity).
 *
 * The generated binding knows the INPUT LENGTH @p n -- it passes it -- but
 * has not looked at the profile array, so it knows how many samples go in
 * and not how far they dilate. With the profile unseen there is no exact
 * answer: the bound scales @p n by a floor on the scale and the kernel clamps
 * to the caller's real capacity, as `Resampler_execute_ctrl_max_out` does for
 * its equally arbitrary `ctrl`. The floor allows a 2x expansion, i.e. a
 * Doppler of -500000 ppm -- half the speed of light closing, six orders of
 * magnitude past any geometry this object models.
 *
 * @param state  channel state (unused; the signature is jm's).
 * @param n      Input sample count the binding is about to pass.
 * @return The capacity the binding allocates.
 */
size_t dp_doppler_channel_execute_profile_max_out(dp_doppler_channel_state_t *state, size_t n);

/**
 * @brief Apply a per-sample Doppler PROFILE to a block of complex baseband.
 *
 * The array form of dp_doppler_channel_execute(): instead of the create-time
 * `(doppler_ppm, doppler_rate_ppm_s)` closed form -- a straight line, which a
 * real pass is not -- the Doppler is supplied as one value per INPUT sample.
 * The length contract is the one `dp_resamp_execute_ctrl()` underneath already
 * has (`ctrl` parallel to `in`), so a profile is handed to the resampler
 * rather than reduced to fit it.
 *
 * **The profile is ABSOLUTE.** `ppm[i]` is the total instantaneous Doppler at
 * input sample `i`; the create-time scalars do not add to it. They cancel
 * exactly rather than by convention: the resampler's rate is `base + ctrl`,
 * and this fills `ctrl = ratio(ppm[i]) - base` with the same `base` it was
 * built with. Creating with zeros and supplying a profile is the ordinary use.
 *
 * **The carrier is read off the resampler, not integrated beside it.** The
 * excess delay at output `k` is `(p_k - k + 1)/fs`, where `p_k` is the input
 * position the resampler's own accumulator reports for that output
 * (dp_resamp_execute_ctrl_pos()). So the carrier `exp(j*2*pi*fc*excess)` is
 * derived from the dilation the resampler actually performed:
 *
 *   - It cannot disagree with the dilation, and there is no second
 *     accumulator to drift from the first.
 *   - It cannot depend on how the stream was chunked: nothing here maps a
 *     profile index to an output index, which is what made the first attempt
 *     at this (reverted) chunk-dependent. Output `k` has the carrier of the
 *     position it was interpolated at, whatever call it fell in.
 *   - It has no cancellation: the integer part of `p_k - k` is exact, and only
 *     a fraction below one input sample is floating point.
 *
 * Mixing the two calls on one stream is permitted and coherent -- both
 * advance the same clocks -- and dp_doppler_channel_get_offset_hz() reports
 * whichever drove the most recent call: the profile's last value after
 * this, the closed form again after the next dp_doppler_channel_execute(). The profile is in the serialized state (layout
 * version 2) only as that last value; the carrier needs none.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.impairment import DopplerChannel
 * >>> ch = DopplerChannel(fs=1e6, carrier_hz=2.5e9)
 * >>> n = 1000
 * >>> ppm = np.where(np.arange(n) < n // 2, 20.0, -20.0)
 * >>> y = ch.execute_profile(np.ones(n, dtype=np.complex64), ppm)
 * >>> y.shape          # closing then opening: the record STRETCHES overall
 * (1001,)
 * >>> round(ch.offset_hz, 1)   # fc * d at the last profile sample
 * -50000.0
 *
 * @endcode
 *
 * A sign change mid-record is the point: no `(doppler_ppm,
 * doppler_rate_ppm_s)` pair produces it.
 *
 * @param state    channel state.
 * @param x        Input CF32 samples, @p x_len of them.
 * @param x_len    Input sample count.
 * @param ppm      Doppler in ppm, parallel to @p x.
 * @param ppm_len  Profile length; must EQUAL @p x_len.
 * @param out      Output buffer.
 * @param max_out  Capacity of @p out in samples. Production stops there and
 *                 the rest of the input is not consumed:
 *                 dp_doppler_channel_consumed() says how much was, and the
 *                 last instantaneous d is the last consumed sample's
 *                 (doppler#2643). An empty call, or one that consumes
 *                 nothing, changes nothing -- offset_hz still reports what
 *                 drove the stream before it.
 * @return Samples written (>= 0); in Python, the array's length. A refused
 *         call writes nothing, leaves the clocks alone and returns
 *         DP_ERR_INVALID (negative), so a refusal is never mistaken for an
 *         empty answer: 0 is a valid result (a one-sample call returns it
 *         about half the time at +1e6 ppm). Refused: a
 *         NULL pointer, a @p ppm_len that differs from @p x_len, or a profile
 *         sample that is non-finite or below -5e5 ppm (a scale under 1/2:
 *         past the 2x expansion the output is sized for, and at -1e6 ppm
 *         time stops, which create() already refuses for the scalar). All
 *         checked over the whole profile BEFORE any output is produced, so a
 *         bad call writes nothing rather than a valid prefix.
 */
int64_t dp_doppler_channel_execute_profile(dp_doppler_channel_state_t *state, const float _Complex *x, size_t x_len, const double *ppm, size_t ppm_len, float _Complex *out, size_t max_out);

/**
 * @brief Input samples the last dp_doppler_channel_execute() or
 *        dp_doppler_channel_execute_profile() took.
 *
 * Equal to its @p x_len unless @p out filled first; then the caller resumes
 * at x + consumed (and ppm + consumed).  0 after create, reset and a
 * refused call.
 *
 * @param state  Must be non-NULL.
 * @return Input samples taken by the last execute.
 */
size_t dp_doppler_channel_consumed(const dp_doppler_channel_state_t *state);

/**
 * @brief Receive time in seconds produced so far (`n_out/fs`).
 * @param state  Must be non-NULL.
 * @return Seconds of output produced since create or reset.
 */
double dp_doppler_channel_get_elapsed_s(const dp_doppler_channel_state_t *state);

/**
 * @brief Instantaneous carrier offset `fc*d(t)` in Hz at `elapsed_s`.
 *
 * For a stream driven by the closed form, `d(t) = (doppler_ppm +
 * doppler_rate_ppm_s * elapsed_s) * 1e-6`.  After a profile call it is
 * the last profile sample's d instead.  0 whenever carrier_hz is 0.
 *
 * @param state  Must be non-NULL.
 * @return Carrier offset, Hz.
 */
double dp_doppler_channel_get_offset_hz(const dp_doppler_channel_state_t *state);

/**
 * @brief The resampler's group delay, in samples (10.5 for the built-in bank).
 *
 * Constant, and in addition to the dilation: output `k` at receive time
 * `t = k/fs` carries the input at `t + excess(t) - delay/fs`
 * (doppler_channel_excess()). Input and receive samples differ by the ppm of
 * Doppler, so the unit is either to that precision. dp_resamp_get_delay() for
 * the derivation.
 *
 * @code
 * >>> from doppler.impairment import DopplerChannel
 * >>> DopplerChannel(fs=1e6).delay_samples
 * 10.5
 * @endcode
 *
 * @param state  Must be non-NULL.
 * @return Group delay, samples (10.5 for the built-in bank).
 */
double dp_doppler_channel_get_delay_samples(const dp_doppler_channel_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_CHANNEL_CORE_H */
