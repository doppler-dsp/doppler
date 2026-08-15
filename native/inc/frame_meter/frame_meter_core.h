/**
 * @file frame_meter_core.h
 * @brief Frame outcomes accumulated across a record: FER, and sync detection.
 *
 * The fourth metric, and the only one that needs NO TRUTH and still catches a
 * false lock. `ber_evm_db` and `snr_m2m4_db` need no truth either, and a
 * stationary-but-wrong constellation reads clean on both — measured across
 * orders in `test_mpsk_receiver_performance.py`, with the penalty SHRINKING as
 * M rises. BER sees it but needs truth and a trustworthy alignment. A
 * CRC-checked frame needs no payload truth at all: it either checks or it does
 * not, and a false lock fails it. That makes a frame error rate the one metric
 * usable on a real capture that still detects the failure this receiver family
 * is most prone to.
 *
 * ## What a frame outcome is
 *
 * Two independent things can go wrong, and collapsing them loses the
 * diagnosis: the sync word may not be FOUND, or the frame may be found and
 * fail its CRC. Both are frame errors — a frame you did not detect is a frame
 * you did not deliver — but "the sync is too short at this Es/N0" and "the
 * demodulator is making bit errors" are different repairs, so both counts come
 * back separately.
 *
 * A frame carrying no CRC (`crc = -1`, which is exactly what
 * `wfm_frame_crc_ok()` returns for one) counts as delivered when its sync was
 * detected. Counting it as an error instead would make every unprotected frame
 * fail, which is a measurement of the frame format rather than the receiver.
 *
 * ## The stopping rule is the ERROR count, and that is not decoration
 *
 * `ber_confidence()` is the exact Gamma/chi-square interval for INVERSE
 * BINOMIAL sampling — fix the errors, let the trial count fall out. Its
 * relative standard error is `1/sqrt(r)`, a function of the error count ALONE,
 * which is why a run stopped on errors gives a consistent measurement and one
 * stopped on a fixed count does not. This meter therefore uses the same rule
 * as `ber_meter`, exposes the same `enough` read-back, and hands the same
 * interval back. **Reusing that interval under a fixed-frame-count stopping
 * rule would be the wrong sampling model** (that is binomial, and its exact
 * interval is Clopper-Pearson), so the convention is stated here rather than
 * left for a caller to assume.
 *
 * @see docs/design/rx-test.md section 2.5
 */
#ifndef FRAME_METER_CORE_H
#define FRAME_METER_CORE_H

#include "ber/ber_core.h"
#include "dp_state.h"

