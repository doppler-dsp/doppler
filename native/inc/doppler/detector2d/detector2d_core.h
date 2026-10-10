/**
 * @file detector2d_core.h
 * @brief 2-D streaming signal detector with FFT2D-based correlation,
 *        integrate-and-dump, and configurable noise-referenced threshold.
 *
 * Two-dimensional extension of detector_core.  The input stream is chunked
 * into ny×nx frames (flat row-major CF32) by the ring's framed face
 * (DECLARE_DP_BUFFER_FRAMES), so the detections are a function of the input
 * stream, not of how it was split into calls.  The test statistic and
 * threshold semantics are identical to the 1-D variant; the only difference
 * is that the peak index maps to a (row, col) pair instead of a single lag.
 *
 * Detection events:
 *   det_result2d_t = { row, col, peak_mag, noise_est, test_stat }
 *
 * Lifecycle:
 * @code
 * float _Complex ref[NY * NX] = { ... };
 * dp_detector2d_state_t *det = dp_detector2d_create(ref, NY, NX, 1,
 *     0, NY*NX-1, DET_NOISE_MEAN, 0.0f, 1);
 * det_result2d_t results[64];
 * // a full results[] stops a push, and the input it did not take is
 * // offered again -- dp_detector2d_consumed(det) says where it stopped
 * while (recv(chunk, CHUNK_SZ)) {
 *     for (size_t off = 0; off < CHUNK_SZ;
 *          off += dp_detector2d_consumed(det)) {
 *         size_t n = dp_detector2d_push(det, chunk + off, CHUNK_SZ - off,
 *                                       results, 64);
 *         for (size_t i = 0; i < n; i++)
 *             printf("row=%zu col=%zu stat=%.2f\n",
 *                    results[i].row, results[i].col, results[i].test_stat);
 *     }
 * }
 * dp_detector2d_destroy(det);
 * @endcode
 */
#ifndef DP_DETECTOR2D_CORE_H
#define DP_DETECTOR2D_CORE_H

#include "doppler/corr2d/corr2d_core.h"
#include "doppler/dp_state.h"
#include "doppler/f32_buffer/f32_buffer_core.h"

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
 * @brief Detection event returned by dp_detector2d_push().
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

/**
 * @brief One listed peak of a surface: a cell and its value, in the
 *        surface's own units (a magnitude on a coherent surface, a power on
 *        a non-coherent one). What det_peak_list() (det_private.h) returns,
 *        and what the acquisition engine keeps per dwell.
 */
#ifndef DET_PEAK_T_DEFINED
#define DET_PEAK_T_DEFINED
typedef struct
{
  size_t row;   /**< surface row                                    */
  size_t col;   /**< surface column                                 */
  float  value; /**< the cell's value                               */
} det_peak_t;
#endif

/* ── Detector2D state ───────────────────────────────────────────────────── */

/**
 * @brief 2-D signal detector state.
 *
 * Allocate with dp_detector2d_create(); never stack-allocate.
 */
typedef struct
{
  dp_corr2d_state_t *corr;     /**< 2-D FFT correlator + int-dump engine.     */
  dp_f32_t *ring;             /**< The carry's storage: bound to @c framer,
                                   freed by destroy.                        */
  dp_f32_framer_t framer;     /**< Any chunk in, ny*nx-sample frames out.    */
  float _Complex *out_buf;   /**< Corr2D output (ny*nx complex samples).     */
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
  int _last_corr_valid;     /**< 1 once a dump has filled out_buf; 0
                                 after create, reset and set_state.     */
  size_t consumed;          /**< Input samples the last push took.          */
} dp_detector2d_state_t;

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * @brief Allocate a 2-D streaming signal detector backed by a 2-D correlator.
 * Two-dimensional extension of dp_detector_create().  Input frames are flat
 * row-major CF32 arrays of length ny*nx, cut from the stream by the ring's
 * framer.  On
 * every int-dump the peak flat index is decomposed into (row, col) and a
 * det_result2d_t is emitted when test_stat > threshold.  The Python wrapper
 * accepts a (ny, nx) CF32 ndarray for both @p ref and the push input.
 *
 * @param ref        2-D reference image, (ny, nx) CF32 ndarray in Python.
 * @param ny         Number of rows in the reference and input frames.
 * @param nx         Number of columns in the reference and input frames.
 * @param dwell      Int-dump depth; must be >= 1.
 * @param noise_lo   Lower flat-index noise bin (inclusive, 0-based).
 * @param noise_hi   Upper flat-index noise bin (inclusive, < ny*nx).  A
 *                  value at or beyond the window clamps to ny*nx - 1, so
 *                  the default sentinel selects the full window.
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
dp_detector2d_state_t *dp_detector2d_create (const float _Complex *ref, size_t ny,
                                       size_t nx, size_t dwell,
                                       size_t noise_lo, size_t noise_hi,
                                       det_noise_mode_t noise_mode,
                                       float threshold, int nthreads);

