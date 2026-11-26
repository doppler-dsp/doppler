/**
 * @file loop_filter_core.h
 * @brief Second-order proportional-integral loop filter — the shared engine
 *        of every tracking loop (Costas/PLL, DLL, symbol timing).
 *
 * An error @c e in, a control value out: `control = integ + kp*e`, with the
 * integrator advancing `integ += ki*e`. The integrator therefore holds the
 * running frequency/rate estimate; `kp*e` is the instantaneous (phase) nudge.
 * Gains @c kp / @c ki come from a loop noise bandwidth, damping, and update
 * period via the standard 2nd-order form (dp_loop_filter_init()).
 *
 * ### Keep `bn * t <= 0.0112` and the bandwidth is the one you asked for
 *
 * @c bn is the noise bandwidth of the CLOSED loop, in cycles per sample, and
 * @c t is the update period in samples — so a loop ticking once per update
 * has a bandwidth of `bn * t` cycles per update, and only that PRODUCT
 * matters. Measured (`native/validation/loop_filter_noise_bw.c`), the
 * delivered bandwidth is always slightly **wide**, never narrow, by a
 * fractional excess with a closed form:
 *
 *     Bn / (bn*t)  -  1  ~=  16*zeta^2 / (4*zeta^2 + 1)^2  *  (bn*t)
 *
 * At @c zeta = 0.707 that is `bn*t <= 0.0112` for 1% and `<= 0.0552` for 5%;
 * heavier damping is more forgiving (`0.0450` for 1% at zeta = 2.0). Every
 * configuration shipped in this library sits inside the 1% figure. Being
 * wide rather than narrow is the safe direction — a caller sizing jitter or
 * settling off @c bn is conservative.
 *
 * The promise holds only when the rest of the loop has unit gain, which this
 * filter cannot see. What the caller must deliver, and what happens when it
 * does not, is **the contract**, stated once on dp_loop_filter_create() (and
 * so on the Python class's docstring): the detector's slope at lock is 1, the
 * actuator's gain is 1, and a gain K scales the bandwidth by
 * `(4*K*zeta^2 + 1) / (4*zeta^2 + 1)`. See also docs/design/loop-filter.md
 * section 3.
 *
 * Settling follows from the same number: a step settles to +-5% within about
 * 2.3 loop constants (`2.3/bn` updates) at zeta = 0.707, so the `5/bn` rule
 * used throughout this library is comfortable rather than tight.
 *
 * The state struct is **public** so a tracker can embed it by value (no heap)
 * and drive it with dp_loop_filter_init()/dp_loop_filter_step() — e.g. a despreader
 * keeps one for the carrier loop and one for the code loop.
 * dp_loop_filter_create() is the heap path used by the Python wrapper.
 *
 * Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`
 *
 * @code
 * dp_loop_filter_state_t *lf = dp_loop_filter_create(0.01, 0.707, 1.0);
 * double ctl = dp_loop_filter_step(lf, 0.25);   // integ += ki*e; ret integ+kp*e
 * dp_loop_filter_destroy(lf);
 * @endcode
 */
