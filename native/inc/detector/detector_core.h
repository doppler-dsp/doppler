/**
 * @file detector_core.h
 * @brief 1-D streaming signal detector with FFT-based correlation,
 *        integrate-and-dump, and configurable noise-referenced threshold.
 *
 * Wraps a corr_state_t (FFT correlator + coherent int-dump) behind a
 * double-mapped ring buffer so that arbitrary-length sample streams can be
 * fed in any chunk size.  After every int-dump a test statistic is computed:
 *
 *   test_stat = peak_mag / noise_est
 *
 * where peak_mag = max|R&#91;τ&#93;| and noise_est is an aggregate (mean, median,
 * min, or max) of |R&#91;τ&#93;| over a user-supplied bin range &#91;noise_lo, noise_hi&#93;.
 * A detection event is emitted when test_stat > threshold (or whenever
 * threshold == 0.0, which means "always fire").
 *
 * The detector operates as a single-threaded object; do not call
 * detector_push() concurrently from multiple threads.
 *
 * Lifecycle:
 * @code
 * float complex ref[N] = { ... };
 * detector_state_t *det = detector_create(ref, N, 1,
 *     1, N-1, DET_NOISE_MEAN, 0.0f, 1);
 * det_result_t results[64];
 * // stream loop
 * while (recv(chunk, CHUNK_SZ)) {
 *     size_t n = detector_push(det, chunk, CHUNK_SZ, results, 64);
 *     for (size_t i = 0; i < n; i++)
 *         printf("lag=%zu stat=%.2f\n", results[i].lag, results[i].test_stat);
 * }
 * detector_destroy(det);
 * @endcode
 */
#ifndef DETECTOR_CORE_H
#define DETECTOR_CORE_H

#include "buffer/buffer.h"
#include "corr/corr_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Noise aggregation mode ─────────────────────────────────────────────── */

/**
 * @brief Selects how noise power is estimated from the correlation magnitude
 *        vector over bins &#91;noise_lo, noise_hi&#93;.
 *
 * The test statistic is peak_mag / noise_est.  A zero noise_est (e.g., when
 * all bins in the range are zero) yields test_stat = 0.
 */
#ifndef DET_NOISE_MODE_T_DEFINED
#define DET_NOISE_MODE_T_DEFINED
typedef enum
{
  DET_NOISE_MEAN = 0,   /**< Arithmetic mean of |R&#91;τ&#93;| over &#91;lo, hi&#93;. */
  DET_NOISE_MEDIAN = 1, /**< Median of |R&#91;τ&#93;| over &#91;lo, hi&#93;.          */
  DET_NOISE_MIN = 2,    /**< Minimum of |R&#91;τ&#93;| over &#91;lo, hi&#93;.         */
  DET_NOISE_MAX = 3,    /**< Maximum of |R&#91;τ&#93;| over &#91;lo, hi&#93;.         */
} det_noise_mode_t;
#endif /* DET_NOISE_MODE_T_DEFINED */

/* ── Per-detection result ───────────────────────────────────────────────── */

/**
 * @brief Detection event returned by detector_push().
 *
 * Fields are filled on every int-dump that passes the threshold test.
 */
typedef struct
{
  size_t lag;       /**< argmax |R&#91;τ&#93;| — lag index of the correlation peak. */
  float peak_mag;   /**< max |R&#91;τ&#93;| (linear magnitude, not power).          */
  float noise_est;  /**< Noise estimate (aggregated |R| in &#91;noise_lo,hi&#93;).   */
  float test_stat;  /**< peak_mag / noise_est; 0 if noise_est == 0.         */
} det_result_t;

/* ── Detector state ─────────────────────────────────────────────────────── */

/**
 * @brief 1-D signal detector state.
 *
 * Allocate with detector_create(); never stack-allocate.
 */
