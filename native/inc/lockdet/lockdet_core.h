/**
 * @file lockdet_core.h
 * @brief Portable lock detector — level + time hysteresis over any scalar
 *        lock metric, embeddable in every loop that makes a lock decision.
 *
 * A tracking loop that computes a lock statistic (a CFAR ratio, a coherence
 * metric, an error variance) still needs a *decision rule*: when is the
 * statistic "high enough, long enough" to declare lock, and "low enough,
 * long enough" to drop it? This component is that rule, factored out once:
 *
 *  - **Level hysteresis**: separate declare (@c up_thresh) and drop
 *    (@c down_thresh) thresholds. With `up_thresh >= down_thresh` the band
 *    between them is sticky in both directions — a metric wobbling around a
 *    single threshold cannot chatter the flag.
 *  - **Time hysteresis**: @c n_up consecutive looks above @c up_thresh to
 *    declare, @c n_down consecutive looks below @c down_thresh to drop. A
 *    single contrary look resets the run (consecutive, not cumulative), so
 *    the verify counts compose probabilistically. At per-look false-alarm
 *    rate p the false-declare rate per look is
 *    `p^n_up * (1 - p) / (1 - p^n_up)`, whose reciprocal is exactly
 *    det_verify_delay(p, n_up), the mean looks to a declare. `p^n_up`
 *    alone is the **p -> 0 limit** of that, and is what
 *    det_verify_count() sizes against -- correct to 0.001% at p = 1e-5,
 *    10% at p = 0.1, and **+87% at p = 0.5 with n_up = 4**. Use it as the
 *    budget (it errs high, so it over-provisions n_up) and
 *    det_verify_delay() for the number a caller actually observes.
 *    Measured across p from 0.1 to 0.5 and n_up from 1 to 4:
 *    native/validation/lockdet_verify.c.
 *
 *    (Both the formula and those ranges are written without an indented
 *    block or square brackets on purpose: mkdoxy renders this comment into
 *    markdown, where an indented line is swallowed into the paragraph
 *    before it and a bare `p in [0.1, 0.5]` parses as a link reference and
 *    fails the --strict docs build.)
 *
 *  - **Non-finite looks**: a NaN look is a **miss in both states** — it never
 *    advances a declare, and while locked it advances the drop run like any
 *    other miss, so a metric that goes NaN drops the lock after @c n_down
 *    rather than holding it lit. An unknown lock is not a lock. The policy is
 *    not implemented here: the look is passed through util_core.h's
 *    saturate(), whose @c nan_to parameter documents a lock statistic as the
 *    caller that wants the floor. Only NaN is unordered — the infinities are
 *    ordinary looks (+inf a hit, -inf a miss), and the exclusive edges are
 *    unchanged.
 *
 * The state struct is **public** so a tracker embeds it by value (no heap)
 * and drives it with lockdet_init()/lockdet_step() — e.g. the DLL steps one
 * on its CFAR statistic each N-look decision, the MPSK receiver steps one on
 * the carrier lock metric each recovered symbol. lockdet_create() is the
 * heap path used by the Python wrapper. Pointer-free POD: it rides an
 * embedding composer's whole-struct state snapshot with no extra packing.
 *
 * Lifecycle: `create -> (step / steps / configure / reset)* -> destroy`
 *
 * @code
 * lockdet_state_t d;
 * lockdet_init (&d, 1.5, 1.2, 2, 3);       // declare: 2 looks > 1.5
 * lockdet_reset (&d);                      // cnt = 0, locked = 0
 * int locked = lockdet_step (&d, metric);  // one look -> current flag
 * @endcode
 */
