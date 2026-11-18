/**
 * @file state_filter_kalman_core.h
 * @brief The Kalman flavor of the third-order tracking loop: the gains come
 *        from a covariance recursion instead of being held.
 *
 * See docs/design/loop-filter.md Part III. The filter is the base
 * dp_state_filter_state_t (the loop's three lines, called not copied)
 * followed by the covariance of the predicted state, its start, and the two
 * figures `q` (the rate's random-walk variance per update) and `r` (the
 * detector-error variance per update). A caller states the two rms figures
 * `sigma_e` and `sigma_a`, or a bandwidth alone.
 *
 * Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`;
 * dp_state_filter_kalman_init() does the same into caller memory with no
 * heap, as the base's dp_state_filter_init() does.
 *
 * @code
 * dp_state_filter_kalman_state_t k;
 * dp_state_filter_kalman_init_bn (&k, 0.01, 1.0);  // the automatic case
 * double control = dp_state_filter_kalman_step (&k, 0.1);
 * @endcode
 */
#ifndef DP_STATE_FILTER_KALMAN_CORE_H
#define DP_STATE_FILTER_KALMAN_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/state_filter/state_filter_core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

/* ── The Kalman flavor: its own component ───────────────────────────────
 * It calls the fixed-gain component (state_filter_core.h): the base struct
 * is the first member, and the update, the exact stability and the exact
 * bandwidth are the base's own functions. The loop is the same, with the
 * gains computed each update from a covariance recursion on a model with a
 * noise figure `q` (the rate random-walks) and a measurement figure `r`
 * (docs/design/loop-filter.md Part III). Its own C type, its own functions: a
 * Kalman filter handed to the fixed-gain functions that write the state would
 * pair gains and covariance wrongly.
 */

/**
 * @brief The smallest `bn*t` the Kalman flavor accepts: the smallest the
 *        validation harness measures it at, so nothing below is claimed. At
 *        1e-6 the delivered bandwidth is `bn*t` to under 1e-8 and the plain
 *        covariance update stays positive definite with gains within 3e-11 of
 *        a long double reference over 10/(bn*t) updates (the harness's gains
 *        and plain-update blocks, which assert this constant is not below
 *        1e-6). Not the fixed-gain flavor's STATE_FILTER_BNT_MIN.
 */
#define STATE_FILTER_KALMAN_BNT_MIN 1e-6

/**
 * @brief The largest steady-state `bn*t` the Kalman flavor accepts. Its own
 *        constant: the Riccati gains are not the fixed-gain flavor's, so the
 *        rectangle is derived again from the exact function at them, by
 *        native/validation/state_filter_kalman_loop.c, which finds the
 *        largest `bn*t` stable at the upper corner (K 2, delay 2) to be 0.1302.
 *        This constant is the largest value on its 0.01 grid at which the
 *        upper gain edge at the worst delay keeps 5% headroom over
 *        STATE_FILTER_K_MAX, so a 5% error in loop gain or detector slope
 *        stays stable: 2.18 against 2 at 0.12 and 2.004 at 0.13. The harness
 *        asserts exactly that, so a different value fails it. Both bounds are
 *        compared in double: a bandwidth within rounding of one (a figure
 *        set whose implied `bn*t` lands within an ulp of it) is outside the
 *        contract, and may be accepted on one platform and refused on
 *        another.
 */
#define STATE_FILTER_KALMAN_BNT_MAX 0.12

/**
 * @brief The largest start any figure set can be given: the initial
 *        frequency deviation as a multiple of the steady-state one.
 *
 * It is a ceiling, not the rule. The rule is that the gains along the
 * covariance's whole trajectory stay inside the stability rectangle (K from
 * 0.5 to 2, detector delays 0 to 2), and
 * dp_state_filter_kalman_params_ok_qr() checks it for the figures given. The
 * rule's own cap is above 5.9 at every bn*t up to 0.04, so the ceiling binds
 * there: 5 is the integer part of the smallest of those caps (5.90 at 0.04),
 * which the harness asserts, so the ceiling is the rule's own number rounded
 * down and not a free choice. Past
 * 0.04 the rule binds: 3.77 at 0.06, 2.42 at 0.08, 1.67 at 0.10 and 1.19 at
 * the bound 0.12 (the harness's caps table, which the validation report
 * renders in section 2.5).
 */