typedef struct
{
  corr_state_t *corr;       /**< FFT correlator + int-dump engine.         */
  dp_f32_t *ring;             /**< Double-mapped ring buffer (auto-sized).    */
  float complex *out_buf;   /**< Corr output buffer (n complex samples).    */
  float *mag_buf;           /**< |out_buf&#91;k&#93;|, n floats.                   */
  float *noise_scratch;     /**< Scratch for median sort.                   */
  size_t n;                 /**< Frame / FFT length in complex samples.     */
  size_t ring_cap;          /**< Ring buffer capacity in complex samples.   */
  size_t noise_lo;          /**< Noise bin range lower bound (inclusive).   */
  size_t noise_hi;          /**< Noise bin range upper bound (inclusive).   */
  det_noise_mode_t noise_mode;
  float threshold;          /**< 0 = always fire; >0 = gate on test_stat.  */
  /* Last dump results — updated on every dump regardless of threshold. */
  size_t peak_lag;
  float peak_mag;
  float noise_est;
  float test_stat;
  int _last_corr_valid;     /**< 1 after the first dump, else 0.           */
} detector_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * @brief Allocate a 1-D streaming signal detector backed by an FFT correlator.
 * Combines a corr_state_t with a double-mapped ring buffer so that arbitrary
 * chunk sizes can be pushed.  After every int-dump the peak-to-noise test
 * statistic is compared against @p threshold; a det_result_t is emitted when
 * it passes.  Setting @p threshold to 0.0 unconditionally fires on every dump.
 * The ring capacity is next_pow2(max(n, 512)) complex samples.
 *
 * @param ref        Reference signal, CF32 ndarray of length n.
 * @param n          Reference / FFT length in complex samples.
 * @param dwell      Int-dump depth; must be >= 1.
 * @param noise_lo   Lower noise bin index (inclusive, 0-based).
 * @param noise_hi   Upper noise bin index (inclusive, < n).
 * @param noise_mode Noise aggregation: "mean", "median", "min", or "max".
 * @param threshold  Test-stat gate; 0.0 = always emit.
 * @param nthreads   Accepted for API compatibility; ignored.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @code
 * >>> from doppler.spectral import Detector
 * >>> import numpy as np
 * >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
 * >>> det = Detector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
 * ...                noise_mode="mean", threshold=0.0)
 * >>> det.n, det.dwell, det.ring_cap
 * (8, 1, 512)
 * @endcode
 */
detector_state_t *detector_create (const float complex *ref, size_t n,
                                   size_t dwell, size_t noise_lo,
                                   size_t noise_hi,
                                   det_noise_mode_t noise_mode,
                                   float threshold, int nthreads);

/** @brief Destroy and free a detector instance.  @param state May be NULL. */
void detector_destroy (detector_state_t *state);

/**
 * @brief Reset the correlator, ring buffer, and last-corr flag.
 * Discards any partial frame buffered in the ring and zeroes the coherent
 * accumulator.  Equivalent to starting fresh from the same reference without
 * rebuilding any internal object.
 *
 * @code
 * >>> from doppler.spectral import Detector
 * >>> import numpy as np
 * >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
 * >>> det = Detector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
 * ...                noise_mode="mean", threshold=0.0)
 * >>> _ = det.push(np.ones(8, dtype=np.complex64))
 * >>> det.reset()
 * >>> det.count
 * 0
 * @endcode
 */
void detector_reset (detector_state_t *state);

/**
 * @brief Replace the reference signal and recompute conj(FFT(ref)).
 *
 * Also resets (see detector_reset()).  The new reference must have the same
 * length @p n that was passed to detector_create().
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, CF32, length state->n.
 */
void detector_set_ref (detector_state_t *state, const float complex *ref);

/**
 * @brief Change the threshold without rebuilding the object.
 *
 * @param state     Must be non-NULL.
 * @param threshold New threshold; 0.0 = always fire.
 */
void detector_set_threshold (detector_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

/**
 * @brief Stream an arbitrary-length CF32 chunk through the detector pipeline.
 * Writes samples into the ring buffer, drains complete n-sample frames
 * through the correlator, and on every int-dump computes the test statistic
 * peak_mag / noise_est.  Detections that pass the threshold are appended to
 * the Python return list as (lag, peak_mag, noise_est, test_stat) tuples.
 * In Python the result is always a list, even when empty.
 *
 * @param state        Allocated detector (non-NULL).
 * @param in           CF32 input chunk of arbitrary length.
 * @param n_in         Number of input samples in @p in.
 * @param result       Caller-supplied array of at least @p max_results
 *                     det_result_t structs; filled on return.
 * @param max_results  Capacity of @p result (maximum detections to emit).
 * @return Number of det_result_t entries written to @p result.
 * @code
 * >>> from doppler.spectral import Detector
 * >>> import numpy as np
 * >>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
 * >>> det = Detector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
 * ...                noise_mode="mean", threshold=0.0)
 * >>> results = det.push(np.ones(8, dtype=np.complex64))
 * >>> len(results)
 * 1
 * >>> lag, peak, noise, stat = results[0]
 * >>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
 * (0, 1.0, 1.0, 1.0)
 * @endcode
 */
size_t detector_push (detector_state_t *state, const float complex *in,
                      size_t n_in, det_result_t *result, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR_CORE_H */
