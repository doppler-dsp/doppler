/**
 * @file state_filter_core.h
 * @brief A filter over an explicit state vector (the frequency estimate and
 *        its rate): the fixed-gain third-order tracking loop.
 *
 * An error @c e in, a control value out, with two accumulators in cascade:
 *
 *     rate  += kr * e;            the rate estimate
 *     integ += rate + ki * e;     the frequency estimate
 *     control = integ + kp * e;
 *
 * so `F(z) = kp + ki/(1 - z^-1) + kr/(1 - z^-1)^2`. @c integ keeps the
 * meaning it has in dp_loop_filter_state_t (the running frequency estimate,
 * in the caller's control units per update); @c rate is its slope, in the
 * same units per update. A loop with this filter has no steady phase error
 * on a frequency ramp, where the second-order loop's error is `r/ki`.
 *
 * ### The caller gives `bn` and `t`, and gets a stable loop
 *
 * The three gains come from a loop noise bandwidth @c bn (cycles per
 * sample) and an update period @c t (samples), with no other coefficient
 * to pick. The loop's shape is fixed: the analog prototype
 * `s^3 + b*w0*s^2 + a*w0^2*s + w0^3` with @c STATE_FILTER_SHAPE_A and
 * @c STATE_FILTER_SHAPE_B (a Butterworth placement) has the gains
 * `kp = b*th`, `ki = a*th^2`, `kr = th^3`. The scale `th` is SOLVED so that
 * the exact noise bandwidth of the discrete loop equals `bn*t`: the delivered
 * bandwidth is the one asked for at a detector delay of zero, with no
 * excess to correct for. The derivation is docs/design/loop-filter.md
 * section 11.1.
 *
 * ### What it needs of the rest of the loop
 *
 * Like the second-order filter, the promise assumes a unit-gain loop: the
 * contract is stated on dp_state_filter_create_why() (and so on the Python
 * class's docstring) and in docs/design/loop-filter.md section 3. A
 * third-order loop is
 * CONDITIONALLY stable in the loop gain @c K (the discriminator slope
 * times the oscillator gain): it needs `K` above a lower edge near
 * `1/(a*b)` of nominal, and the sampled loop has an upper edge that falls
 * with the detector's delay. dp_state_filter_create() accepts a `bn*t` up to
 * STATE_FILTER_BNT_MAX, the largest for which the loop is stable at every
 * gain from STATE_FILTER_K_MIN to STATE_FILTER_K_MAX and every detector delay
 * up to STATE_FILTER_DELAY_MAX updates. The object cannot see the gain or
 * the delay: a discriminator whose slope is below the lower edge breaks the
 * loop with no error reported. dp_state_filter_stable() and
 * dp_state_filter_bandwidth() answer for a gain and delay you know.
 *
 * The state struct is public so a tracker can embed it by value, as with
 * dp_loop_filter_state_t, and drive it with dp_state_filter_init() and the
 * inline dp_state_filter_step().
 *
 * Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`
 *
 * @code
 * dp_state_filter_state_t *sf = dp_state_filter_create(0.01, 1.0);
 * double ctl = dp_state_filter_step(sf, 0.25);
 * dp_state_filter_destroy(sf);
 * @endcode
 */
#ifndef DP_STATE_FILTER_CORE_H
#define DP_STATE_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include <stdbool.h>
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief The shape of the loop: the analog prototype is
 *        `s^3 + B*w0*s^2 + A*w0^2*s + w0^3`.
 *
 * Both constants live here and nowhere else; a different placement is a
 * change of these two numbers and a rerun of the validator. `A = B = 2` is
 * the third-order Butterworth polynomial `(s+1)(s^2+s+1)`, whose lower
 * gain edge is `1/(A*B) = 0.25` of nominal. The noise bandwidth of the
 * prototype is `w0 * (A*B^2 + A^2 - B) / (4*(A*B - 1))`.
 */
#define STATE_FILTER_SHAPE_A 2.0
/** @brief See STATE_FILTER_SHAPE_A. */
#define STATE_FILTER_SHAPE_B 2.0