#define STATE_FILTER_KALMAN_START_MAX 5.0

/**
 * @brief A third-order loop whose gains come from a covariance recursion.
 *
 * The base is first: its `kp`, `ki`, `kr` are the gains in force (rewritten
 * each update), its `integ` and `rate` the estimates, its `bn` and `t` the
 * steady-state bandwidth and the update period. The covariance is of the
 * PREDICTED state `(phase, frequency, rate)`, its six distinct entries in
 * the order `p00 p01 p02 p11 p12 p22`.
 *
 * Public so an embedder can hold one by value (the init functions need no
 * heap). Pass a pointer to this type to the `kalman` functions; the
 * base's functions that only read (dp_state_filter_stable(),
 * dp_state_filter_bandwidth(), the fields) accept `&k->base`, and those that
 * write do not (they do not know the covariance).
 */
typedef struct
{
  dp_state_filter_state_t base; /**< gains in force, estimates, bn, t        */
  double                  p[6]; /**< covariance of the predicted state       */
  double                  p0[6]; /**< covariance reset() returns to          */
  double                  q;    /**< rate-walk variance per update           */
  double                  r;    /**< detector-error variance per update      */
} dp_state_filter_kalman_state_t;

/**
 * @brief The steady-state covariance and gains of the recursion, by
 *        structure-preserving doubling (quadratically convergent, no
 *        iteration to a tolerance in `1/(bn*t)` updates).
 *
 * @param q          Rate-walk variance per update, > 0.
 * @param r          Detector-error variance per update, > 0.
 * @param[out] p     The steady-state covariance, `p00 p01 p02 p11 p12 p22`.
 * @param[out] gains The loop gains `kp`, `ki`, `kr` it gives.
 * @return 1 on success, 0 for a non-positive or non-finite figure.
 */
int dp_state_filter_kalman_steady (double q, double r, double p[6],
                                   double gains[3]);

/**
 * @brief The rate-walk variance whose steady-state loop has the exact
 *        noise bandwidth @p bnt per update, for a unit measurement
 *        variance: the noise ratio `bn` selects.
 * @param bnt  The bandwidth in cycles per update, between
 *             STATE_FILTER_KALMAN_BNT_MIN and STATE_FILTER_KALMAN_BNT_MAX.
 * @return The ratio `q/r`, or 0 outside that range.
 */
double dp_state_filter_kalman_ratio_for_bandwidth (double bnt);

/**
 * @brief The domain: `q > 0`, `r > 0`, `t > 0`, all finite; the steady-state
 *        bandwidth from STATE_FILTER_KALMAN_BNT_MIN up to
 *        STATE_FILTER_KALMAN_BNT_MAX per update; and a start from 1 up to
 *        STATE_FILTER_KALMAN_START_MAX whose whole covariance trajectory
 *        keeps the gains inside the stability rectangle. A start above 1
 *        costs a walk of 8/(bn*t) updates and about 1200 stability tests at
 *        bn*t 1e-2 (3000 at 1e-4): measured 1.2 ms and 5.7 ms by the bench
 *        rows create_start_1e2 and create_start_1e4, growing as 1/(bn*t).
 * @param q      Rate-walk variance per update.
 * @param r      Detector-error variance per update.
 * @param t      Update period in samples.
 * @param start  Initial frequency deviation over the steady-state one.
 * @return 1 inside the domain, 0 outside it.
 */
int dp_state_filter_kalman_params_ok_qr (double q, double r, double t,
                                      double start);

