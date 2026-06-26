/**
 * @file corr2d_core.h
 * @brief 2-D FFT-based cross-correlator with coherent integrate-and-dump.
 *
 * Two-dimensional extension of corr_core: all buffers are ny×nx row-major
 * flat arrays of length ny*nx.  The correlation theorem extends naturally:
 *
 *   `R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`
 *
 * The reference spectrum is pre-computed at create time.  The int-dump
 * semantics are identical to the 1-D case: coherently sum ``dwell``
 * frames, then dump.
 *
 * Lifecycle:
 * @code
 * float complex ref[NY * NX] = { ... };    // row-major 2-D reference
 * corr2d_state_t *c = corr2d_create(ref, NY, NX, 4, 1);
 * float complex out[NY * NX];
 * for (int i = 0; i < 4; i++) {
 *     size_t n_out = corr2d_execute(c, frame[i], NY*NX, out);
 *     if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
 * }
 * corr2d_destroy(c);
 * @endcode
 */
#ifndef CORR2D_CORE_H
#define CORR2D_CORE_H

#include "clib_common.h"
#include "fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 2-D FFT correlator state.
 *
 * Allocate with corr2d_create(); never stack-allocate.  All heap buffers
 * are ``ny * nx`` complex floats stored in row-major order.
 */
typedef struct {
  fft2d_state_t *fwd;       /**< Forward 2-D plan (sign = -1) at (ny, nx).   */
  fft2d_state_t *inv;       /**< Inverse 2-D plan (sign = +1) at (ny_out,…). */
  float complex *ref_spec;  /**< conj(FFT2(ref)), pre-computed.  (ny, nx)    */
  float complex *work_fft;  /**< Scratch: FFT2(in) · ref_spec (product).     */
  float complex *accum;     /**< Coherent product-spectrum accumulator.      */
  /* Decoupled-inverse scratch — allocated only when (ny_out,nx_out) differ
   * from (ny,nx); NULL on the native path.  zeropad goes accum → ztmp (rows)
   * → work_pad (cols), then inverse(work_pad). */
  float complex *work_pad;  /**< Zero-padded product, (ny_out, nx_out).      */
  float complex *ztmp;      /**< Row-padded intermediate, (ny, nx_out).      */
  float complex *zcol;      /**< Column gather scratch, (ny).                */
  float complex *zcolout;   /**< Column-padded scratch, (ny_out).            */
  size_t ny;                /**< Row count.                               */
  size_t nx;                /**< Column count.                            */
  size_t n;                 /**< ny * nx — total element count.           */
  size_t ny_out;            /**< Output rows (== ny unless decoupled).    */
  size_t nx_out;            /**< Output columns (== nx unless decoupled). */
  size_t n_out;             /**< ny_out * nx_out — output element count.  */
  size_t dwell;             /**< Integration depth.                       */
  size_t count;             /**< Frames accumulated (0 … dwell-1).        */
} corr2d_state_t;

/**
 * @brief Allocate a 2-D FFT correlator with coherent integrate-and-dump.
 * Two-dimensional extension of corr_create().  The reference is a flat
 * row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once
 * so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies.
 * The Python wrapper requires @p ref to be a 2-D ndarray with shape
 * (ny, nx); it passes a flat view to C.
 *
 * @param ref       Reference image, 2-D (ny, nx) CF32 ndarray in Python.
 * @param ny        Number of rows in the reference and input frames.
 * @param nx        Number of columns in the reference and input frames.
 * @param dwell     Integration depth; must be >= 1.
 * @param nthreads  Accepted for API compatibility; ignored.
 * @param ny_out    Inverse/output rows; 0 => native (ny).  Must be >= ny.  A
 *                  larger output zero-pads the cross-spectrum before the
 *                  inverse, returning the band-limited (Dirichlet) interpolation
 *                  of the correlation on a finer (ny_out, nx_out) grid — same
 *                  peak, sub-bin resolution.  Native is bit-exact and allocates
 *                  no extra buffers.
 * @param nx_out    Inverse/output columns; 0 => native (nx).  Must be >= nx.
 * @return Heap-allocated state, or NULL on failure.
 * @code
 * >>> from doppler.spectral import Corr2D
 * >>> import numpy as np
 * >>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Corr2D(ref=ref, dwell=1, nthreads=1)
 * >>> c.ny, c.nx, c.dwell, c.count
 * (4, 4, 1, 0)
 * @endcode
 */
corr2d_state_t *corr2d_create(const float complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out);

/** @brief Destroy and free a corr2d instance.  @param state May be NULL. */
void corr2d_destroy(corr2d_state_t *state);

/**
 * @brief Zero the accumulator and reset the integration counter to 0.
 * Equivalent to starting a fresh dwell cycle without rebuilding FFT plans
 * or recomputing ref_spec.
 *
 * @code
 * >>> from doppler.spectral import Corr2D
 * >>> import numpy as np
 * >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Corr2D(ref=ref, dwell=3)
 * >>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
 * >>> c.count
 * 1
 * >>> c.reset()
 * >>> c.count
 * 0
 * @endcode
 */
void corr2d_reset(corr2d_state_t *state);

/**
 * @brief Replace the reference and recompute conj(FFT2(ref)).
 *
 * Also resets accumulator and counter.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 */
void corr2d_set_ref(corr2d_state_t *state, const float complex *ref);

/** @brief Maximum output samples per execute call (always == ny*nx). */
size_t corr2d_execute_max_out(corr2d_state_t *state);

/**
 * @brief Correlate one 2-D frame and optionally dump the coherent accumulator.
 * Runs the 2-D pipeline: FFT2 → pointwise multiply with ref_spec → accumulate
 * the cross-spectrum; on dump, IFFT2 → normalise (÷ ny*nx).  Accumulating in
 * the frequency domain and inverting once is exactly the per-frame inverse
 * summed, by linearity of the IFFT — valid because the dwell is **coherent**
 * (a complex sum); a non-coherent (magnitude) integration could not defer the
 * inverse.  The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns
 * a flat length-ny*nx ndarray, a no-dump returns None.
 *
 * @param state  Allocated 2-D correlator (non-NULL).
 * @param in     Input frame, flat row-major CF32, length ny*nx.
 * @param n_in   Number of input samples; must equal ny*nx.
 * @param out    Output buffer for the correlation map (CF32, length ny*nx);
 *               written only on a dump call.
 * @return ny*nx on a dump, 0 otherwise (None in Python).
 * @code
 * >>> from doppler.spectral import Corr2D
 * >>> import numpy as np
 * >>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
 * >>> c = Corr2D(ref=ref, dwell=2)
 * >>> x = np.ones((2, 2), dtype=np.complex64)
 * >>> c.execute(x) is None   # frame 1 — no dump
 * True
 * >>> c.execute(x).tolist()  # frame 2 — dump
 * [(2+0j), (2+0j), (2+0j), (2+0j)]
 * @endcode
 */
size_t corr2d_execute(corr2d_state_t *state, const float complex *in,
                      size_t n_in, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
