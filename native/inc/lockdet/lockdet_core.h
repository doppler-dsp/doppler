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
 *    the verify counts compose probabilistically: at per-look false-alarm
 *    rate p the false-declare rate is p^n_up. Size the counts with
 *    det_verify_count() and predict the declare latency with
 *    det_verify_delay() (detection module).
 *
 * The state struct is **public** so a tracker embeds it by value (no heap)
 * and drives it with lockdet_init()/lockdet_step() — e.g. the DLL steps one
 * on its CFAR statistic each N-look decision, the MPSK receiver steps one on
 * the carrier lock metric each recovered symbol. lockdet_create() is the
 * heap path used by the Python wrapper. Pointer-free POD: it rides an
 * embedding composer's whole-struct state snapshot with no extra packing.
 *
 * Lifecycle: create -> [step / steps / configure / reset]* -> destroy
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
   */
  void lockdet_configure(lockdet_state_t *state, double up_thresh,
                         double down_thresh, uint32_t n_up, uint32_t n_down);

  /**
   * @brief Drop the lock and clear the verify counter; keep the config.
   * @param state  Must be non-NULL.
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
   * it. A metric inside the [down_thresh, up_thresh] band is sticky — it
   * neither advances a declare nor a drop.
   *
   * @param state  Must be non-NULL.
   * @param x      Lock metric for this look.
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
   * @param state   Component state (mutated).
   * @param input   Metric array (length >= n).
   * @param output  Decision array, 0/1 per look (length >= n).
   * @param n       Number of looks.
   */
  void lockdet_steps (lockdet_state_t *state, const double *input, int *output,
                      size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOCKDET_CORE_H */
