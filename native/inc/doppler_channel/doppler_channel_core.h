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
 * The dilation is `resamp_execute_ctrl` (see `resamp_core.h`), whose per-sample
 * rate deviation tracks the ramp exactly instead of approximating it with a
 * piecewise-constant ratio re-set once per block. No resampling math is
 * implemented here.
 *
 * Lifecycle: create -> `[execute / reset]*` -> destroy
 *
 * Example — a 2.5 GHz carrier seen at +20 ppm, ramping at 0.2 ppm/s:
 * @code
 * doppler_channel_state_t *ch =
 *     doppler_channel_create (6.138e6, 2.5e9, 20.0, 0.2);
 * size_t         cap = doppler_channel_execute_max_out (ch);
 * float complex *out = malloc (cap * sizeof *out);
 * size_t         n   = doppler_channel_execute (ch, in, 65536, out, cap);
 * // n ~= 65536/(1+20e-6); doppler_channel_get_offset_hz (ch) ~= 50000.0
 * free (out);
 * doppler_channel_destroy (ch);
 * @endcode
 */
#ifndef DOPPLER_CHANNEL_CORE_H
#define DOPPLER_CHANNEL_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "resamp/resamp_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief State-blob magic ('DPCH') and layout version. */
#define DOPPLER_CHANNEL_STATE_MAGIC DP_FOURCC('D', 'P', 'C', 'H')
#define DOPPLER_CHANNEL_STATE_VERSION 2u

/**
 * @brief Largest input block one `doppler_channel_execute()` call accepts.
 *
 * `doppler_channel_execute_max_out()` reports a bound for the output buffer,
 * and the generated Python binding sizes its buffer from that alone — it never
 * sees the input length. So the bound has to assume a worst-case input, and
 * this is that assumption (the same convention, and the same value, as
 * `RateConverter_execute_max_out`). Longer inputs are processed up to the
 * caller's `max_out` and the remainder is *not* consumed; feed large streams in
 * blocks of at most this many samples.
 */
#define DOPPLER_CHANNEL_MAX_BLOCK 65536u

/**
 * @brief DopplerChannel state.
 *
 * Allocate with doppler_channel_create().
 */
typedef struct {
    double fs;                 /* receive sample rate, Hz                  */
    double carrier_hz;         /* RF carrier fc, Hz — drives the offset    */
    double doppler_ppm;        /* d0, ppm of nominal                       */
    double doppler_rate_ppm_s; /* d-dot, ppm/s                             */

    resamp_state_t *rs; /* the dilation — never hand-rolled here     */

    /* Two separate clocks, on purpose. The resampler's per-sample rate
       deviation is indexed by INPUT sample; the carrier phase is a function of
       receive time, which is the OUTPUT sample index. They differ by the
       dilation itself, so conflating them would fold a second copy of the
       Doppler into the carrier. */
    uint64_t n_in;  /* input samples consumed                    */
    uint64_t n_out; /* output samples produced                   */

    double *ctrl;         /* per-sample rate deviation scratch         */
    size_t ctrl_cap;

    /* Profile mode (doppler_channel_execute_profile). A supplied per-sample
       Doppler has no closed form, so the excess-delay integral the carrier
       needs is ACCUMULATED instead of evaluated -- the one thing the closed
       form buys that a profile cannot have. Accumulating `excess` in seconds
       rather than phase in cycles is what keeps that affordable: excess stays
       ~1e-2 s over a long capture while the phase it implies is ~5e7 cycles,
       so the running sum never carries the large magnitude. */
    double excess_s;  /* running tau(t)-t, seconds                 */
    double prof_d;    /* most recent instantaneous d (dimensionless)*/
    uint8_t profiled; /* a profile has driven this stream          */
} doppler_channel_state_t;

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
doppler_channel_excess(const doppler_channel_state_t *s, double t)
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
doppler_channel_scale(const doppler_channel_state_t *s, double t)
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
doppler_channel_phase(const doppler_channel_state_t *s, double t)
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
 * @return Heap-allocated state, or NULL on allocation failure or `fs <= 0`.
 * @note Caller must call doppler_channel_destroy() when done.
 */
doppler_channel_state_t *doppler_channel_create(double fs, double carrier_hz, double doppler_ppm, double doppler_rate_ppm_s);

/**
 * @brief Destroy a doppler_channel instance and release all memory.
 * @param state  May be NULL.
 */
void doppler_channel_destroy(doppler_channel_state_t *state);

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
void doppler_channel_reset(doppler_channel_state_t *state);

/** @brief Bytes doppler_channel_get_state() writes (envelope + payload). */
size_t doppler_channel_state_bytes(const doppler_channel_state_t *state);

/** @brief Serialize the running state (both clocks + the resampler's). */
void doppler_channel_get_state(const doppler_channel_state_t *state, void *blob);

/**
 * @brief Restore a blob written by doppler_channel_get_state().
 * @return DP_OK, or DP_ERR_INVALID if the envelope or a child blob is rejected.
 */
int doppler_channel_set_state(doppler_channel_state_t *state, const void *blob);

/**
 * @brief Upper bound on the output of one execute() call.
 *
 * Assumes an input of at most `DOPPLER_CHANNEL_MAX_BLOCK` samples — see that
 * macro for why the bound cannot depend on the actual input length.
 */