/**
 * @brief Initialise in place (no allocation): the covariance at its steady
 *        state with the frequency entry scaled by @p start squared, the
 *        estimates zero, the gains the first update's.
 *
 * Arguments are not validated here, as in dp_state_filter_init(), and the
 * result is defined for every input: a non-positive or non-finite q or r
 * makes the steady solve refuse, which leaves the inert filter (zero
 * covariance, zero gains, zero estimates; the figures and the period are
 * still stored); any other input, including a bandwidth outside the flavor's
 * range or a start past the ceiling, builds the live filter those figures
 * describe. dp_state_filter_kalman_params_ok_qr() is the check.
 *
 * @param k      Must be non-NULL.
 * @param q      Rate-walk variance per update.
 * @param r      Detector-error variance per update.
 * @param t      Update period in samples.
 * @param start  Initial frequency deviation over the steady-state one, >= 1
 *               (1 is the automatic case: the steady-state gains from the
 *               first update).
 */
void dp_state_filter_kalman_init_qr (dp_state_filter_kalman_state_t *k,
                                     double q,
                                  double r, double t, double start);

/**
 * @brief Create a Kalman filter, validating its arguments.
 * @param q      Rate-walk variance per update, > 0.
 * @param r      Detector-error variance per update, > 0.
 * @param t      Update period in samples, > 0.
 * @param start  Initial frequency deviation over the steady-state one.
 * @return Heap-allocated filter, or NULL outside
 *         dp_state_filter_kalman_params_ok_qr().
 */
dp_state_filter_kalman_state_t *dp_state_filter_kalman_create_qr (double q,
                                                                  double r,
                                                         double t,
                                                         double start);

/**
 * @brief Destroy a Kalman filter.
 * @param k  May be NULL.
 */
void dp_state_filter_kalman_destroy (dp_state_filter_kalman_state_t *k);

/**
 * @brief Return to the start: the estimates zero and the covariance back to
 *        the one the filter began with. The noise figures are kept.
 * @param k  Must be non-NULL.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)
 * >>> _ = k.steps(np.full(50, 0.1))
 * >>> round(k.integ, 4)
 * 0.0016
 * >>> k.reset()
 * >>> k.integ
 * 0.0
 *
 * @endcode
 */
void dp_state_filter_kalman_reset (dp_state_filter_kalman_state_t *k);

/**
 * @brief The implementation of dp_state_filter_kalman_step(), out of line.
 *        Call the step; this is its body: the loop's three lines on the
 *        gains in force, then the covariance advanced and the gains
 *        refreshed. The step is a header-inline wrapper over it because the
 *        binding generator derives dp_state_filter_kalman_step as the
 *        built-in step's name and looks for it in the header.
 * @param k  Must be non-NULL.
 * @param e  The loop error for this update.
 * @return The control value `integ + kp*e`.
 */
double dp_state_filter_kalman_update (dp_state_filter_kalman_state_t *k,
                                      double e);

/**
 * @brief One update, the call to use: the loop's three lines on the gains
 *        in force, then the covariance advanced and the gains refreshed.
 *        Inline, over dp_state_filter_kalman_update().
 * @param k  Must be non-NULL.
 * @param e  The loop error for this update.
 * @return The control value `integ + kp*e`.
 *
 * @code
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)
 * >>> round(k.step(0.1), 4)             # the first nudge
 * 0.0024
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT double
dp_state_filter_kalman_step (dp_state_filter_kalman_state_t *k, double e)
{
  return dp_state_filter_kalman_update (k, e);
}

/**
 * @brief A block of updates, equivalent to dp_state_filter_kalman_step()
 *        once per element.
 * @param k    Must be non-NULL.
 * @param x    Loop errors.
 * @param out  Control values (length >= n; may alias @p x).
 * @param n    Number of updates.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)
 * >>> y = k.steps(np.full(50, 0.1))     # constant error: open loop, ramping
 * >>> round(float(y[0]), 4), round(float(y[-1]), 4)
 * (0.0024, 0.004)
 *
 * @endcode
 */
void dp_state_filter_kalman_steps (dp_state_filter_kalman_state_t *k,
                                   const double *x, double *out, size_t n);

