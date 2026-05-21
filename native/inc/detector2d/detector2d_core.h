/**
 * @file detector2d_core.h
 * @brief 2-D streaming signal detector with FFT2D-based correlation,
 *        integrate-and-dump, and configurable noise-referenced threshold.
 *
 * Two-dimensional extension of detector_core.  The input stream is chunked
 * into ny×nx frames (flat row-major CF32).  The test statistic and threshold
 * semantics are identical to the 1-D variant; the only difference is that the
 * peak index maps to a (row, col) pair instead of a single lag.
 *
 * Detection events:
 *   det_result2d_t = { row, col, peak_mag, noise_est, test_stat }
 *
 * Lifecycle:
 * @code
 * float complex ref[NY * NX] = { ... };
 * detector2d_state_t *det = detector2d_create(ref, NY, NX, 1,
 *     0, NY*NX-1, DET_NOISE_MEAN, 0.0f, 1);
 * det_result2d_t results[64];
 * while (recv(chunk, CHUNK_SZ)) {
 *     size_t n = detector2d_push(det, chunk, CHUNK_SZ, results, 64);
 *     for (size_t i = 0; i < n; i++)
 *         printf("row=%zu col=%zu stat=%.2f\n",
 *                results[i].row, results[i].col, results[i].test_stat);
 * }
 * detector2d_destroy(det);
 * @endcode
 */
#ifndef DETECTOR2D_CORE_H
#define DETECTOR2D_CORE_H

#include "buffer/buffer.h"
#include "corr2d/corr2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Noise aggregation mode (shared definition with detector_core.h) ────── */

#ifndef DET_NOISE_MODE_T_DEFINED
#define DET_NOISE_MODE_T_DEFINED
typedef enum
{
  DET_NOISE_MEAN = 0,
  DET_NOISE_MEDIAN = 1,
  DET_NOISE_MIN = 2,
  DET_NOISE_MAX = 3,
} det_noise_mode_t;
#endif /* DET_NOISE_MODE_T_DEFINED */

/* ── Per-detection result ───────────────────────────────────────────────── */

/**
 * @brief Detection event returned by detector2d_push().
 *
 * The peak index in the flat ny×nx correlation map is decomposed into
 * (row, col) so that callers do not need to know nx.
 */
typedef struct
{
  size_t row;       /**< Row of the correlation peak (0-indexed).           */
  size_t col;       /**< Column of the correlation peak (0-indexed).        */
  float peak_mag;   /**< max |R[i,j]| (linear magnitude).                  */
  float noise_est;  /**< Noise estimate aggregated over [noise_lo, hi].     */
  float test_stat;  /**< peak_mag / noise_est; 0 if noise_est == 0.        */
} det_result2d_t;

/* ── Detector2D state ───────────────────────────────────────────────────── */

/**
 * @brief 2-D signal detector state.
 *
 * Allocate with detector2d_create(); never stack-allocate.
 */
typedef struct
{
  corr2d_state_t *corr;     /**< 2-D FFT correlator + int-dump engine.     */
  dp_f32 *ring;             /**< Double-mapped ring buffer (auto-sized).    */
  float complex *out_buf;   /**< Corr2D output (ny*nx complex samples).     */
  float *mag_buf;           /**< |out_buf[k]|, ny*nx floats.               */
  float *noise_scratch;     /**< Scratch for median sort.                   */
  size_t ny;                /**< Number of rows.                            */
  size_t nx;                /**< Number of columns.                         */
  size_t n;                 /**< ny * nx — total frame length.              */
  size_t ring_cap;          /**< Ring buffer capacity in complex samples.   */
  size_t noise_lo;          /**< Noise bin range lower bound (inclusive).   */
  size_t noise_hi;          /**< Noise bin range upper bound (inclusive).   */
  det_noise_mode_t noise_mode;
  float threshold;          /**< 0 = always fire; >0 = gate on test_stat.  */
  /* Last dump results — updated on every dump regardless of threshold. */
  size_t peak_row;
  size_t peak_col;
  float peak_mag;
  float noise_est;
  float test_stat;
  int _last_corr_valid;     /**< 1 after the first dump, else 0.           */
} detector2d_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * @brief Create a 2-D signal detector.
 *
 * @param ref        Reference image, flat row-major CF32, length ny*nx.
 * @param ny         Number of rows.
 * @param nx         Number of columns.
 * @param dwell      Int-dump depth.  Must be >= 1.
 * @param noise_lo   Lower flat-index noise bin (inclusive).
 * @param noise_hi   Upper flat-index noise bin (inclusive, < ny*nx).
 * @param noise_mode Noise aggregation mode.
 * @param threshold  0.0 = always emit a detection.
 * @param nthreads   Passed to corr2d_create(); currently ignored.
 * @return Heap-allocated state, or NULL on allocation failure.
 */
detector2d_state_t *detector2d_create (const float complex *ref, size_t ny,
                                       size_t nx, size_t dwell,
                                       size_t noise_lo, size_t noise_hi,
                                       det_noise_mode_t noise_mode,
                                       float threshold, int nthreads);

/** @brief Destroy and free.  @param state May be NULL. */
void detector2d_destroy (detector2d_state_t *state);

/**
 * @brief Reset correlator, ring buffer, and last-corr flag.
 * @param state Must be non-NULL.
 */
void detector2d_reset (detector2d_state_t *state);

/**
 * @brief Replace the reference image and recompute conj(FFT2(ref)).
 *
 * Also resets.  The new reference must have the same ny*nx total size.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 */
void detector2d_set_ref (detector2d_state_t *state, const float complex *ref);

/**
 * @brief Change threshold without rebuilding.
 * @param state     Must be non-NULL.
 * @param threshold New threshold; 0.0 = always fire.
 */
void detector2d_set_threshold (detector2d_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

/**
 * @brief Push an arbitrary-length CF32 chunk through the 2-D detector.
 *
 * Behaviour is identical to detector_push() except frames have length ny*nx
 * and results carry (row, col) instead of a single lag.
 *
 * @param state       Must be non-NULL.
 * @param in          Input CF32 array of length @p n_in.
 * @param n_in        Number of complex samples to push.
 * @param result      Output array; caller allocates at least @p max_results.
 * @param max_results Maximum detections to store.
 * @return Number of detections stored in @p result[].
 */
size_t detector2d_push (detector2d_state_t *state, const float complex *in,
                        size_t n_in, det_result2d_t *result,
                        size_t max_results);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR2D_CORE_H */