size_t doppler_channel_execute_max_out(doppler_channel_state_t *state);

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
 * @param max_out  Capacity of @p out; production stops there.
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
size_t doppler_channel_execute(doppler_channel_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

/**
 * @brief Upper bound on the output of one execute_profile() call.
 *
 * A caller cannot size the output buffer from doppler_channel_execute_max_out()
 * in profile mode: that bound reads the create-time ramp, and a profile
 * replaces it. Output count is `sum 1/(1+d_i)`, maximised where `d` is
 * smallest, so the bound is `n / (1 + min(d))` plus slack for the
 * resampler's carried accumulator.
 *
 * @param ppm  Profile, @p n samples of Doppler in ppm.
 * @param n    Profile length.
 * @return Samples that execute_profile() can write for this profile, or 0 if
 *         @p ppm is NULL, @p n is 0, or any sample is at or below -1e6 ppm.
 */
size_t doppler_channel_profile_max_out(const double *ppm, size_t n);

/**
 * @brief The BINDING's output bound for execute_profile() (jm pass_capacity).
 *
 * Two bounds, because there are two callers with different knowledge, and
 * collapsing them would make one of them wrong:
 *
 *   - doppler_channel_profile_max_out() is the EXACT bound, for a C caller
 *     that holds the profile and can read its minimum.
 *   - this one is jm's `pass_capacity` contract. The generated binding knows
 *     the INPUT LENGTH -- it passes it -- but has not looked at the profile
 *     array, so it knows how many samples go in and not how far they dilate.
 *
 * With the profile unseen there is no exact answer, so the bound scales the
 * known length by a floor on the scale and the kernel clamps to the caller's
 * real capacity, as Resampler_execute_ctrl_max_out() does for its equally
 * arbitrary `ctrl`. The floor allows a 2x expansion, i.e. a scale of 0.5 or a
 * Doppler of -500000 ppm -- half the speed of light closing, six orders of
 * magnitude past any geometry this object models, so a real profile cannot
 * reach it.
 *
 * @param state  channel state (unused; the signature is jm's).
 * @param n      Input sample count the binding is about to pass.
 * @return The capacity the binding allocates.
 */
size_t doppler_channel_execute_profile_max_out(doppler_channel_state_t *state, size_t n);

/**
 * @brief Apply a per-sample Doppler PROFILE to a block of complex baseband.
 *
 * The array form of doppler_channel_execute(): instead of the create-time
 * `(doppler_ppm, doppler_rate_ppm_s)` closed form -- a straight line, which a
 * real pass is not -- the Doppler is supplied as one value per INPUT sample.
 * That length contract is not a convenience; it is the contract
 * `resamp_execute_ctrl()` underneath already has (`ctrl` parallel to `in`), so
 * a profile is handed to the resampler rather than reduced to fit it.
 *
 * **The profile is ABSOLUTE.** `ppm[i]` is the total instantaneous Doppler at
 * input sample `i`; the create-time scalars do not add to it. They cancel
 * exactly rather than by convention: the resampler's rate is `base + ctrl`,
 * and this fills `ctrl = ratio(ppm[i]) - base` with the same `base` it was
 * built with. Creating with zeros and supplying a profile is the ordinary use.
 *
 * **The carrier is accumulated, not evaluated.** doppler_channel_phase()'s
 * closed form has no counterpart for an arbitrary sequence, so the excess-delay
 * integral is summed as the stream advances (see `excess_s`). Two consequences
 * worth knowing:
 *
 *   - The sum is over OUTPUT samples while the profile is indexed by INPUT
 *     samples, mapped `j*n_in/n_out` within each block. The two clocks differ
 *     by the dilation itself, ~1e-5 relative -- the same approximation
 *     doppler_channel_execute() already takes and documents where it maps an
 *     input index to a receive time.
 *   - It is a running total, so it is part of the serialized state
 *     (`DOPPLER_CHANNEL_STATE_VERSION` 2) and is zeroed by
 *     doppler_channel_reset(), exactly like the two sample clocks.
 *
 * Mixing the two calls on one stream is permitted and coherent -- both advance
 * the same clocks -- but a stream that has ever been driven by a profile
 * reports its diagnostics from the profile, since the closed form no longer
 * describes it. See doppler_channel_get_offset_hz().
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
 * doppler_rate_ppm_s)` pair produces it, because a ramp through those points
 * would have to pass through them in order and keep going.
 *
 * @param state    channel state.
 * @param x        Input CF32 samples, @p x_len of them.
 * @param x_len    Input sample count.
 * @param ppm      Doppler in ppm, parallel to @p x.
 * @param ppm_len  Profile length; must EQUAL @p x_len.
 * @param out      Output buffer.
 * @param max_out  Capacity of @p out in samples.
 * @return Samples written. 0 if any pointer is NULL, if @p ppm_len differs
 *         from @p x_len -- "one value per waveform sample" is the contract,
 *         and a separate length is what lets it be checked rather than
 *         trusted -- or if any profile sample is at or below -1e6 ppm, a
 *         scale of zero or less meaning time stopped or ran backwards, which
 *         create() already refuses for the scalar. All checked over the whole
 *         profile BEFORE any output is produced, so a bad call writes nothing
 *         rather than a valid prefix.
 */
size_t doppler_channel_execute_profile(doppler_channel_state_t *state, const float _Complex *x, size_t x_len, const double *ppm, size_t ppm_len, float _Complex *out, size_t max_out);

/** @brief Receive time in seconds produced so far (`n_out/fs`). */
double doppler_channel_get_elapsed_s(const doppler_channel_state_t *state);

/** @brief Instantaneous carrier offset `fc*d(t)` in Hz at `elapsed_s`. */
double doppler_channel_get_offset_hz(const doppler_channel_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_CHANNEL_CORE_H */
