/**
 * @file loop_filter_core.h
 * @brief Second-order proportional-integral loop filter — the shared engine
 *        of every tracking loop (Costas/PLL, DLL, symbol timing).
 *
 * An error @c e in, a control value out: `control = integ + kp*e`, with the
 * integrator advancing `integ += ki*e`. The integrator therefore holds the
 * running frequency/rate estimate; `kp*e` is the instantaneous (phase) nudge.
 * Gains @c kp / @c ki come from a loop noise bandwidth, damping, and update
 * period via the standard 2nd-order form (loop_filter_init()).
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
 * The promise assumes the REST of the loop has unit gain (`Kd*K0 = 1`): a
 * discriminator whose slope is 4 delivers a loop four times wider than the
 * @c bn it was handed, and nothing here can detect that. See
 * docs/design/loop-filter.md §3.
 *
 * Settling follows from the same number: a step settles to +-5% within about
 * 2.3 loop constants (`2.3/bn` updates) at zeta = 0.707, so the `5/bn` rule
 * used throughout this library is comfortable rather than tight.
 *
 * The state struct is **public** so a tracker can embed it by value (no heap)
 * and drive it with loop_filter_init()/loop_filter_step() — e.g. a despreader
 * keeps one for the carrier loop and one for the code loop.
 * loop_filter_create() is the heap path used by the Python wrapper.
 *
 * Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`
 *
 * @code
 * loop_filter_state_t *lf = loop_filter_create(0.01, 0.707, 1.0);
 * double ctl = loop_filter_step(lf, 0.25);   // integ += ki*e; ret integ+kp*e
 * loop_filter_destroy(lf);
 * @endcode
 */
#ifndef LOOP_FILTER_CORE_H
#define LOOP_FILTER_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
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
    double zeta;  /**< damping factor (0.707 = critically damped).     */
    double t;     /**< update period in samples.                       */
  } loop_filter_state_t;

  /**
   * @brief Initialise a loop filter in place (no allocation).
   *
   * Computes @c kp / @c ki from the loop noise bandwidth @p bn (normalized,
   * cycles/sample), damping @p zeta, and update period @p t (samples), and
   * stores @p bn / @p zeta / @p t. Does **not** touch @c integ, so it doubles
   * as a reconfigure that preserves lock. Use this for a `loop_filter_state_t`
   * embedded by value; loop_filter_create() is calloc + loop_filter_init().
   *
   * **Arguments are NOT validated here, on purpose.** This is the by-value
   * path taken by the objects that embed a filter, all of which validate
   * upstream; loop_filter_create() is the boundary that faces an untrusted
   * caller and it rejects the same domain this documents. Passing `t = 0`
   * here yields `kp = ki = 0` — a loop that never moves — and a non-finite
   * argument yields NaN gains that never recover.
   *
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707), > 0.
   * @param t      Update period in samples (> 0).
   */
  void loop_filter_init(loop_filter_state_t *state, double bn, double zeta,
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
   * Unguarded, like loop_filter_init() and for the same reason: this is the
   * trusting path, and loop_filter_create() is the boundary that rejects the
   * domain. `zeta = 0` divides by zero here exactly as it always has.
   *
   * @param bn    Loop noise bandwidth, normalized (>= 0).
   * @param zeta  Damping factor (typically 0.707), > 0.
   * @return      The natural frequency, in @p bn's units.
   */
  double loop_filter_wn(double bn, double zeta);

  /**
   * @brief Create a loop_filter instance, validating its arguments.
   *
   * This is the untrusted boundary — the Python constructor passes a
   * caller's arbitrary doubles here — so unlike loop_filter_init() it
   * **rejects** anything outside the declared domain rather than computing
   * gains from it. `bn = 0` is inside the domain and is accepted: it means a
   * deliberately frozen loop.
   *
   * @param bn    Loop noise bandwidth, normalized cycles/sample; >= 0 and
   *              finite (default 0.01).
   * @param zeta  Damping factor; > 0 and finite (default 0.707).
   * @param t     Update period in samples; > 0 and finite (default 1.0).
   * @return Heap-allocated state, or NULL if any argument is outside the
   *         domain above or on allocation failure. The Python binding turns
   *         the former into a @c ValueError.
   * @note Caller must call loop_filter_destroy() when done.
   */
  loop_filter_state_t *loop_filter_create(double bn, double zeta, double t);

  /**
   * @brief Destroy a loop_filter instance and release all memory.
   * @param state  May be NULL.
   */
  void loop_filter_destroy(loop_filter_state_t *state);

  /**
   * @brief Retune the loop gains @c kp / @c ki for a new (bn, zeta, t) without
   *        disturbing the integrator.
   *
   * Recomputes the proportional and integral gains from the standard 2nd-order
   * form but leaves @c integ untouched, so a loop can be widened for fast
   * acquisition and then narrowed for steady-state tracking while holding its
   * accumulated frequency/rate estimate — the retune preserves lock.
   *
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707).
   * @param t      Update period in samples (> 0).
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
  void loop_filter_configure(loop_filter_state_t *state, double bn, double zeta,
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
  void loop_filter_reset(loop_filter_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); config fields restore identically
   * into an identically-built instance, the integrator memory resumes exactly.
   */
#define LOOP_FILTER_STATE_MAGIC DP_FOURCC('L', 'P', 'F', 'L')
#define LOOP_FILTER_STATE_VERSION 1u

  /** @brief Serialized-state byte size. */
  size_t loop_filter_state_bytes(const loop_filter_state_t *state);
  /** @brief Serialize the loop state into @p blob. */
  void loop_filter_get_state(const loop_filter_state_t *state, void *blob);
  /** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects. */
  int loop_filter_set_state(loop_filter_state_t *state, const void *blob);

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
  loop_filter_step (loop_filter_state_t *state, double x)
  {
    state->integ += state->ki * x;
    return state->integ + state->kp * x;
  }

  /**
   * @brief Filter a whole block of loop errors, returning the control value
   *        for each update.
   *
   * Equivalent to calling loop_filter_step() once per element of @p x in order,
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
  void loop_filter_steps (loop_filter_state_t *state, const double *x,
                          double *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOOP_FILTER_CORE_H */
