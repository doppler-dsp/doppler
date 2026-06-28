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
 * Lifecycle: create -> [step / steps / configure / reset]* -> destroy
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
   * @brief Recompute @c kp / @c ki for a new (bn, zeta, t); preserve @c integ.
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalized cycles/sample (>= 0).
   * @param zeta   Damping factor (typically 0.707).
   * @param t      Update period in samples (> 0).
   */
  void loop_filter_configure(loop_filter_state_t *state, double bn, double zeta,
                             double t);

  /**
   * @brief Zero the integrator; keep the configured gains.
   * @param state  Must be non-NULL.
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
   * @brief Advance the loop one update with error @p x; return the control.
   *
   * `integ += ki*x; return integ + kp*x`.
   *
   * @param state  Must be non-NULL.
   * @param x      Loop error.
   * @return Control value (integ + kp*x).
   */
  JM_FORCEINLINE JM_HOT double
  loop_filter_step (loop_filter_state_t *state, double x)
  {
    state->integ += state->ki * x;
    return state->integ + state->kp * x;
  }

  /**
   * @brief Run a block of errors through the loop.
   * @param state   Component state (mutated).
   * @param input   Error array (length >= n).
   * @param output  Control array (length >= n; may alias input).
   * @param n       Number of updates.
   */
  void loop_filter_steps (loop_filter_state_t *state, const double *input,
                          double *output, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOOP_FILTER_CORE_H */