/**
 * @brief Size of the Kalman blob: envelope, gains, estimates, covariance,
 *        its start, and the two figures.
 * @param k  The filter (its size is the same for every filter).
 * @return Bytes dp_state_filter_kalman_get_state() writes.
 */
size_t
dp_state_filter_kalman_state_bytes (const dp_state_filter_kalman_state_t *k);

/**
 * @brief Serialize into a buffer of dp_state_filter_kalman_state_bytes().
 * @param k     The filter to serialize.
 * @param blob  Destination, at least dp_state_filter_kalman_state_bytes().
 */
void dp_state_filter_kalman_get_state (const dp_state_filter_kalman_state_t *k,
                                       void *blob);

/**
 * @brief Restore from a blob; nothing changes unless it is wholly accepted.
 *
 * Refused (DP_ERR_INVALID): a bad envelope (the fixed-gain filter's blob
 * has another magic); a covariance P or start P0 that is not finite and
 * positive definite; figures outside dp_state_filter_kalman_params_ok_qr();
 * gains that are not the ones P and r give; and a P0 that is not a start a
 * live object holds: the steady state for (q, r) with only the frequency entry
 * scaled, by a start from 1 up to STATE_FILTER_KALMAN_START_MAX. A start past
 * the ceiling (an embedder that used the unguarded init) is refused here, and
 * that is the embedder's to know.
 *
 * What is not checked, and why. The current covariance P is checked for
 * positive definiteness and for giving the gains in force, and no further:
 * mid-transient it cannot be recomputed, as the base's integ and rate are not
 * checked. The trajectory rule is not rerun on restore (it walks 8/(bn*t)
 * updates and 1200 to 3000 stability tests: 1.2 ms at bn*t 1e-2, 5.7 ms at
 * 1e-4, measured): a restored start is checked for the ceiling and for being
 * a start at all, so a start between the rule's cap and the ceiling that an
 * embedder built with the unguarded init is restored as it was. Whether to
 * rerun the rule on restore is tracked in
 * https://github.com/doppler-dsp/doppler/issues/3136.
 *
 * @param k     The filter to restore into.
 * @param blob  A blob from dp_state_filter_kalman_get_state().
 * @return DP_OK or DP_ERR_INVALID.
 */
int dp_state_filter_kalman_set_state (dp_state_filter_kalman_state_t *k,
                                      const void *blob);

/* ── The Kalman flavor, as a caller states it ──────────────────────────
 * Two figures in the units of the loop's error input: @c sigma_e, the rms of
 * the detector error when the loop is locked (cycles, if the error is a phase
 * in cycles), and @c sigma_a, the rms change of the rate per update (those
 * units per update per update); the `_qr` functions above take their
 * squares. An optional frequency uncertainty @c sigma_f0 (0 = absent; those
 * units per update) must be at least the steady state's own frequency
 * deviation and at most STATE_FILTER_KALMAN_START_MAX times it, and no more
 * than the trajectory rule allows. A caller with a carrier-to-noise
 * density and a jerk makes the two figures from them; nothing here knows a
 * detector.
 */

/**
 * @brief The start multiplier a frequency uncertainty means: @p sigma_f0
 *        over the steady state's frequency deviation for these figures.
 * @param sigma_e   RMS detector error when locked, > 0.
 * @param sigma_a   RMS change of the rate per update, > 0.
 * @param sigma_f0  Frequency uncertainty; 0 for the steady state.
 * @return The multiplier (1 for 0), or 0 for a non-positive figure or a
 *         @p sigma_f0 below the steady state's deviation or above
 *         STATE_FILTER_KALMAN_START_MAX times it.
 */
double dp_state_filter_kalman_start_for (double sigma_e, double sigma_a,
                                         double sigma_f0);