#ifndef DP_LOOP_FILTER_CORE_H
#define DP_LOOP_FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Second-order PI loop filter state (embeddable by value).
   */
  typedef struct
  {
    double kp;    /**< proportional gain (derived from bn, zeta, t).   */
    double ki;    /**< integral gain (derived from bn, zeta, t).       */
    double integ; /**< integrator memory = running rate/freq estimate. */
    double bn;    /**< loop noise bandwidth, normalized cycles/sample.  */
    double zeta;  /**< damping factor (1.0 = critical; 0.707 usual).   */
    double t;     /**< update period in samples.                       */
  } dp_loop_filter_state_t;

  /**
   * @brief Initialise a loop filter in place (no allocation).
   *
   * Computes @c kp / @c ki from the loop noise bandwidth @p bn (normalized,
   * cycles/sample), damping @p zeta, and update period @p t (samples), and
   * stores @p bn / @p zeta / @p t. Does **not** touch @c integ, so it doubles
   * as a reconfigure that preserves lock. Use this for a `dp_loop_filter_state_t`
   * embedded by value; dp_loop_filter_create() is calloc + dp_loop_filter_init().
   *
   * **Arguments are NOT validated here, on purpose.** This is the by-value
   * path taken by the objects that embed a filter, all of which validate
   * upstream; dp_loop_filter_create() is the boundary that faces an untrusted
   * caller and it rejects the same domain this documents. Passing `t = 0`
   * here yields `kp = ki = 0` — a loop that never moves — and a non-finite
   * argument yields NaN gains that never recover.
   *
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707), > 0.
   * @param t      Update period in samples (> 0).
   */
  void dp_loop_filter_init(dp_loop_filter_state_t *state, double bn, double zeta,
                        double t);

  /**
   * @brief Natural frequency implied by a loop bandwidth and damping.
   *
   * `wn = 8*zeta*bn / (4*zeta^2 + 1)`, which at `zeta = 0.707` is
   * `1.8857*bn`. In the same units as @p bn: pass a `bn` normalised to the
   * symbol rate and `wn` is per symbol; pass one normalised to the sample
   * rate and it is per sample.
   *
   * Public because it is the number every closed form about this loop is
   * written in, and callers were re-deriving it rather than asking. The
   * steady-state phase lag under a frequency RAMP of `r` (cycles per unit
   * time squared) is `2*pi*r / wn^2` — the only one of the two standard
   * disturbances that a type-2 loop does NOT null, and therefore the one a
   * measurement can check a gain against. A frequency STEP is nulled
   * regardless of gain, so it cannot.
   *
   * The formula had five copies (this file, the loop's own C test, a
   * validation harness, an example and a validation script) and no home; a
   * gain error that moved `wn` would have had to be found five times.
   *
   * Unguarded, like dp_loop_filter_init() and for the same reason: this is the
   * trusting path, and dp_loop_filter_create() is the boundary that rejects the
   * domain. `zeta = 0` divides by zero here exactly as it always has.
   *
   * @param bn    Loop noise bandwidth, normalized (>= 0).
   * @param zeta  Damping factor (typically 0.707), > 0.
   * @return      The natural frequency, in @p bn's units.
   */
  double dp_loop_filter_wn(double bn, double zeta);

  /** The residual a loop is called settled at: the 2% settling-time
   *  convention of second-order systems. What the library's objects pass to
   *  dp_loop_filter_settle_time() for their own `settle_time`. */
