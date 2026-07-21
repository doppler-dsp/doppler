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
#include "dp_state.h"

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
  float peak_mag;   /**< max |R&#91;i,j&#93;| (linear magnitude).                  */
  float noise_est;  /**< Noise estimate aggregated over &#91;noise_lo, hi&#93;.     */
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
  dp_f32_t *ring;             /**< Double-mapped ring buffer (auto-sized).    */
  float complex *out_buf;   /**< Corr2D output (ny*nx complex samples).     */
  float *mag_buf;           /**< |out_buf&#91;k&#93;|, ny*nx floats.               */
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
 * @brief Allocate a 2-D streaming signal detector backed by a 2-D correlator.
 * Two-dimensional extension of detector_create().  Input frames are flat
 * row-major CF32 arrays of length ny*nx streamed through a ring buffer.  On
 * every int-dump the peak flat index is decomposed into (row, col) and a
 * det_result2d_t is emitted when test_stat > threshold.  The Python wrapper
 * accepts a (ny, nx) CF32 ndarray for both @p ref and the push input.
 *
 * @param ref        2-D reference image, (ny, nx) CF32 ndarray in Python.
 * @param ny         Number of rows in the reference and input frames.
 * @param nx         Number of columns in the reference and input frames.
 * @param dwell      Int-dump depth; must be >= 1.
 * @param noise_lo   Lower flat-index noise bin (inclusive, 0-based).
 * @param noise_hi   Upper flat-index noise bin (inclusive, < ny*nx).
 * @param noise_mode Noise aggregation: "mean", "median", "min", or "max".
 * @param threshold  Test-stat gate; 0.0 = always emit.
 * @param nthreads   Accepted for API compatibility; ignored.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @code
 * >>> from doppler.spectral import CorrDetector2D
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
 * ...                  noise_mode="mean", threshold=0.0)
 * >>> det.ny, det.nx, det.n, det.dwell
 * (4, 4, 16, 1)
 * @endcode
 */
detector2d_state_t *detector2d_create (const float complex *ref, size_t ny,
                                       size_t nx, size_t dwell,
                                       size_t noise_lo, size_t noise_hi,
                                       det_noise_mode_t noise_mode,
                                       float threshold, int nthreads);

/** @brief Destroy and free.  @param state May be NULL. */
void detector2d_destroy (detector2d_state_t *state);

/**
 * @brief Reset the 2-D correlator, ring buffer, and last-corr flag.
 * Discards any partial frame buffered in the ring and zeroes the coherent
 * accumulator.  The reference spectrum and FFT plans are preserved.
 *
 * @code
 * >>> from doppler.spectral import CorrDetector2D
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
 * ...                  noise_mode="mean", threshold=0.0)
 * >>> _ = det.push(np.ones((4, 4), dtype=np.complex64))
 * >>> det.reset()
 * >>> det.count
 * 0
 * @endcode
 */
void detector2d_reset (detector2d_state_t *state);

/**
 * @brief Replace the reference image and recompute its spectrum.
 *
 * Always resets (ring, corr2d accumulator, last-dump bookkeeping), even if
 * the new reference is subsequently rejected.  The new reference must have
 * the same ny*nx total size; see corr2d_set_ref() for the single-row-fast-
 * path rejection rule this forwards.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 * @return 0 on success, -1 if rejected by corr2d_set_ref().
 */
int detector2d_set_ref (detector2d_state_t *state, const float complex *ref);

/**
 * @brief Change threshold without rebuilding.
 * @param state     Must be non-NULL.
 * @param threshold New threshold; 0.0 = always fire.
 */
void detector2d_set_threshold (detector2d_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

/**
 * @brief Stream an arbitrary-length CF32 chunk through the 2-D detector.
 * Identical to detector_push() except frames are ny*nx complex samples and
 * each detection event carries (row, col) for the peak location instead of a
 * single lag index.  In Python the result is always a list of
 * (row, col, peak_mag, noise_est, test_stat) tuples.
 *
 * @param state        Allocated 2-D detector (non-NULL).
 * @param in           CF32 input chunk of arbitrary length.
 * @param n_in         Number of input samples in @p in.
 * @param result       Caller-supplied array of at least @p max_results
 *                     det_result2d_t structs; filled on return.
 * @param max_results  Capacity of @p result (maximum detections to emit).
 * @return Number of det_result2d_t entries written to @p result.
 * @code
 * >>> from doppler.spectral import CorrDetector2D
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
 * ...                  noise_mode="mean", threshold=0.0)
 * >>> results = det.push(np.ones((4, 4), dtype=np.complex64))
 * >>> len(results)
 * 1
 * >>> row, col, peak, noise, stat = results[0]
 * >>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
 * (0, 0, 1.0, 1.0, 1.0)
 * @endcode
 */
size_t detector2d_push (detector2d_state_t *state, const float complex *in,
                        size_t n_in, det_result2d_t *result,
                        size_t max_results);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * corr2d child + the input ring's unconsumed samples (zero-padded to ring_cap)
 * + the last-dump result fields; scratch is config (rebuilt by create). */
#define DETECTOR2D_STATE_MAGIC DP_FOURCC ('D','E','T','2')
#define DETECTOR2D_STATE_VERSION 1u
size_t detector2d_state_bytes (const detector2d_state_t *state);
void detector2d_get_state (const detector2d_state_t *state, void *blob);
int detector2d_set_state (detector2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR2D_CORE_H */