/**
 * @brief The domain, in the caller's figures.
 * @param sigma_e   RMS detector error when locked, > 0.
 * @param sigma_a   RMS change of the rate per update, > 0.
 * @param t         Update period in samples, > 0.
 * @param sigma_f0  Frequency uncertainty, 0 or within
 *                  dp_state_filter_kalman_start_for()'s range.
 * @return 1 inside, 0 outside (including a steady-state bandwidth outside
 *         STATE_FILTER_KALMAN_BNT_MIN to STATE_FILTER_KALMAN_BNT_MAX).
 */
int dp_state_filter_kalman_params_ok (double sigma_e, double sigma_a, double t,
                                      double sigma_f0);

/**
 * @brief Initialise in place from the caller's figures. Unguarded, as
 *        dp_state_filter_init() is: outside the domain the filter holds a
 *        zero covariance and zero gains, defined and inert.
 * @param k         Must be non-NULL.
 * @param sigma_e   RMS detector error when locked.
 * @param sigma_a   RMS change of the rate per update.
 * @param t         Update period in samples.
 * @param sigma_f0  Frequency uncertainty, 0 for the steady state.
 */
void dp_state_filter_kalman_init (dp_state_filter_kalman_state_t *k,
                                  double sigma_e,
                                  double sigma_a, double t, double sigma_f0);

/**
 * @brief Create from the caller's figures.
 * @param sigma_e   RMS detector error when locked.
 * @param sigma_a   RMS change of the rate per update.
 * @param t         Update period in samples.
 * @param sigma_f0  Frequency uncertainty, 0 for the steady state.
 * @return Heap-allocated filter, or NULL outside
 *         dp_state_filter_kalman_params_ok().
 */
dp_state_filter_kalman_state_t *dp_state_filter_kalman_create (double sigma_e,
                                                         double sigma_a,
                                                         double t,
                                                         double sigma_f0);

/**
 * @brief Change the figures, or to a bandwidth alone, keeping the estimates.
 *
 * A retune restarts the gain schedule; the estimates carry: the covariance
 * restarts from the new steady state (scaled by the start @p sigma_f0 means),
 * the gains follow it, and `integ` and `rate` are untouched. An argument of 0
 * keeps its current value (@p sigma_f0 0 is the steady state), so a call names
 * only what changes. @p bn alone is the automatic case and may not be mixed
 * with the figures. A call outside the domain
 * (dp_state_filter_kalman_params_ok()) changes nothing.
 *
 * Three behaviors, all in the blob (q, r and t are reject keys, the start is
 * P0):
 * - The automatic case holds the figures its bandwidth implies, with
 *   `sigma_e` 1 (the detector's rms as the unit) and `sigma_a` the square root
 *   of the noise ratio that bandwidth selects; a figures-form call that names
 *   one figure keeps the other at that implied value.
 * - A call with @p bn replaces the stored figures by those the bandwidth
 *   implies: a figures-form call that follows keeps the implied `r` and `q`,
 *   not the ones the filter was built with.
 * - A call with every argument 0 puts the gains at the steady state at once
 *   and keeps the estimates: it ends a high-gain start early, once the loop
 *   has pulled in, without changing the design. `configure (k, 0, 0, 0, f, 0)`
 *   with a frequency uncertainty @p f alone does the opposite and re-opens
 *   the gains with the same figures, as after a loss of lock.
 * @param k         Must be non-NULL.
 * @param sigma_e   RMS detector error when locked, 0 to keep.
 * @param sigma_a   RMS change of the rate per update, 0 to keep.
 * @param t         Update period in samples, 0 to keep.
 * @param sigma_f0  Frequency uncertainty, 0 for the steady state.
 * @param bn        Noise bandwidth in cycles per sample (automatic form), 0
 *                  for the figures form.
 * @return DP_OK, or DP_ERR_INVALID (nothing changed).
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)
 * >>> _ = k.steps(np.full(50, 0.1))
 * >>> k.configure(bn=0.02)               # widen the loop, keep lock
 * >>> round(k.integ, 4), round(k.bn, 4)
 * (0.0016, 0.02)
 *
 * @endcode
 */
