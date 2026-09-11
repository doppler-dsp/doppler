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
 * **Single-row-reference fast path.**  When @p ref is nonzero only in row 0
 * (the DSSS acquisition/detection shape: a code replica with no Doppler
 * content of its own) and @p ny_out == @p ny (no row-axis interpolation),
 * `ref_spec[u,v]` is independent of `u` — the row axis of the 2-D transform
 * pair cancels to an exact identity (DFT orthogonality:
 * `sum_u exp(2*pi*i*u*(i-i')/ny) = ny` iff `i==i'`, else 0), for *any* row
 * content.  `corr2d_execute` then reduces exactly to
 * `R(i,j) = IFFT_nx(FFT_nx(row_i) · conj(FFT_nx(ref_row0)))(j) / nx`,
 * applied independently per row — so `corr2d_create` detects this shape and
 * runs `ny` independent length-`nx` 1-D FFTs instead of a full `(ny,nx)`
 * 2-D FFT, skipping the row-axis work that would otherwise cancel to a
 * no-op.  Any other reference shape, or `ny_out > ny`, uses the general
 * 2-D path unchanged.
 *
 * Lifecycle:
 * @code
 * float _Complex ref[NY * NX] = { ... };    // row-major 2-D reference
 * corr2d_state_t *c = corr2d_create(ref, NY, NX, 4, 1, 0, 0, -1);
 * float _Complex out[NY * NX];
 * for (int i = 0; i < 4; i++) {
 *     size_t n_out = corr2d_execute(c, frame[i], NY*NX, out, NY*NX);
 *     if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
 * }
 * corr2d_destroy(c);
 * @endcode
 */