/** @brief Destroy and free.  @param state May be NULL. */
void dp_detector2d_destroy (dp_detector2d_state_t *state);

/**
 * @brief Reset the 2-D correlator, the carry, and last-corr flag.
 * Discards any partial frame carried between pushes and zeroes the coherent
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
void dp_detector2d_reset (dp_detector2d_state_t *state);

/**
 * @brief Replace the reference image and recompute its spectrum.
 *
 * Always resets (the carry, corr2d accumulator, last-dump bookkeeping), even if
 * the new reference is subsequently rejected.  The new reference must have
 * the same ny*nx total size; see dp_corr2d_set_ref() for the single-row-fast-
 * path rejection rule this forwards.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 * @return 0 on success, -1 if rejected by dp_corr2d_set_ref().
 */
int dp_detector2d_set_ref (dp_detector2d_state_t *state, const float _Complex *ref);

/**
 * @brief Change threshold without rebuilding.
 * @param state     Must be non-NULL.
 * @param threshold New threshold; 0.0 = always fire.
 */
void dp_detector2d_set_threshold (dp_detector2d_state_t *state, float threshold);

/* ── Stream push ────────────────────────────────────────────────────────── */

/**
 * @brief Stream an arbitrary-length CF32 chunk through the 2-D detector.
 * The same pipeline as dp_detector_push(), except that frames are ny*nx
 * complex samples and each detection event carries (row, col) for the peak
 * location instead of a single lag index.  In Python the result is always a
 * list of (row, col, peak_mag, noise_est, test_stat) tuples.
 *
 * Python's push() has room for 1024 detections a call.  Once a push fills
 * it, every later frame of that call is lost, whether or not it would have
 * made a detection; the stream stays frame-aligned, so the next push starts
 * on a frame boundary and its peaks keep their (row, col).  Keep a chunk
 * under 1024 frames.  Before v0.66 the room was 64, and a push past it kept
 * up to ring_cap/n - 1 of those frames for the next call and dropped the
 * rest.  #1992 and just-buildit/just-makeit#2184 track sizing the list to
 * the call.
 *
 * @param state        Allocated 2-D detector (non-NULL).
 * @param in           CF32 input chunk of arbitrary length.
 * @param n_in         Number of input samples in @p in.
 * @param result       Caller-supplied array of at least @p max_results
 *                     det_result2d_t structs; filled on return.
 * @param max_results  Capacity of @p result (maximum detections to emit).
 *                     A full @p result never loses input: a frame yields at
 *                     most one detection, and once @p result is full the
 *                     push takes nothing more, so it stops on the boundary
 *                     of its last frame.  dp_detector2d_consumed() says how
 *                     many samples it took, and the caller offers the rest
 *                     again.  Input that runs out mid-frame is the carry,
 *                     held inside (fewer than ny*nx samples).  A push with
 *                     room 0 takes nothing, so a resume loop needs room for
 *                     at least one; with that, a push of any input takes at
 *                     least one sample.
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
size_t dp_detector2d_push (dp_detector2d_state_t *state, const float _Complex *in,
                        size_t n_in, det_result2d_t *result,
                        size_t max_results);

/**
 * @brief Input samples the last dp_detector2d_push() took.
 *
 * Equal to its @p n_in unless @p result filled up; then the caller resumes
 * at in + consumed. 0 after create, reset, set_ref and set_state.
 *
 * @param state  Must be non-NULL.
 */
size_t dp_detector2d_consumed (const dp_detector2d_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * corr2d child + the framer's carry (its own child blob, fewer than ny*nx
 * samples) + the last-dump result fields; scratch is config (rebuilt by
 * create).  Version 2: the carry was the ring's raw contents, zero-padded to
 * ring_cap. */
#define DETECTOR2D_STATE_MAGIC DP_FOURCC ('D','E','T','2')
#define DETECTOR2D_STATE_VERSION 2u
size_t dp_detector2d_state_bytes (const dp_detector2d_state_t *state);
void dp_detector2d_get_state (const dp_detector2d_state_t *state, void *blob);
int dp_detector2d_set_state (dp_detector2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DETECTOR2D_CORE_H */
