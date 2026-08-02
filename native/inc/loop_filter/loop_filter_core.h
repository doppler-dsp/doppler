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
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707).
   * @param t      Update period in samples (> 0).
   */
  void loop_filter_init(loop_filter_state_t *state, double bn, double zeta,
                        double t);

  /**
   * @brief Create a loop_filter instance.
   * @param bn    Loop noise bandwidth, normalized cycles/sample (default 0.01).
   * @param zeta  Damping factor (default 0.707).
   * @param t     Update period in samples (default 1.0).
   * @return Heap-allocated state, or NULL on allocation failure.
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
   * proportional term @c kp*x is the instantaneous phase nudge. Fed a constant
   * error the integrator ramps linearly and the control converges to the
   * steady-state estimate — the behaviour that pulls a Costas/DLL/timing loop
   * into lock.
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
   * state persist from one call to the next. This is the vectorized path used
   * to run a captured error sequence through the filter in one shot.
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
   * >>> round(float(ctl[-1]), 4)           # converging toward the estimate
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