#ifndef LOCKDET_CORE_H
#define LOCKDET_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "util/util_core.h" /* saturate() — the NaN policy, shared */
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Lock-detector state (embeddable by value; pointer-free POD).
   */
  typedef struct
  {
    double up_thresh;   /**< declare side: hit when metric > up_thresh.   */
    double down_thresh; /**< drop side: miss when metric < down_thresh.   */
    uint32_t n_up;      /**< consecutive hits required to declare (>= 1). */
    uint32_t n_down;    /**< consecutive misses required to drop (>= 1).  */
    uint32_t cnt;       /**< running consecutive-look verify counter.     */
    int locked;         /**< current decision (1 = locked).               */
  } lockdet_state_t;

  /**
   * @brief Initialise a lock detector in place (no allocation).
   *
   * Stores the thresholds and verify counts (each count clamped to >= 1; a
   * count of 1 means no time hysteresis on that side). Does **not** touch
   * @c cnt / @c locked, so it doubles as a reconfigure that preserves the
   * current decision. Use this for a `lockdet_state_t` embedded by value;
   * lockdet_create() is calloc + lockdet_init().
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    Declare threshold (hit when metric > up_thresh).
   * @param down_thresh  Drop threshold (miss when metric < down_thresh);
   *                     choose <= up_thresh for level hysteresis.
   * @param n_up         Consecutive hits to declare; clamped to >= 1.
   * @param n_down       Consecutive misses to drop; clamped to >= 1.
   */
  void lockdet_init(lockdet_state_t *state, double up_thresh,
                    double down_thresh, uint32_t n_up, uint32_t n_down);

  /**
   * @brief Create a lockdet instance.
   * @param up_thresh    Declare threshold (hit when metric > up_thresh).
   * @param down_thresh  Drop threshold (miss when metric < down_thresh).
   * @param n_up         Consecutive hits to declare; clamped >= 1 (default 1).
   * @param n_down       Consecutive misses to drop; clamped >= 1 (default 1).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @note Caller must call lockdet_destroy() when done.
   */
  lockdet_state_t *lockdet_create(double up_thresh, double down_thresh,
                                  uint32_t n_up, uint32_t n_down);

  /**
   * @brief Destroy a lockdet instance and release all memory.
   * @param state  May be NULL.
   */
  void lockdet_destroy(lockdet_state_t *state);

  /**
   * @brief Re-tune thresholds and verify counts; preserve the decision.
   *
   * The current @c locked flag survives (a live lock is not dropped by a
   * re-tune); the in-flight verify counter is cleared so the next run is
   * counted entirely under the new config.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    Declare threshold (hit when metric > up_thresh).
   * @param down_thresh  Drop threshold (miss when metric < down_thresh).
   * @param n_up         Consecutive hits to declare; clamped to >= 1.
   * @param n_down       Consecutive misses to drop; clamped to >= 1.
   * @code
   * >>> from doppler.detection import LockDet
   * >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
   * >>> d.configure(up_thresh=3.0, down_thresh=2.5, n_up=1, n_down=1)
   * >>> d.up_thresh          # thresholds re-tuned in place
   * 3.0
   * >>> d.step(4.0)          # a single hit now declares (n_up=1)
   * 1
   *
   * @endcode
   */
  void lockdet_configure(lockdet_state_t *state, double up_thresh,
                         double down_thresh, uint32_t n_up, uint32_t n_down);

  /**
   * @brief Drop the lock and clear the verify counter; keep the config.
   * Returns the detector to the unlocked state with an empty verify run, as if
   * freshly constructed with the same thresholds. Call it at a segment boundary
   * so a decision made on one capture does not leak into an unrelated next one.
   * @param state  Must be non-NULL.
   * @code
   * >>> from doppler.detection import LockDet
   * >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=1, n_down=1)
   * >>> d.step(2.0)          # one hit declares lock (n_up=1)
   * 1
   * >>> d.reset()            # drop it and clear the verify run
   * >>> d.locked
   * False
   *
   * @endcode
   */
  void lockdet_reset(lockdet_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); the decision flag and the
   * in-flight verify run resume exactly.
   */