#include <stddef.h>
#include <stdint.h>
#include "detection/detection_core.h"
#include "ber_meter/ber_meter_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define FRAME_METER_STATE_MAGIC   DP_FOURCC ('F', 'R', 'M', 'M')
#define FRAME_METER_STATE_VERSION 1u

  /** @brief Frame-outcome accumulator. Allocate with frame_meter_create(). */
  typedef struct
  {
    size_t target_errors; /**< config: stop-on-errors target             */
    double conf;          /**< config: confidence level for the interval */
    size_t frames;        /**< running: frames attempted                 */
    size_t sync_detected; /**< running: frames whose sync was found      */
    size_t crc_passed;    /**< running: frames whose CRC checked         */
    size_t errors;        /**< running: frames not delivered             */
  } frame_meter_state_t;

  /**
   * @brief Create an accumulator.
   *
   * @param target_errors  frame errors to accumulate before `enough`; 0 is
   *                       taken as BER_TARGET_ERRORS.
   * @param conf           confidence level in (0, 1); 0 is taken as BER_CONF.
   * @return the meter, or NULL if @p conf is outside (0, 1).
   */
  frame_meter_state_t *frame_meter_create (size_t target_errors, double conf);

  /** @brief Release the meter. */
  void frame_meter_destroy (frame_meter_state_t *state);

  /**
   * @brief Clear every counter; the configuration is untouched.
   *
   * The target and the confidence level are what the caller asked for, so
   * resetting the accumulation must not silently re-negotiate them. Use it
   * between records, or to discard a run that turned out to be measuring the
   * wrong thing.
   *
   * @param state  the meter.
   * @code
   * >>> from doppler.ber import FrameMeter
   * >>> met = FrameMeter(target_errors=10)
   * >>> met.add(1, 0)
   * >>> met.frames, met.errors
   * (1, 1)
   * >>> met.reset()
   * >>> met.frames, met.errors
   * (0, 0)
   *
   * @endcode
   */
  void frame_meter_reset (frame_meter_state_t *state);

  /**
   * @brief Record one frame's outcome.
   *
   * @param state    the meter.
   * @param sync_ok  non-zero when the frame's sync word was detected. Pass the
   *                 detector's own decision — `ber_align_t::ok`, or
   *                 `burst_demod`'s frame_offset validity — never a threshold
   *                 applied afterwards to a statistic.
   * @param crc      `wfm_frame_crc_ok()`'s return, passed straight through:
   *                 1 pass, 0 fail, -1 the frame carries no CRC.
   *
   * A frame counts as an error when its sync was not detected, or when it was
   * and the CRC failed. With `crc = -1` a detected frame counts as delivered,
   * because nothing about it can be checked.
   *
   * @code
   * >>> from doppler.ber import FrameMeter
   * >>> met = FrameMeter(target_errors=10)
   * >>> met.add(1, 1)    # found, and it checked
   * >>> met.add(1, 0)    # found, and the CRC failed
   * >>> met.add(0, 0)    # never found: still a frame you did not deliver
   * >>> met.add(1, -1)   # found, no CRC: delivered but not CHECKED
   * >>> met.frames, met.sync_detected, met.crc_passed, met.errors
   * (4, 3, 1, 2)
   *
   * @endcode
   */
  void frame_meter_add (frame_meter_state_t *state, int sync_ok, int crc);

  /** @brief Frames attempted. */
  size_t frame_meter_get_frames (const frame_meter_state_t *state);
  /** @brief Frames whose sync word was detected. */
  size_t frame_meter_get_sync_detected (const frame_meter_state_t *state);
  /** @brief Frames whose CRC checked. */
  size_t frame_meter_get_crc_passed (const frame_meter_state_t *state);
  /** @brief Frames not delivered: no sync, or a failed CRC. */
  size_t frame_meter_get_errors (const frame_meter_state_t *state);

  /**
   * @brief Non-zero once `target_errors` frame errors have accumulated.
   *
   * The stopping condition, so a caller loops records until the measurement
   * has the precision it asked for rather than until a frame count someone
   * guessed.
   */
  int frame_meter_get_enough (const frame_meter_state_t *state);

  /**
   * @brief Frame error rate with its exact interval.
   *
   * `ber_confidence(errors, frames, conf)` — the same interval `ber_meter`
   * reports, which is generic over trials and therefore applies to frames
   * unchanged. Assert on `lo`, never on `p_hat`.
   *
   * @param state  the meter.
   * @return the rate with its exact interval.
   * @code
   * >>> from doppler.ber import FrameMeter
   * >>> met = FrameMeter(target_errors=4)
   * >>> for i in range(20):
   * ...     met.add(1, 0 if i % 5 == 0 else 1)
   * >>> met.enough
   * 1
   * >>> fer = met.fer()
   * >>> round(fer.p_hat, 3), fer.lo < fer.p_hat < fer.hi
   * (0.158, True)
   *
   * @endcode
   */
  ber_interval_t frame_meter_fer (const frame_meter_state_t *state);

  /**
   * @brief Sync MISS rate with its exact interval.
   *
   * Reported as a miss rate rather than a detection rate so it is an ERROR
   * rate like every other number here, and so the same interval applies
   * without reinterpretation. **This is what turns "is this sync word long
   * enough at this Es/N0" into a measurement** — `ber_align_detect()` already
   * returns `margin_db` and `runner_db` per attempt, and accumulating the
   * decisions is what answers the question with a number.
   *
   * @param state  the meter.
   * @return the miss rate with its exact interval.
   * @code
   * >>> from doppler.ber import FrameMeter
   * >>> met = FrameMeter()
   * >>> for i in range(50):
   * ...     met.add(0 if i % 10 == 0 else 1, 1)
   * >>> met.frames, met.sync_detected
   * (50, 45)
   * >>> miss = met.sync_miss()
   * >>> round(miss.p_hat, 3), miss.hi > miss.p_hat
   * (0.082, True)
   *
   * @endcode
   */
  ber_interval_t frame_meter_sync_miss (const frame_meter_state_t *state);

  /** @brief Serialized-state byte size. */
  size_t frame_meter_state_bytes (const frame_meter_state_t *state);
  /** @brief Serialize the running counters into @p blob. */
  void frame_meter_get_state (const frame_meter_state_t *state, void *blob);
  /** @brief Restore; DP_OK, or DP_ERR_INVALID if the blob is rejected. */
  int frame_meter_set_state (frame_meter_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_METER_CORE_H */