#define DP_LOOP_FILTER_SETTLE_EPS 0.02

  /**
   * @brief How long the loop takes to settle from a step: the last time the
   *        error of a unit phase step is larger than @p eps.
   *
   * The loop this filter closes is second order, and its error after a
   * phase step of 1 is known in closed form. With `wn` from
   * dp_loop_filter_wn(), below critical damping it is
   * `exp(-zeta*wn*t) * cos(wd*t + asin(zeta)) / sqrt(1 - zeta^2)`,
   * `wd = wn*sqrt(1 - zeta^2)`; at `zeta = 1` it is
   * `(1 - wn*t) * exp(-wn*t)`; above, it is the difference of two decaying
   * exponentials. This returns the last time that error's magnitude equals
   * @p eps: from then on it stays under. It is exact for the continuous
   * loop, so it holds for the filter where `bn*t` is small (the same
   * condition the bandwidth itself holds under, at the top of this file).
   *
   * The answer scales as `1/bn`. At `zeta = 0.707` and `eps = 0.02` it is
   * `2.59/bn`, and at `eps = 0.001` it is `4.91/bn`, which is where the
   * `5/bn` rule of thumb sits. Read in the reciprocal of @p bn's units: a
   * `bn` normalized to the symbol rate gives symbols.
   *
   * What it does not know: it is the loop's linear response, so the start
   * must be inside the detector's monotone range, and a detector whose
   * slope is `K` and not 1 runs the loop at `K` times this decay rate. A
   * frequency offset at the start adds its own transient, which this does
   * not count.
   *
   * @code
   * #include <doppler/loop_filter/loop_filter_core.h>
   * #include <stdio.h>
   *
   * int
   * main (void)
   * {
   *   printf ("%.1f\n", dp_loop_filter_settle_time (0.005, 0.707, 0.02));
   *   printf ("%.1f\n", dp_loop_filter_settle_time (0.005, 1.0, 0.02));
   *   return 0;
   * }
   * @endcode
   *
   * @param bn    Loop noise bandwidth, normalized.
   * @param zeta  Damping factor.
   * @param eps   The error called settled, as a fraction of the step;
   *              DP_LOOP_FILTER_SETTLE_EPS is the library's.
   * @return      The settling time, in the reciprocal of @p bn's units; 0
   *              for `eps >= 1`; infinity for a loop that never settles
   *              (`bn <= 0`, `zeta <= 0`, `eps <= 0`, or a NaN).
   */
  double dp_loop_filter_settle_time (double bn, double zeta, double eps);

  /**
   * @brief The domain the loop's gains are defined on: `bn >= 0`,
   *        `zeta > 0`, `t > 0`, all three finite, and gains that come out
   *        finite.
   *
   * The ONE predicate. dp_loop_filter_create() and dp_loop_filter_configure()
   * refuse outside it, and an embedder that takes these numbers from a
   * caller calls it before the unguarded dp_loop_filter_init(): a non-finite
   * argument there yields NaN gains that never recover, a negative `bn` with
   * `zeta >= 1` can drive the gains' denominator through zero, and a finite
   * `bn` past ~7e153 at `t = 1` overflows the gains to NaN. It checks the
   * gains init would write, from the same formula. The Dll, the Despreader's
   * two loops, ratesync, the MPSK receiver, Costas, SymbolSync,
   * CarrierMpsk and CarrierNda call it (doppler#2103, #2112).
   *
   * @param bn    Loop noise bandwidth, normalized.
   * @param zeta  Damping factor.
   * @param t     Update period, in samples.
   * @return      1 inside the domain, 0 outside it (a NaN is outside).
   */
  int dp_loop_filter_params_ok(double bn, double zeta, double t);

  /**
   * @brief The domain of a first-order FLL-assist bandwidth: `bn_fll >= 0`
   *        with a finite gain `4 * bn_fll`.
   *
   * The FLL assist Costas and CarrierMpsk run beside their PLL has one gain,
   * `k_fll = 4 * bn_fll`, and `0` means "pure PLL". A negative or NaN value
   * would silently disable the assist while the getter reports it, and an
   * infinite one drives the loop integrator to inf/NaN, so both objects'
   * create() and set_bn_fll() refuse outside this predicate (doppler#2646).
   *
   * @param bn_fll  FLL-assist bandwidth, normalized to the symbol rate.
   * @return        1 inside the domain, 0 outside it (a NaN is outside).
   */
  int dp_loop_filter_fll_bn_ok(double bn_fll);

  /**
   * @brief Whether a stored filter is one a live filter could hold.
   *
   * For restoring a serialized filter, whose gains are taken verbatim rather
   * than recomputed. The filter must be in the domain at its OWN bn, zeta and
   * t (dp_loop_filter_params_ok()), and carry gains that match the one gains body at that bn, zeta and t to a relative
   * tolerance of 1e-9. The tolerance lets a blob written by another compiler
   * restore; a forged NaN, infinite or absurd gain is refused.
   *
   * @param s  The stored filter.
   * @return 1 if it is one a live filter could hold, 0 otherwise.
   */
  int dp_loop_filter_state_ok(const dp_loop_filter_state_t *s);

  /**
   * @brief Whether a filter EMBEDDED by value is one its owner could hold:
   *        the all-zero struct of an embedding not yet seeded, or one
   *        dp_loop_filter_state_ok() accepts.
   *
   * A composition that holds a loop filter by value holds it all-zero until
   * its init runs (a receiver keeps its Costas loops that way until it seeds
   * them), and its blob carries them zeroed; that state round-trips, so a
   * composition's restore must not refuse it. dp_loop_filter_state_ok()
   * alone does (a zero interval is outside the domain) and stays the
   * predicate for a standalone filter, which create() never leaves zero.
   * Anything else still has to match its own bn, zeta and t, so a forged NaN
   * or infinite gain is refused.
   *
   * @param s  The embedded filter.
   * @return 1 if @p s is all-zero or dp_loop_filter_state_ok(), 0 otherwise.
   */
  int dp_loop_filter_embedded_ok(const dp_loop_filter_state_t *s);

  /**
   * @brief Create a second-order loop filter whose closed loop has noise
   *        bandwidth `bn`, for a loop whose rest has unit gain.
   *
   * ### The contract
   *
   * The closed loop's noise bandwidth is `bn` cycles per sample, a fixed
   * function of `kp` and `ki`, provided the rest of the loop has unit gain.
   * The filter cannot see the rest of the loop, so each of these is the
   * caller's to deliver, and each is checkable:
   *
   * 1. The error detector's slope at lock is 1 per unit of the filter's
   *    error input, open loop and noise free. The filter takes the error in
   *    the unit the control is applied in, per update: the product of the
   *    detector's slope and the actuator's gain, in the units they are
   *    given, is 1. A detector that returns radians and an oscillator that
   *    takes cycles need the factor of 2*pi once, after the filter (as
   *    Costas converts the control to cycles per sample), never inside the
   *    loop gain and never twice.
   * 2. The actuator applies the control with unit gain, once per update,
   *    `t` samples apart.
   * 3. The loop is tracking, inside the detector's linear region.
   * 4. The error reaches the filter and the control reaches the actuator
   *    within the same update: no extra delay (every update of delay widens
   *    the loop; docs/design/loop-filter.md sections 11 and 12).
   *
   * A loop gain K other than 1 (the detector's slope times the actuator's
   * gain) scales the natural frequency and the damping each by the square
   * root of K, so the noise bandwidth becomes `(4*K*zeta^2 + 1) /
   * (4*zeta^2 + 1)` times `bn`: 5/3 at K = 2 and 3 at K = 4 for zeta =
   * 0.707, and 2/3 at K = 0.5. A detector whose slope is 2 is the first
   * instance found (doppler#3224). A detector whose slope is not 1, or rides
   * on signal level or SNR, is scaled to unit slope before the filter, by
   * dividing by its own slope; a slope that rides on level makes the
   * bandwidth ride on level.
   *
   * Held by `native/validation/loop_filter_noise_bw.c` (the promise),
   * `test_loop_filter_contract.py` (the gain formula, and these sentences on
   * every face) and test_loop_detector_contract.py (the callers' side).
   *
   * ### Arguments
   *
   * This is the untrusted boundary — the Python constructor passes a
   * caller's arbitrary doubles here — so unlike dp_loop_filter_init() it
   * **rejects** anything outside the declared domain rather than computing
   * gains from it. `bn = 0` is inside the domain and is accepted: it means a
   * deliberately frozen loop.
   *
   * @param bn    Loop noise bandwidth, normalized cycles/sample, of the
   *              closed loop given a unit-slope detector (see the contract);
   *              >= 0 and finite (default 0.01).
   * @param zeta  Damping factor; > 0 and finite (default 0.707).
   * @param t     Update period in samples; > 0 and finite (default 1.0).
   * @return Heap-allocated state, or NULL if any argument is outside the
   *         domain above or on allocation failure. The Python binding turns
   *         the former into a @c ValueError.
   * @note Caller must call dp_loop_filter_destroy() when done.
   */
  dp_loop_filter_state_t *dp_loop_filter_create(double bn, double zeta, double t);

  /**
   * @brief Destroy a loop_filter instance and release all memory.
   * @param state  May be NULL.
   */
  void dp_loop_filter_destroy(dp_loop_filter_state_t *state);

  /**
   * @brief Retune the loop gains @c kp / @c ki for a new (bn, zeta, t) without
   *        disturbing the integrator.
   *
   * Recomputes the proportional and integral gains from the standard 2nd-order
   * form but leaves @c integ untouched, so a loop can be widened for fast
   * acquisition and then narrowed for steady-state tracking while holding its
   * accumulated frequency/rate estimate — the retune preserves lock.
   *
   * A (bn, zeta, t) outside dp_loop_filter_params_ok() is refused and
   * changes nothing: a NaN gave gains that never recover, and a negative
   * bn could zero the denominator (doppler#2103).
   *
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707).
   * @param t      Update period in samples (> 0).
   * @return `DP_OK`, or `DP_ERR_INVALID` outside dp_loop_filter_params_ok().
   *
   * @code
   * >>> from doppler.track import LoopFilter
   * >>> lf = LoopFilter(bn=0.01, zeta=0.707, t=1.0)
   * >>> _ = lf.step(1.0)
   * >>> before = round(lf.integ, 6)
   * >>> lf.configure(0.05, 0.707, 1.0)   # widen the loop, keep lock
   * >>> round(lf.integ, 6) == before     # integrator preserved
   * True
   * >>> round(lf.kp, 6)                  # proportional gain rose
   * 0.124728
   *
   * @endcode
   */
  int dp_loop_filter_configure(dp_loop_filter_state_t *state, double bn, double zeta,
                             double t);

  /**
   * @brief Zero the integrator memory while keeping the configured gains.
   *
   * Clears the accumulated frequency/rate estimate (@c integ) back to zero but
   * leaves @c kp / @c ki as configured, so the loop reacquires from a clean
   * slate at its current bandwidth — the right thing when a tracker drops lock
   * and must restart, without re-deriving gains.
   *
   * @param state  Must be non-NULL.
   *
   * @code
   * >>> from doppler.track import LoopFilter
   * >>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
   * >>> for _ in range(10):
   * ...     _ = lf.step(1.0)             # ramp the integrator
   * >>> round(lf.integ, 6)
   * 0.013849
   * >>> lf.reset()
   * >>> lf.integ                          # integrator cleared, gains kept
   * 0.0
   *
   * @endcode
   */
  void dp_loop_filter_reset(dp_loop_filter_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); config fields restore identically
   * into an identically-built instance, the integrator memory resumes exactly.
   */
  /** State-blob magic ("LPFL") checked by dp_loop_filter_set_state(). */