#ifndef CORR2D_CORE_H
#define CORR2D_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "fft/fft_core.h"
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
  fft2d_state_t *fwd;       /**< Forward 2-D plan (sign = -1) at (ny, nx).
                                 NULL when @ref fast_path.                  */
  fft2d_state_t *inv;       /**< Inverse 2-D plan (sign = +1) at (ny_out,…).
                                 NULL when @ref fast_path.                  */
  float _Complex *ref_spec;  /**< conj(FFT2(ref)), pre-computed.  (ny, nx).
                                 NULL when @ref fast_path (see row_ref_spec). */
  float _Complex *work_fft;  /**< Scratch: FFT(in)·ref_spec product.  (ny,nx)
                                 either path — fast path reinterprets this
                                 as ny independent length-nx row spectra.   */
  float _Complex *accum;     /**< Coherent product-spectrum accumulator, same
                                 (ny,nx)/reinterpretation rule as work_fft.  */
  /* Decoupled-inverse scratch — allocated only when (ny_out,nx_out) differ
   * from (ny,nx); NULL on the native path.  General path: zeropad goes
   * accum -> ztmp (rows) -> work_pad (cols), then inverse(work_pad).  Fast
   * path (nx_out != nx only, ny_out == ny is required for fast_path at all):
   * zeropad goes accum -> work_pad directly, one row at a time, via
   * corr2d_zeropad_1d; ztmp/zcol/zcolout are unused (2-axis-pad only). */
  float _Complex *work_pad;  /**< Zero-padded product, (ny_out, nx_out) or,
                                 fast path, (ny, nx_out).                   */
  float _Complex *ztmp;      /**< Row-padded intermediate, (ny, nx_out).
                                 General path only.                        */
  float _Complex *zcol;      /**< Column gather scratch, (ny). General path
                                 only.                                      */
  float _Complex *zcolout;   /**< Column-padded scratch, (ny_out). General
                                 path only.                                 */
  /* Single-row-reference fast path (see the file doc comment for the
   * identity this relies on).  fast_path is decided once at create() and
   * fixed for the object's lifetime; set_ref() may only refresh within the
   * same mode (see corr2d_set_ref doc comment). */
  int             fast_path;    /**< 1 if using the 1-D-per-row fast path.   */
  fft_state_t    *fwd1d;         /**< Forward 1-D plan, length nx.  Fast only.*/
  fft_state_t    *inv1d;         /**< Inverse 1-D plan, length nx_out.  Fast
                                      only.                                 */
  float _Complex  *row_ref_spec;  /**< conj(FFT_nx(ref row 0)), length nx.
                                      Fast-path replacement for ref_spec.   */
  size_t ny;                /**< Row count.                               */
  size_t nx;                /**< Column count.                            */
  size_t n;                 /**< ny * nx — total element count.           */
  size_t ny_out;            /**< Output rows (== ny unless decoupled).    */
  size_t nx_out;            /**< Output columns (== nx unless decoupled). */
  size_t n_out;             /**< ny_out * nx_out — output element count.  */
  size_t dwell;             /**< Integration depth.                       */
  size_t count;             /**< Frames accumulated (0 … dwell-1).        */
  /* Known-column output (see corr2d_create's @p col_out).  A caller that
   * already knows the correlation lag it wants does not need the other
   * nx_out-1 columns, and evaluating the inverse at one bin is a dot
   * product against the conjugated reference, with NO transform in either
   * direction -- O(nx) per row against the full map's O(nx log nx), which
   * is what makes the kernel affordable for a caller that would otherwise
   * hand-roll the lag sum beside it. */
  int             col_out;   /**< Output column, or < 0 for the full map. */
  float _Complex *col_ref;   /**< conj(ref row 0) in the TIME domain, length
                                  nx; NULL when col_out < 0.              */
  /** Bounce buffer for a dump into an under-sized `out` (jm gh-138).
   *  Both execute paths -- the 2-D inverse and the per-row fast path --
   *  write the full n_out surface, so a short `out` is served by writing
   *  here and copying the prefix. Allocated lazily; the sized path never
   *  touches it. */
  float _Complex *work_trunc;
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
 * @param col_out   Emit ONLY this correlation lag, or < 0 for the whole map.
 *                  When set, a dump writes @p ny values -- one per row --
 *                  instead of ny*nx_out, computed as the time-domain sum
 *                  `sum_p conj(ref[p]) * row[(p + col_out) mod nx]`, which
 *                  is what R(i, col_out) expands to once the 1/nx cancels.
 *                  No transform runs in either direction, so the cost per
 *                  row is O(nx) rather than O(nx log nx).  Requires the
 *                  single-row-reference fast path (a caller that knows its
 *                  lag is by construction correlating against a code
 *                  replica) and the NATIVE output grid: an interpolated
 *                  column is a fractional lag, which no time-domain sum
 *                  produces.  create() returns NULL if @p col_out >= 0 with
 *                  a reference the fast path rejects, with @p ny_out or
 *                  @p nx_out decoupled, or with a lag outside [0, nx).
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
corr2d_state_t *corr2d_create(const float _Complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out, int col_out);

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
 * @brief Replace the reference and recompute its spectrum.
 *
 * Also resets accumulator and counter on success.  The object's fast-path
 * mode (see the file doc comment) is fixed at create() time and never
 * changes here: on a fast-path object, @p ref must still be nonzero only in
 * row 0, or the call is rejected and the object's existing reference and
 * spectrum are left completely untouched (never a silent partial update) —
 * a caller that needs a genuinely different reference shape must build a
 * new corr2d_state_t.  A general-path object accepts any (ny,nx) @p ref and
 * always succeeds.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 * @return 0 on success, -1 if @p state is fast-path and @p ref is no longer
 *         single-row.
 */
int corr2d_set_ref(corr2d_state_t *state, const float _Complex *ref);

/** @brief Maximum output samples per execute call (@ref corr2d_state_t::n_out
 *         -- ny*nx_out normally, or ny when a single @ref
 *         corr2d_state_t::col_out was selected). */
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
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return ny*nx on a dump (or max_out if smaller), 0 otherwise
 *         (None in Python).
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
size_t corr2d_execute(corr2d_state_t *state, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * running 2-D product-spectrum accumulator + frame count;
 * FFT plans + ref_spec are config, rebuilt by create. */
#define CORR2D_STATE_MAGIC DP_FOURCC ('C','R','2','D')
#define CORR2D_STATE_VERSION 1u
size_t corr2d_state_bytes (const corr2d_state_t *state);
void corr2d_get_state (const corr2d_state_t *state, void *blob);
int corr2d_set_state (corr2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