int dp_state_filter_kalman_configure (dp_state_filter_kalman_state_t *k,
                                      double sigma_e, double sigma_a, double t,
                                      double sigma_f0, double bn);

/**
 * @brief The domain of the automatic case: a bandwidth and a period alone,
 *        `bn * t` from STATE_FILTER_KALMAN_BNT_MIN to
 *        STATE_FILTER_KALMAN_BNT_MAX.
 * @param bn  Noise bandwidth in cycles per sample.
 * @param t   Update period in samples.
 * @return 1 inside the domain, 0 outside it.
 */
int dp_state_filter_kalman_params_ok_bn (double bn, double t);

/**
 * @brief Initialise in place from `bn` and `t`: the steady-state gains of the
 *        optimal filter whose exact bandwidth is `bn`, from the first update.
 *        Unguarded; see dp_state_filter_kalman_init().
 * @param k   Must be non-NULL.
 * @param bn  Noise bandwidth in cycles per sample.
 * @param t   Update period in samples.
 */
void dp_state_filter_kalman_init_bn (dp_state_filter_kalman_state_t *k,
                                     double bn,
                                     double t);

/**
 * @brief Create the automatic case.
 * @param bn  Noise bandwidth in cycles per sample.
 * @param t   Update period in samples.
 * @return Heap-allocated filter, or NULL outside
 *         dp_state_filter_kalman_params_ok_bn().
 */
dp_state_filter_kalman_state_t *dp_state_filter_kalman_create_bn (double bn,
                                                            double t);

/* ── For the binding ─────────────────────────────────────────────────────
 * The Python class takes either the figures or `bn` alone; these dispatch to
 * the typed C functions above. A C caller calls the typed ones directly. */

/** @brief Magic of the Kalman blob (the fixed-gain filter's is 'SFLT'). */
#define STATE_FILTER_KALMAN_STATE_MAGIC DP_FOURCC ('S', 'F', 'K', 'A')
/** @brief Version of the Kalman blob layout. */
#define STATE_FILTER_KALMAN_STATE_VERSION 1u

/**
 * @brief Create from the figures or from `bn` alone. The binding calls
 *        dp_state_filter_kalman_create_args_why(), which also says why.
 *
 * The figures form is @p sigma_e and @p sigma_a both > 0 with @p bn 0; the
 * automatic form is @p bn > 0 with @p sigma_e, @p sigma_a and @p sigma_f0
 * all 0. Both or neither is refused.
 * @param sigma_e   RMS detector error when locked (figures form).
 * @param sigma_a   RMS change of the rate per update (figures form).
 * @param t         Update period in samples.
 * @param sigma_f0  Initial frequency uncertainty (figures form), 0 for the
 *                  steady state.
 * @param bn        Noise bandwidth in cycles per sample (automatic form).
 * @return Heap-allocated filter, or NULL outside either form's domain.
 */
dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args (double sigma_e, double sigma_a, double t,
                                    double sigma_f0, double bn);

/**
 * @brief The reason the arguments of dp_state_filter_kalman_create_args()
 *        are refused, naming the argument, or NULL when they are accepted.
 *
 * The conditions are those of the two constructors it dispatches to, in the
 * order they are checked: the form (figures or `bn`, not both, not neither),
 * `t`, then the figures' own conditions (each positive, a steady state, a
 * bandwidth inside the domain, a `sigma_f0` that is a legal start) or `bn`'s.
 * The strings are static and carry no numbers, so they do not drift with the
 * domain's limits.
 *
 * @param sigma_e   RMS detector error when locked (the figures form).
 * @param sigma_a   RMS change of the rate per update (the figures form).
 * @param t         Update period in samples.
 * @param sigma_f0  Frequency uncertainty (the figures form), 0 for none.
 * @param bn        Bandwidth alone (the automatic form).
 * @return A static sentence, or NULL if the arguments are accepted.
 */
const char *dp_state_filter_kalman_args_why (double sigma_e, double sigma_a,
                                             double t, double sigma_f0,
                                             double bn);

