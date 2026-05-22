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
 * where peak_mag = max|R[τ]| and noise_est is an aggregate (mean, median,
 * min, or max) of |R[τ]| over a user-supplied bin range [noise_lo, noise_hi].
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
 *        vector over bins [noise_lo, noise_hi].
 *
 * The test statistic is peak_mag / noise_est.  A zero noise_est (e.g., when
 * all bins in the range are zero) yields test_stat = 0.
 */
#ifndef DET_NOISE_MODE_T_DEFINED
#define DET_NOISE_MODE_T_DEFINED
typedef enum
{
  DET_NOISE_MEAN = 0,   /**< Arithmetic mean of |R[τ]| over [lo, hi]. */
  DET_NOISE_MEDIAN = 1, /**< Median of |R[τ]| over [lo, hi].          */
  DET_NOISE_MIN = 2,    /**< Minimum of |R[τ]| over [lo, hi].         */
  DET_NOISE_MAX = 3,    /**< Maximum of |R[τ]| over [lo, hi].         */
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
  size_t lag;       /**< argmax |R[τ]| — lag index of the correlation peak. */
  float peak_mag;   /**< max |R[τ]| (linear magnitude, not power).          */
  float noise_est;  /**< Noise estimate (aggregated |R| in [noise_lo,hi]).   */
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
  float *mag_buf;           /**< |out_buf[k]|, n floats.                   */
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
 * @brief Create a 1-D signal detector.
 *
 * Allocates a corr_state_t, a dp_f32_t ring buffer of capacity
 * next_pow2(max(n, 512)), and all scratch buffers.  The ring buffer
 * satisfies the dp_f32_create() page-alignment constraint automatically.
 *
 * @param ref        Reference signal, CF32, length @p n.  May be freed after
 *                   this call returns.
 * @param n          Frame / FFT length in complex samples.  Must be >= 1.
 * @param dwell      Int-dump depth.  Must be >= 1.
 * @param noise_lo   Lower noise bin (inclusive, 0 <= noise_lo <= noise_hi).
 * @param noise_hi   Upper noise bin (inclusive, noise_hi < n).
 * @param noise_mode Aggregation mode for noise estimation.
 * @param threshold  Test-stat threshold.  0.0 = always emit a detection.
 * @param nthreads   Passed through to corr_create(); currently ignored.
 * @return Heap-allocated state, or NULL on allocation failure.
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
 *
 * Discards any partial frame buffered in the ring.  Equivalent to starting
 * fresh from the same reference.
 *
 * @param state Must be non-NULL.
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
 * @brief Push an arbitrary-length CF32 chunk through the detector.
 *
 * Writes @p n_in complex samples into the ring buffer in the minimum number
 * of chunks that fit, then drains all complete n-sample frames through the
 * correlator.  On every int-dump a test statistic is computed; if it passes
 * the threshold, a det_result_t is appended to @p result[].  The function
 * returns as soon as @p n_in samples have been consumed or @p max_results
 * detections have been stored, whichever comes first.
 *
 * The @p result array must be pre-allocated by the caller.  A stack array of
 * 64 elements is sufficient for any realistic push size:
 * @code
 * det_result_t buf[64];
 * size_t n = detector_push(det, chunk, len, buf, 64);
 * @endcode
 *
 * @param state       Must be non-NULL.
 * @param in          Input CF32 array of length @p n_in.
 * @param n_in        Number of complex samples to push.
 * @param result      Output array; caller allocates at least @p max_results.
 * @param max_results Maximum detections to store; prevents unbounded output.
 * @return Number of detections stored in @p result[].
 */
size_t detector_push (detector_state_t *state, const float complex *in,
                      size_t n_in, det_result_t *result, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR_CORE_H */