/**
 * @brief The stability rectangle the automatic case is good for.
 *
 * The loop is stable at every loop gain from STATE_FILTER_K_MIN to
 * STATE_FILTER_K_MAX times nominal and every detector delay from
 * 0 to STATE_FILTER_DELAY_MAX updates (exactly at an integer delay; between
 * integers under the interpolation model of dp_state_filter_loop_stable()),
 * for `bn*t` up to STATE_FILTER_BNT_MAX. `K_MIN` is the floor the automatic case covers for
 * a detector whose slope falls with signal-to-noise ratio: a requirement, with
 * no measured source of that slope on main yet (the async receiver's pull-in
 * validator will print one, #3135); `DELAY_MAX` covers the delay a
 * detector that updates on a sample grid sweeps through. The bound is the
 * largest `bn*t` for which the exact stability test passes at the upper
 * corner (`K_MAX`, `DELAY_MAX`), rounded down; the C test derives it from
 * dp_state_filter_loop_stable() rather than restating it.
 *
 * What `K_MIN` constrains is the claim, not the predicate: the lower gain
 * edge is set by the shape (about `1/(A*B)` of nominal, a little higher
 * with delay), so no `bn*t` bound can move it, and the predicate bounds
 * `bn*t` from the upper corner alone. The C test asserts that the measured
 * lower edge at the bound, at every delay up to `DELAY_MAX`, is below
 * `K_MIN`, and prints the margin. At the bound that edge is 0.236 of
 * nominal at a delay of zero and 0.310 at `DELAY_MAX`, 4.1 dB under `K_MIN`.
 */
#define STATE_FILTER_K_MIN 0.5
/** @brief See STATE_FILTER_K_MIN. */
#define STATE_FILTER_K_MAX 2.0
/** @brief See STATE_FILTER_K_MIN. */
#define STATE_FILTER_DELAY_MAX 2.0
/** @brief See STATE_FILTER_K_MIN. */
#define STATE_FILTER_BNT_MAX 0.12

/**
 * @brief The smallest nonzero `bn*t` the filter accepts.
 *
 * Below it the gains `th`, `a*th^2` and `th^3` approach the underflow of a
 * double (`th^3` reaches zero near `th = 1e-108`), where the closed form the
 * gains are solved from is 0/0. `bn = 0` is not below it: it is the frozen
 * loop, with all three gains zero.
 */
#define STATE_FILTER_BNT_MIN 1e-30

/** @brief The largest detector delay, in updates, the exact functions take. */
#define STATE_FILTER_DELAY_LIMIT 4.0

/**
 * @brief Third-order tracking-loop filter state (embeddable by value).
 */
typedef struct
{
  double kp;    /**< proportional gain (from bn, t).                    */
  double ki;    /**< frequency-path gain (from bn, t).                  */
  double kr;    /**< rate-path gain (from bn, t).                       */
  double integ; /**< frequency estimate, control units per update.      */
  double rate;  /**< slope of the frequency estimate: control units per
                     update per update (a Doppler rate R in Hz/s at an update
                     period T in seconds is R * T^2 cycles per update per
                     update).                                             */
  double bn;    /**< loop noise bandwidth, normalized cycles/sample.    */
  double t;     /**< update period in samples.                          */
} dp_state_filter_state_t;

/**
 * @brief The domain the loop is defined on: `bn >= 0`, `t > 0`, both
 *        finite, and `bn*t` zero or between STATE_FILTER_BNT_MIN and
 *        STATE_FILTER_BNT_MAX.
 *
 * The ONE predicate; dp_state_filter_create() and
 * dp_state_filter_configure() refuse outside it, and an embedder that takes
 * these numbers from a caller calls it before the unguarded
 * dp_state_filter_init().
 *
 * @param bn  Loop noise bandwidth, normalized cycles/sample.
 * @param t   Update period, in samples.
 * @return    1 inside the domain, 0 outside it (a NaN is outside).
 */
int dp_state_filter_params_ok (double bn, double t);