/**
 * @brief Create from the figures or from `bn` alone: the one constructor the
 *        Python class `KalmanStateFilter` calls.
 *
 * The figures form is @p sigma_e and @p sigma_a both > 0 with @p bn 0; the
 * automatic form is @p bn > 0 with @p sigma_e, @p sigma_a and @p sigma_f0
 * all 0. Both or neither is refused.
 * @param sigma_e   RMS detector error when locked (figures form).
 * @param sigma_a   RMS change of the rate per update (figures form).
 * @param t         Update period in samples.
 * @param sigma_f0  Initial frequency uncertainty (figures form), 0 for the
 *                  steady state.
 * @param bn        Noise bandwidth in cycles per sample (automatic form).
 * @param why       Where the refusal's sentence goes, naming the argument it
 *                  refused (dp_state_filter_kalman_args_why()); set to NULL
 *                  when the filter is built. May be NULL.
 * @return Heap-allocated filter, or NULL (with @p why set) outside either
 *         form's domain.
 *
 * @code
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)             # the automatic case
 * >>> round(k.bandwidth(), 6)
 * 0.01
 * >>> k.stable()
 * True
 * >>> f = KalmanStateFilter(sigma_e=1.0, sigma_a=1e-3)   # the two figures
 * >>> round(f.bandwidth(), 4)
 * 0.0904
 * >>> KalmanStateFilter()                        # neither form
 * Traceback (most recent call last):
 *     ...
 * ValueError: give sigma_e and sigma_a, or bn alone
 *
 * @endcode
 */
dp_state_filter_kalman_state_t *
dp_state_filter_kalman_create_args_why (double sigma_e, double sigma_a,
                                        double t, double sigma_f0, double bn,
                                        const char **why);

/**
 * @brief The steady state's frequency deviation for this filter's figures,
 *        in the loop's units: the lower edge of a `sigma_f0` that
 *        dp_state_filter_kalman_params_ok() accepts (its upper edge is
 *        STATE_FILTER_KALMAN_START_MAX times this).
 * @param k  Must be non-NULL.
 * @return The deviation, or 0 for an inert (out-of-domain) filter.
 *
 * @code
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6)
 * >>> sf = k.sigma_f
 * >>> KalmanStateFilter(sigma_e=1.0, sigma_a=2e-6, sigma_f0=3 * sf).bn > 0
 * True
 *
 * @endcode
 */
double
dp_state_filter_kalman_sigma_f (const dp_state_filter_kalman_state_t *k);

/**
 * @brief The exact bandwidth of the loop at the gains in force now: the
 *        base's dp_state_filter_bandwidth() on `&k->base`.
 * @param k      Must be non-NULL.
 * @param delay  Detector delay in updates.
 * @param gain   Loop gain as a multiple of nominal.
 * @return Cycles per update, NaN where the frozen loop is not stable.
 *
 * @code
 * >>> from doppler.track import KalmanStateFilter
 * >>> k = KalmanStateFilter(bn=0.01)
 * >>> round(k.bandwidth(delay=1.0), 4)   # a detector delay of one update
 * 0.0103
 *
 * @endcode
 */
double
dp_state_filter_kalman_bandwidth (const dp_state_filter_kalman_state_t *k,
                                 double delay, double gain);

/**
 * @brief The exact stability of the loop at the gains in force now: the
 *        base's dp_state_filter_stable() on `&k->base`.
 * @param k      Must be non-NULL.
 * @param delay  Detector delay in updates.
 * @param gain   Loop gain as a multiple of nominal.
 * @return true where the frozen loop is stable.
 *
 * @code
 * >>> from doppler.track import KalmanStateFilter
 * >>> KalmanStateFilter(bn=0.01).stable(delay=2.0)
 * True
 *
 * @endcode
 */
bool dp_state_filter_kalman_stable (const dp_state_filter_kalman_state_t *k,
                                    double delay, double gain);

#ifdef __cplusplus
}
#endif

#endif /* DP_STATE_FILTER_KALMAN_CORE_H */