#define LOCKDET_STATE_MAGIC DP_FOURCC('L', 'K', 'D', 'T')
#define LOCKDET_STATE_VERSION 1u

  /** @brief Serialized-state byte size. */
  size_t lockdet_state_bytes(const lockdet_state_t *state);
  /** @brief Serialize the detector state into @p blob. */
  void lockdet_get_state(const lockdet_state_t *state, void *blob);
  /** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects. */
  int lockdet_set_state(lockdet_state_t *state, const void *blob);

  /**
   * @brief Feed one look of the lock metric; return the current decision.
   *
   * Unlocked: a hit (`x > up_thresh`) advances the verify run and the
   * n_up-th consecutive hit declares lock; any miss resets the run. Locked:
   * a miss (`x < down_thresh`) advances the run and the n_down-th
   * consecutive miss drops the lock; any hit (`x >= down_thresh`) resets
   * it. A metric inside the `[down_thresh, up_thresh]` band is sticky — it
   * neither advances a declare nor a drop.
   *
   * A **non-finite look is a miss in both states**: it never advances a
   * declare, and while locked it advances the drop run like any other miss.
   * An unknown lock is not a lock, which is the rule util_core.h states for
   * lock statistics generally. So a metric that goes NaN drops the lock
   * after @c n_down looks rather than holding it lit indefinitely.
   *
   * @param state  Must be non-NULL.
   * @param x      Lock metric for this look. Non-finite counts as a miss.
   * @return Decision after this look (1 = locked, 0 = not).
   *
   * @code
   * >>> from doppler.detection import LockDet
   * >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
   * >>> [d.step(2.0), d.step(2.0)]     # declared on the 2nd straight hit
   * [0, 1]
   * >>> d.step(1.3)                    # in the hysteresis band: stays up
   * 1
   * >>> [d.step(1.0), d.step(1.0), d.step(1.0)]  # 3rd straight miss drops
   * [1, 1, 0]
   *
   * @endcode
   */
  JM_FORCEINLINE JM_HOT int
  lockdet_step (lockdet_state_t *state, double x)
  {
    /* An unknown lock is not a lock. Send a non-finite look to the floor
       through the SHARED primitive rather than encoding the policy here:
       saturate()'s own documentation names a lock statistic as the caller
       that wants NaN at the floor, and until now no lock detector called
       it, so that paragraph described a caller who did not exist.
       Doing the substitution once, up front, is also what keeps the two
       comparisons below plain. NaN fails every comparison, so a detector
       that handles it inline has to encode the policy in the SPELLING of a
       predicate (`!(x >= t)` rather than `x < t`) — which is subtle enough
       that the drop side was written the other way and held the lock lit
       forever on a dead metric.
       The bounds are infinite because the substitution is the only job:
       every finite look, and both infinities, pass through untouched. */
    x = saturate (x, -INFINITY, INFINITY, -INFINITY);

    if (!state->locked)
      {
        if (x > state->up_thresh)
          {
            if (++state->cnt >= state->n_up)
              {
                state->locked = 1;
                state->cnt    = 0;
              }
          }
        else
          state->cnt = 0;
      }
    else
      {
        if (x < state->down_thresh)
          {
            if (++state->cnt >= state->n_down)
              {
                state->locked = 0;
                state->cnt    = 0;
              }
          }
        else
          state->cnt = 0;
      }
    return state->locked;
  }

  /**
   * @brief Run a block of lock-metric looks through the detector.
   * Applies lockdet_step() to each look in turn, so the decision flag and the
   * in-flight verify run carry across the block exactly as they would look by
   * look — a signal can be processed in frames of any size with no seam.
   * @param state  Component state (mutated). Must be non-NULL.
   * @param x      Lock-metric looks, one scalar per look (length >= n).
   * @param out    Per-look decision output, 0 or 1 (length >= n).
   * @param n      Number of looks to process.
   * @code
   * >>> import numpy as np
   * >>> from doppler.detection import LockDet
   * >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=2)
   * >>> x = np.array([2.0, 2.0, 1.0, 2.0])   # declares on the 2nd hit
   * >>> d.steps(x).tolist()
   * [0, 1, 1, 1]
   *
   * @endcode
   */
  void lockdet_steps (lockdet_state_t *state, const double *x, int *out,
                      size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOCKDET_CORE_H */