/**
 * @brief Initialise a filter in place (no allocation).
 *
 * Computes the gains for @p bn and @p t and stores both. Does **not** touch
 * @c integ or @c rate, so it doubles as a reconfigure that preserves lock.
 * Arguments are NOT validated here, on purpose, as in
 * dp_loop_filter_init(): the embedder checks dp_state_filter_params_ok()
 * first. An argument outside it gives gains no stability claim is made for
 * (past STATE_FILTER_BNT_MAX the corner of the rectangle goes unstable, but
 * only well past it: the exact bound is about 0.1248).
 *
 * @param state  Must be non-NULL.
 * @param bn     Loop noise bandwidth, normalized cycles/sample.
 * @param t      Update period in samples.
 */
void dp_state_filter_init (dp_state_filter_state_t *state, double bn,
                           double t);

/**
 * @brief Whether a stored filter is one a live filter could hold.
 *
 * For restoring a serialized filter, whose gains are taken verbatim: the
 * filter must be inside dp_state_filter_params_ok() at its own @c bn and
 * @c t, and its gains must match the ones dp_state_filter_init() writes there
 * to a relative tolerance of 1e-9. @c integ and @c rate are not checked.
 *
 * The 1e-9 is an allowance between platforms, and what is known of the
 * spread is this: the test holds the gains within 1e-12 of a long double
 * solve of the same closed form on every platform whose long double is wider
 * than double (x86-64 Linux, 80-bit; arm64 Linux, 128-bit), so two such
 * platforms agree within 2e-12 and the tolerance is 500 times that. macOS
 * and Windows have no wider reference and are not covered by the number; the
 * tolerance is to be tightened to a stated multiple once they have one.
 *
 * @param s  The stored filter.
 * @return 1 if it is one a live filter could hold, 0 otherwise.
 */
int dp_state_filter_state_ok (const dp_state_filter_state_t *s);

/**
 * @brief Create a state filter, validating its arguments. The binding calls
 *        dp_state_filter_create_why(), which also says why it refused.
 *
 * @param bn  Loop noise bandwidth, normalized cycles/sample; >= 0
 *            (default 0.01). `bn = 0` is a deliberately frozen loop.
 * @param t   Update period in samples; > 0 (default 1.0).
 * @return Heap-allocated state, or NULL if `(bn, t)` is outside
 *         dp_state_filter_params_ok() or on allocation failure.
 * @note Caller must call dp_state_filter_destroy() when done.
 */
dp_state_filter_state_t *dp_state_filter_create (double bn, double t);

/**
 * @brief The reason (@p bn, @p t) is outside the domain, naming the argument,
 *        or NULL when it is inside.
 *
 * The same predicate as dp_state_filter_params_ok(), with a sentence for the
 * first condition that fails: `t` not positive and finite, `bn` NaN, `bn`
 * negative, `bn` infinite, or `bn*t` under the floor or over the bound. The
 * strings are static.
 *
 * @param bn  Loop noise bandwidth.
 * @param t   Update period in samples.
 * @return A static sentence, or NULL if the pair is inside the domain.
 */
const char *dp_state_filter_params_why (double bn, double t);