#define LOOP_FILTER_STATE_MAGIC DP_FOURCC('L', 'P', 'F', 'L')
  /** State-blob layout version; bumped whenever the struct layout changes. */
#define LOOP_FILTER_STATE_VERSION 1u

  /** @brief Serialized-state byte size.
   *  @param state Filter state (unused: the size is fixed).
   *  @return sizeof(dp_state_hdr_t) + sizeof(dp_loop_filter_state_t). */
  size_t dp_loop_filter_state_bytes(const dp_loop_filter_state_t *state);
  /** @brief Serialize the loop state into @p blob.
   *  Writes the whole struct: gains, integrator, bn, zeta and t.
   *  @param state Filter state. Must be non-NULL.
   *  @param[out] blob  At least dp_loop_filter_state_bytes() bytes. */
  void dp_loop_filter_get_state(const dp_loop_filter_state_t *state, void *blob);
  /** @brief Restore state; DP_OK, or DP_ERR_INVALID if the blob is refused.
   *  The envelope (magic, version, endianness, size) is checked, then the
   *  payload is decoded into a temporary and accepted only when
   *  dp_loop_filter_state_ok() accepts it -- the gains, bn, zeta and t that
   *  create() would refuse are refused (doppler#2641). The integrator is
   *  not checked: a NaN in it is garbage in (doppler#2128). An accepted
   *  payload is copied over the whole struct.
   *  @param state Filter state to overwrite. Must be non-NULL.
   *  @param[in] blob  Blob from dp_loop_filter_get_state().
   *  @return DP_OK, or DP_ERR_INVALID on an envelope mismatch or a payload
   *          dp_loop_filter_state_ok() refuses (and @p state is unchanged). */
  int dp_loop_filter_set_state(dp_loop_filter_state_t *state, const void *blob);

  /**
   * @brief Advance the loop one update with error @p x and return the control
   *        value the tracker should apply.
   *
   * The PI recurrence is `integ += ki*x; control = integ + kp*x`: the
   * integrator accumulates the running frequency/rate estimate while the
   * proportional term @c kp*x is the instantaneous phase nudge.
   *
   * Fed a constant error with nothing closing the loop, the integrator — and
   * therefore the control — **ramps without bound**; measured at 1.84x
   * between updates 200 and 400 at `bn = 0.02`. That is the accumulation
   * working, not a defect, and it is what pulls a Costas/DLL/timing loop into
   * lock once the loop IS closed, because a converging loop is one whose
   * error is being driven to zero by the correction. Convergence is a
   * property of the closed loop; this function is one term in it.
   *
   * @param state  Must be non-NULL.
   * @param x      Loop error (discriminator output) for this update.
   * @return Control value @c integ+kp*x to drive the NCO / interpolator.
   *
   * @code
   * >>> from doppler.track import LoopFilter
   * >>> lf = LoopFilter(bn=0.02, zeta=0.707, t=1.0)
   * >>> round(lf.step(1.0), 6)   # unit error: control = ki + kp
   * 0.05331
   * >>> round(lf.integ, 6)       # integrator now holds ki
   * 0.001385
   *
   * @endcode
   */
  JM_FORCEINLINE JM_HOT double
  dp_loop_filter_step (dp_loop_filter_state_t *state, double x)
  {
    state->integ += state->ki * x;
    return state->integ + state->kp * x;
  }

  /**
   * @brief Filter a whole block of loop errors, returning the control value
   *        for each update.
   *
   * Equivalent to calling dp_loop_filter_step() once per element of @p x in order,
   * carrying the integrator across the block, so the loop's memory and lock
   * state persist from one call to the next. This is the block path used to
   * run a captured error sequence through the filter in one shot — a plain
   * per-element loop, not a vectorized one: the recurrence is sequential, so
   * each update depends on the one before it.
   *
   * @param state  Component state (mutated across the block).
   * @param x      Loop-error array, one discriminator sample per update.
   * @param out    Control-value array (length >= n; may alias @p x).
   * @param n      Number of updates.
   *
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import LoopFilter
   * >>> lf = LoopFilter(bn=0.05, zeta=0.707, t=1.0)
   * >>> ctl = lf.steps(np.full(50, 0.1))   # constant error into the loop
   * >>> round(float(ctl[0]), 4)            # first control nudge
   * 0.0133
   * >>> round(float(ctl[-1]), 4)           # open loop: ramping
   * 0.0541
   *
   * @endcode
   */
  void dp_loop_filter_steps (dp_loop_filter_state_t *state, const double *x,
                          double *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOOP_FILTER_CORE_H */