/**
 * @brief Create a third-order state filter whose closed loop has noise
 *        bandwidth `bn`, for a loop whose rest has unit gain: the one
 *        constructor the Python class `StateFilter` calls.
 *
 * ### The contract
 *
 * The closed loop's noise bandwidth is `bn` cycles per sample, a fixed
 * function of `kp`, `ki` and `kr`, provided the rest of the loop has unit
 * gain. The filter cannot see the rest of the loop, so each of these is the
 * caller's to deliver, and each is checkable:
 *
 * 1. The error detector's slope at lock is 1 per unit of the filter's error
 *    input, open loop and noise free, in the unit the control is applied in
 *    per update; the radians-to-cycles factor, where the oscillator takes
 *    cycles, is applied once after the filter, never inside the loop gain.
 * 2. The actuator applies the control with unit gain, once per update, `t`
 *    samples apart.
 * 3. The loop is tracking, inside the detector's linear region.
 * 4. The error reaches the filter and the control the actuator within the
 *    same update: the delivered bandwidth is the one asked for at a delay of
 *    zero.
 *
 * A loop gain K other than 1 changes the delivered bandwidth, and this
 * third-order loop is also stable only for K above a lower edge near
 * `1/(a*b)` of nominal and below an upper edge that falls with the delay
 * (docs/design/loop-filter.md section 12), so a detector slope below that
 * edge breaks the loop with no error reported. The second-order closed form
 * `(4*K*zeta^2 + 1) / (4*zeta^2 + 1)` does not apply to it. A detector whose
 * slope is not 1, or rides on signal level or SNR, is scaled to unit slope
 * before the filter; a slope that rides on level makes the bandwidth ride on
 * level.
 *
 * Held by the state filter's own tests for the promise,
 * `test_loop_filter_contract.py` for these sentences on every face, and the
 * detector matrix of doppler#3229 for the callers' side.
 *
 * ### Arguments
 *
 * @param bn   Loop noise bandwidth, normalized cycles/sample, of the closed
 *             loop given a unit-slope detector (see the contract); >= 0
 *             (default 0.01). `bn = 0` is a deliberately frozen loop.
 * @param t    Update period in samples; > 0 (default 1.0).
 * @param why  Where the refusal's sentence goes, naming the argument it
 *             refused (dp_state_filter_params_why()); set to NULL when the
 *             filter is built. May be NULL.
 * @return Heap-allocated state, or NULL (with @p why set) outside the
 *         domain or on allocation failure.
 * @note Caller must call dp_state_filter_destroy() when done.
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.01, t=1.0)
 * >>> round(sf.kp, 6), round(sf.ki, 6)   # the gains follow from bn and t
 * (0.023622, 0.000279)
 * >>> sf.integ, sf.rate                  # both estimates start at zero
 * (0.0, 0.0)
 * >>> StateFilter(bn=0.5)                # bn*t past the stability bound
 * Traceback (most recent call last):
 *     ...
 * ValueError: bn*t is above the largest the filter accepts, the bound the third-order loop is stable to
 *
 * @endcode
 */
dp_state_filter_state_t *dp_state_filter_create_why (double bn, double t,
                                                     const char **why);

/**
 * @brief Destroy a state filter and release all memory.
 * @param state  May be NULL.
 */
void dp_state_filter_destroy (dp_state_filter_state_t *state);

/**
 * @brief Retune the gains for a new `(bn, t)` without disturbing the
 *        frequency and rate estimates.
 *
 * As dp_loop_filter_configure() does for the second-order filter: widen to
 * acquire, narrow to track, holding the estimates. A `(bn, t)` outside
 * dp_state_filter_params_ok() is refused and changes nothing.
 *
 * @param state  Must be non-NULL.
 * @param bn     Loop noise bandwidth, normalized cycles/sample.
 * @param t      Update period in samples.
 * @return `DP_OK`, or `DP_ERR_INVALID` outside the domain.
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.01, t=1.0)
 * >>> _ = sf.step(1.0)
 * >>> before = sf.rate
 * >>> sf.configure(0.05, 1.0)            # widen the loop, keep lock
 * >>> sf.rate == before                  # the estimates are preserved
 * True
 * >>> round(sf.kp, 6)                    # the gains rose
 * 0.111058
 *
 * @endcode
 */
int dp_state_filter_configure (dp_state_filter_state_t *state, double bn,
                               double t);

/**
 * @brief Zero the frequency and rate estimates, keeping the gains.
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.02, t=1.0)
 * >>> for _ in range(10):
 * ...     _ = sf.step(1.0)               # open loop: both estimates ramp
 * >>> round(sf.integ, 6), round(sf.rate, 6)
 * (0.011506, 0.000126)
 * >>> sf.reset()
 * >>> sf.integ, sf.rate                  # cleared, gains kept
 * (0.0, 0.0)
 *
 * @endcode
 */
void dp_state_filter_reset (dp_state_filter_state_t *state);

/**
 * @brief Whether a loop of this order with these gains is stable.
 *
 * The exact test on the characteristic polynomial of the discrete loop of
 * docs/design/loop-filter.md section 11.1: a unit detector, the filter, an
 * oscillator that accumulates the control, a detector delay of @p delay
 * updates, and a loop gain of @p gain times nominal. The one function for
 * both orders: @p order 2 is dp_loop_filter_state_t's recurrence
 * (@p kr is ignored), @p order 3 this object's.
 *
 * At an integer @p delay the delay is a real transport delay and the answer
 * is exact. Between integers the model is linear interpolation of the
 * detector's error between the two nearest updates, which low-passes the
 * detector, a different model from a pure fractional delay, whose stability
 * this function does not state; the rectangle's claim is exact at integer
 * delays and holds under this model between them. A detector that averages
 * over a window is a third model the caller owns.
 *
 * The test is whether the sum `P = sum_k A^k b b^T (A^T)^k` converges (below),
 * which is stability only when the loop's state is driven (the pair (A, b) is
 * controllable): `P` is then the solution of the loop's Lyapunov equation,
 * and it exists exactly when every pole is inside the unit circle. Where the
 * state is not driven, the answer is 0: a gain at zero
 * leaves a state undriven or marginal, so the frozen loop (`bn = 0`, every
 * gain zero), an order-3 loop with `kr = 0` and an order-2 loop with `ki = 0`
 * (a pole-zero cancellation at `z = 1`) all read as not stable, and
 * dp_state_filter_loop_bandwidth() is NaN for them.
 *
 * Both answers come from one computation in `double` on every platform: the
 * loop's state matrix `A` is balanced by a power-of-two diagonal similarity
 * and then squared repeatedly while `P = sum_k A^k b b^T (A^T)^k` accumulates
 * by `P <- P + A P A^T`, `A <- A A`. Each term of the sum is positive
 * semidefinite, so the sum cannot lose definiteness by cancellation (an
 * individual entry may be negative); the bandwidth is `(d^2 + c P c^T) / 2`
 * of that `P`.
 *
 * The decision rule is exact: after each pass the largest absolute entry of
 * the balanced `A^(2^j)` is read. Below 1e-12 the loop is stable and the sum
 * is complete (every later term is under 1e-24 of the last). Non-finite or
 * above 1e150 the loop is not stable, and the function returns 0 there, before
 * overflow can make any entry NaN. After 64 passes without either, the loop is
 * not stable (a pole at radius `1 - delta` is decided in about
 * `log2 (40/delta)` passes, so the cap reaches `delta` near 1e-17). A pole
 * within about 1e-12 of the unit circle is within rounding of it, and the
 * answer there is not guaranteed either way; a marginal loop (a gain at zero,
 * a pole at `z = 1`) never goes to zero and reads as not stable.
 *
 * What the tests run, and what they do not: the exact bandwidth at delay 0 and
 * loop gain 1 against the bandwidth asked for, from `bn*t` 1e-6 to
 * STATE_FILTER_BNT_MAX (1203 cells, within 1e-9; not claimed below 1e-6,
 * which is above STATE_FILTER_BNT_MIN); the verdict and the bandwidth against
 * a simulation of the real step at one design loop (`bn*t` 0.03) for delays
 * 0, 0.5, 1, 1.25, 2 and 3 and loop gains 1, 0.7 and 1.6; loops built at a
 * known radius either side of 1 (1e-6 and 1e-9); and gains off the design
 * manifold (a Kalman filter's gains while its covariance converges, `bn*t`
 * 1e-3 to 0.08) against a simulation of the loop's own state recursion. The
 * full sweep (every `bn*t` from the floor to the bound, delays 0 to 4, loop
 * gains 0.5 to 2) is not run.
 *
 * @param order  2 or 3.
 * @param kp     Proportional gain.
 * @param ki     Frequency-path gain.
 * @param kr     Rate-path gain (order 3).
 * @param delay  Detector delay in updates, from 0 to STATE_FILTER_DELAY_LIMIT.
 * @param gain   Loop gain as a multiple of nominal, > 0.
 * @return 1 if every pole is strictly inside the unit circle, 0 if not, if
 *         the loop is not controllable, or if an argument is outside its
 *         domain.
 */
int dp_state_filter_loop_stable (int order, double kp, double ki, double kr,
                                 double delay, double gain);

/**
 * @brief The exact delivered noise bandwidth of that loop, in cycles per
 *        update: `Bn = (1/2) * sum h[n]^2` of its impulse response.
 *        The phase is the oscillator's at the detector instant, the phase the
 *        error is formed from; the phase just after the proportional step of
 *        the same update has a smaller sum of squares (design page section
 *        11.1, which gives the ratio).
 *
 * Solved exactly (a discrete Lyapunov equation), not summed. For order 2 at
 * `delay = 0` and `gain = 1` it equals
 * `(2 kp^2 + (2 + kp) ki) / (2 kp (4 - 2 kp - ki))`. The delay and gain
 * model are those of dp_state_filter_loop_stable(). The solve is on the
 * loop's own state space, where a slow loop is a matrix near the identity,
 * so it holds a relative accuracy near 1e-10 for gains of a loop of
 * `bn*t` down to 1e-6; it is not claimed below that.
 *
 * @param order  2 or 3.
 * @param kp     Proportional gain.
 * @param ki     Frequency-path gain.
 * @param kr     Rate-path gain (order 3).
 * @param delay  Detector delay in updates, from 0 to STATE_FILTER_DELAY_LIMIT.
 * @param gain   Loop gain as a multiple of nominal, > 0.
 * @return The bandwidth, or NaN if the loop is not stable there or an
 *         argument is outside its domain.
 */
double dp_state_filter_loop_bandwidth (int order, double kp, double ki,
                                       double kr, double delay, double gain);

/**
 * @brief The steady phase error of a loop of this order to a polynomial
 *        input, from the final-value theorem on the discrete loop.
 *
 * @p input_order 1 is a frequency step, 2 a frequency ramp, 3 a frequency
 * acceleration; @p x is the input's size in cycles per update raised to
 * its order (a ramp of `R` Hz/s at an update period of `T` seconds is
 * `x = R*T^2`). The error is 0 where the loop nulls the input (a step in
 * either order, a ramp in order 3), `x/ki` for a ramp in order 2, `x/kr`
 * for an acceleration in order 3, and +infinity where it does not settle
 * (an acceleration in order 2, whose error grows by `x/ki` every update).
 *
 * @param order        2 or 3.
 * @param ki           Frequency-path gain.
 * @param kr           Rate-path gain (order 3).
 * @param input_order  1, 2 or 3.
 * @param x            Size of the input, cycles per update to the power of
 *                     its order.
 * @return The steady phase error in cycles, or NaN for an unknown order.
 */
double dp_state_filter_steady_error (int order, double ki, double kr,
                                     int input_order, double x);

/**
 * @brief Whether this filter is stable at a detector delay and loop gain.
 * @param state  Must be non-NULL.
 * @param delay  Detector delay in updates.
 * @param gain   Loop gain as a multiple of nominal.
 * @return true if stable, false if not.
 * @see dp_state_filter_loop_stable()
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.12, t=1.0)   # the bound itself
 * >>> sf.stable(delay=2.0, gain=2.0)     # worst corner of the rectangle
 * True
 * >>> sf.stable(delay=2.0, gain=400.0)   # a detector slope far too high
 * False
 * >>> sf.stable(gain=0.05)               # ...and far too low
 * False
 *
 * @endcode
 */
bool dp_state_filter_stable (const dp_state_filter_state_t *state,
                             double delay, double gain);

/**
 * @brief This filter's exact delivered noise bandwidth at a detector delay
 *        and loop gain.
 * @param state  Must be non-NULL.
 * @param delay  Detector delay in updates.
 * @param gain   Loop gain as a multiple of nominal.
 * @return Cycles per update, or NaN if not stable there.
 * @see dp_state_filter_loop_bandwidth()
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.03, t=1.0)
 * >>> round(sf.bandwidth(), 6)           # delay 0: the bandwidth asked for
 * 0.03
 * >>> round(sf.bandwidth(delay=1.0), 6)  # one update of detector delay
 * 0.032646
 * >>> sf.bandwidth(gain=0.05)            # unstable: no bandwidth
 * nan
 *
 * @endcode
 */
double dp_state_filter_bandwidth (const dp_state_filter_state_t *state,
                                  double delay, double gain);

/* ── Serializable state (standard bytes interface; see dp_state.h) ────────
 * The envelope, a flavor u32, a pad u32, then kp ki kr integ rate bn t as
 * doubles. The pad word is written 0 and a blob with any other value is
 * refused, so it stays free to carry something later. The flavor word
 * is a reject key: 0 is the fixed-gain filter, and a blob of any other
 * flavor is refused, so a second flavor can add its own payload under the
 * same version.
 */
/** State-blob magic ("SFLT") checked by dp_state_filter_set_state(). */
#define STATE_FILTER_STATE_MAGIC DP_FOURCC ('S', 'F', 'L', 'T')
/** State-blob layout version. */
#define STATE_FILTER_STATE_VERSION 1u

/** @brief Serialized-state byte size.
 *  @param state Filter state (unused: the size is fixed).
 *  @return Envelope, the flavor word and the seven doubles. */
size_t dp_state_filter_state_bytes (const dp_state_filter_state_t *state);
/** @brief Serialize the filter into @p blob: gains, estimates, bn and t.
 *  @param state Filter state. Must be non-NULL.
 *  @param[out] blob  At least dp_state_filter_state_bytes() bytes. */
void dp_state_filter_get_state (const dp_state_filter_state_t *state,
                                void *blob);
/** @brief Restore state; DP_OK, or DP_ERR_INVALID if the blob is refused.
 *  The envelope is checked, the flavor word must be 0, and the payload is
 *  decoded into a temporary and accepted only when
 *  dp_state_filter_state_ok() accepts it. The estimates are not checked: a
 *  NaN in them is garbage in. A refused blob leaves @p state unchanged.
 *  @param state Filter state to overwrite. Must be non-NULL.
 *  @param[in] blob  Blob from dp_state_filter_get_state().
 *  @return DP_OK, or DP_ERR_INVALID. */
int dp_state_filter_set_state (dp_state_filter_state_t *state,
                               const void *blob);

/**
 * @brief Advance the loop one update with error @p x and return the control
 *        value the tracker should apply.
 *
 * `rate += kr*x; integ += rate + ki*x; control = integ + kp*x`. Inline, with
 * no test of any kind: this is the hot step of a tracking loop.
 *
 * @param state  Must be non-NULL.
 * @param x      Loop error (discriminator output) for this update.
 * @return Control value `integ + kp*x`.
 *
 * @code
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.02, t=1.0)
 * >>> round(sf.step(1.0), 6)             # unit error: kr + ki + kp
 * 0.047602
 * >>> round(sf.rate, 8), round(sf.integ, 8)
 * (1.257e-05, 0.00109406)
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT double
dp_state_filter_step (dp_state_filter_state_t *state, double x)
{
  state->rate += state->kr * x;
  state->integ += state->rate + state->ki * x;
  return state->integ + state->kp * x;
}

/**
 * @brief Filter a block of loop errors, one control value per update.
 *
 * Equivalent to dp_state_filter_step() once per element in order, carrying
 * both accumulators across the block.
 *
 * @param state  Component state (mutated across the block).
 * @param x      Loop-error array, one discriminator sample per update.
 * @param out    Control-value array (length >= n; may alias @p x).
 * @param n      Number of updates.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import StateFilter
 * >>> sf = StateFilter(bn=0.05, t=1.0)
 * >>> ctl = sf.steps(np.full(50, 0.1))   # constant error into the loop
 * >>> round(float(ctl[0]), 4)            # the first nudge
 * 0.0117
 * >>> round(float(ctl[-1]), 4)           # open loop: ramping
 * 0.0638
 *
 * @endcode
 */
void dp_state_filter_steps (dp_state_filter_state_t *state, const double *x,
                            double *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_STATE_FILTER_CORE_H */
