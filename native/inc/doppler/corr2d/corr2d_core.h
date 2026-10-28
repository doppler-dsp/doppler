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
 * content.  `dp_corr2d_execute` then reduces exactly to
 * `R(i,j) = IFFT_nx(FFT_nx(row_i) · conj(FFT_nx(ref_row0)))(j) / nx`,
 * applied independently per row — so `dp_corr2d_create` detects this shape and
 * runs `ny` independent length-`nx` 1-D FFTs instead of a full `(ny,nx)`
 * 2-D FFT, skipping the row-axis work that would otherwise cancel to a
 * no-op.  Any other reference shape, or `ny_out > ny`, uses the general
 * 2-D path unchanged.
 *
 * Lifecycle:
 * @code
 * float _Complex ref[NY * NX] = { ... };    // row-major 2-D reference
 * dp_corr2d_state_t *c = dp_corr2d_create(ref, NY, NX, 4, 1, 0, 0, -1);
 * float _Complex out[NY * NX];
 * for (int i = 0; i < 4; i++) {
 *     size_t n_out = dp_corr2d_execute(c, frame[i], NY*NX, out, NY*NX);
 *     if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
 * }
 * dp_corr2d_destroy(c);
 * @endcode
 */
#ifndef DP_CORR2D_CORE_H
#define DP_CORR2D_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/fft/fft_core.h"
#include "doppler/fft2d/fft2d_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 2-D FFT correlator state.
 *
 * Allocate with dp_corr2d_create(); never stack-allocate.  All heap buffers
 * are ``ny * nx`` complex floats stored in row-major order.
 */
typedef struct {
  dp_fft2d_state_t *fwd;       /**< Forward 2-D plan (sign = -1) at (ny, nx).
                                 NULL when @ref fast_path.                  */
  dp_fft2d_state_t *inv;       /**< Inverse 2-D plan (sign = +1) at (ny_out,…).
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
   * same mode (see dp_corr2d_set_ref doc comment). */
  int             fast_path;    /**< 1 if using the 1-D-per-row fast path.   */
  dp_fft_state_t    *fwd1d;         /**< Forward 1-D plan, length nx.  Fast only.*/
  dp_fft_state_t    *inv1d;         /**< Inverse 1-D plan, length nx_out.  Fast
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
  /* Known-column output (see dp_corr2d_create's @p col_out).  A caller that
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
  /* Worker fan-out (#2666).  The fast path's per-row stage and the
   * delay-Doppler flavor's per-column stage are each a set of independent
   * transforms, run over `nworkers` contiguous chunks on `pool`.  A pocketfft
   * plan keeps scratch of its own, so it cannot be shared between threads:
   * every worker owns one plan per stage, and chunk t always runs on plan t,
   * which is what keeps the output bit-identical across thread counts. */
  int              nworkers;  /**< Chunks per stage (1 = serial).          */
  dp_pool_t       *pool;      /**< Persistent workers; NULL when serial.  */
  dp_fft_state_t **row_fwd;   /**< Per-worker forward row plans, length
                                   nx; worker 0's is @ref fwd1d.  Fast
                                   path only.                             */
  dp_fft_state_t **row_inv;   /**< Per-worker inverse row plans, length
                                   nx_out; worker 0's is @ref inv1d.  Fast
                                   path only.                             */
  /* Delay-Doppler flavor (dp_corr2d_create_delay_doppler).  The flavor is
   * the fast path plus a slow-time FFT down every delay column at dump
   * time; it is fixed at create() and changes what the state blob means,
   * so the blob carries its own magic. */
  int              delay_doppler; /**< 1 for the delay-Doppler flavor.     */
  double           doppler_step;  /**< Doppler spacing between adjacent
                                       output rows, cycles/sample:
                                       1 / (ny_out * nx).  0 for Corr2D.   */
  dp_fft_state_t **slow_fft;  /**< Per-worker slow-time plans, length
                                   ny_out.  Delay-Doppler only.           */
  float _Complex  *col_gather;/**< Per-worker column tiles, nworkers x
                                   CORR2D_COL_TILE x ny_out; rows ny..
                                   ny_out-1 of each column stay zero (the
                                   slow-time zero-pad).  Delay-Doppler.   */
  float _Complex  *col_spec;  /**< Per-worker transformed tiles, same
                                   shape as @ref col_gather.              */
} dp_corr2d_state_t;

/**
 * @brief Allocate a 2-D FFT correlator with coherent integrate-and-dump.
 * Two-dimensional extension of dp_corr_create().  The reference is a flat
 * row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once
 * so each execute() call costs two 2-D FFTs plus ny*nx complex multiplies.
 * The Python wrapper requires @p ref to be a 2-D ndarray with shape
 * (ny, nx); it passes a flat view to C.
 *
 * @param ref       Reference image, 2-D (ny, nx) CF32 ndarray in Python.
 * @param ny        Number of rows in the reference and input frames.
 * @param nx        Number of columns in the reference and input frames.
 * @param dwell     Integration depth; must be >= 1.
 * @param nthreads  Workers for the single-row-reference fast path's per-row
 *                  stage: 1 runs serially (no threads), N runs N workers
 *                  (the caller included), <= 0 uses every online core.  The
 *                  count is capped at @p ny, one row per worker at most.
 *                  The output is bit-identical whatever the count.  The
 *                  general 2-D path and the @p col_out path run serially and
 *                  ignore it.
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
 *                  over p of `conj(ref(p)) * row((p + col_out) mod nx)`,
 *                  which is what R(i, col_out) expands to once the 1/nx
 *                  cancels.
 *                  No transform runs in either direction, so the cost per
 *                  row is O(nx) rather than O(nx log nx).  Requires the
 *                  single-row-reference fast path (a caller that knows its
 *                  lag is by construction correlating against a code
 *                  replica) and the NATIVE output grid: an interpolated
 *                  column is a fractional lag, which no time-domain sum
 *                  produces.  create() returns NULL if @p col_out >= 0 with
 *                  a reference the fast path rejects, with @p ny_out or
 *                  @p nx_out decoupled, or with a lag outside 0 .. nx-1.
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
dp_corr2d_state_t *dp_corr2d_create(const float _Complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads, size_t ny_out,
                              size_t nx_out, int col_out);

/**
 * @brief Allocate the delay-Doppler flavor: a code correlator that maps a raw
 * stream of @p ny code periods onto (Doppler, delay).
 *
 * Input to dp_corr2d_execute() is the raw stream -- @p ny periods of
 * @p code_len samples, back to back, no reshaping or reference padding by the
 * caller.  Each period (row) is correlated against @p code exactly as
 * Corr2D's single-row fast path does (FFT, multiply by `conj(FFT(code))`,
 * coherent @p dwell accumulation of the product spectrum, inverse scaled by
 * `1/nx`), and on the dump the slow-time FFT runs down every delay column.
 * In numpy terms, for one frame `x` (dwell 1):
 *
 *     C   = np.conj(np.fft.fft(code))
 *     R   = np.fft.ifft(np.fft.fft(x.reshape(ny, nx), axis=1) * C, axis=1)
 *     map = np.fft.fftshift(np.fft.fft(R, n=ny_out, axis=0), axes=0)
 *
 * The output is `ny_out x nx` row-major: rows are Doppler bins, centered
 * (zero Doppler on row `ny_out / 2`), columns are delay lags.  A signal of
 * amplitude `a` that is the code delayed by `d` samples and rotated by `f`
 * cycles/sample peaks at column `d` and row `ny_out/2 + f / doppler_step`,
 * where @ref dp_corr2d_state_t::doppler_step is `1 / (ny_out * nx)`
 * cycles/sample (times the sample rate for Hz).  Its peak is
 * `a * dwell * ny * sum(abs(code)**2)`: the 1/nx of the row inverse
 * cancels the row correlation's own gain of nx, and the slow-time FFT is
 * unnormalized, as numpy's is, so it sums the ny rows coherently.
 *
 * `ny_out > ny` zero-pads the slow-time samples after the ny rows: the
 * ny_out-point DFT then samples the same Doppler response on a grid ny_out/ny
 * times finer (band-limited interpolation).  The pad adds no energy, so the
 * peak at a bin center is unchanged and a Doppler between native bins is
 * found at its fractional row.
 *
 * Both stages fan out over @p nthreads workers -- the rows, then the
 * columns -- each worker on its own FFT plans, and the result is
 * bit-identical for every thread count.
 *
 * @param code      The code replica, @p code_len samples (one period).
 * @param code_len  Samples per period, nx; must be >= 1.
 * @param ny        Periods per frame, the slow-time length; must be >= 1.
 * @param dwell     Frames coherently accumulated per dump; must be >= 1.
 * @param nthreads  Workers: 1 serial, N for N (the caller included), <= 0
 *                  every online core; capped at the most chunks a stage
 *                  can use, `max(ny, ceil(code_len / 16))`.
 * @param ny_out    Doppler bins in the map; 0 means @p ny.  Must be >= @p ny.
 * @return Heap-allocated state (free with dp_corr2d_destroy()), or NULL if
 *         @p code is NULL, @p code_len or @p ny is 0, @p dwell is 0, or
 *         @p ny_out is below @p ny.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d, k = 16, 64, 5, 3       # delay 5, Doppler 3 bins
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> n = np.arange(ny * nx)
 * >>> f = k / (ny * nx)                 # cycles/sample
 * >>> x = (np.tile(np.roll(code, d), ny)
 * ...      * np.exp(2j * np.pi * f * n)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny, dwell=2)
 * >>> out = np.empty(dd.execute_max_out(), np.complex64)
 * >>> for _ in range(2):                # one preallocated map per dump
 * ...     m = dd.execute(x, out=out)
 * >>> m is not None and m.base is out
 * True
 * >>> row, col = np.unravel_index(np.argmax(np.abs(out)), (ny, nx))
 * >>> int(row) - ny // 2, int(col)      # (Doppler bins, delay)
 * (3, 5)
 * >>> dd.doppler_step == 1 / (ny * nx)
 * True
 * @endcode
 */
dp_corr2d_state_t *dp_corr2d_create_delay_doppler (const float _Complex *code,
                                                   size_t code_len, size_t ny,
                                                   size_t dwell, int nthreads,
                                                   size_t ny_out);

/** @brief Destroy and free a corr2d instance.  @param state May be NULL. */
void dp_corr2d_destroy(dp_corr2d_state_t *state);

/**
 * @brief Zero the accumulator and reset the integration counter to 0.
 * Equivalent to starting a fresh dwell cycle without rebuilding FFT plans
 * or recomputing ref_spec.
 *
 * @param state  Must be non-NULL.
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
void dp_corr2d_reset(dp_corr2d_state_t *state);

/**
 * @brief Replace the reference and recompute its spectrum.
 *
 * Also resets accumulator and counter on success.  The object's fast-path
 * mode (see the file doc comment) is fixed at create() time and never
 * changes here: on a fast-path object, @p ref must still be nonzero only in
 * row 0, or the call is rejected and the object's existing reference and
 * spectrum are left completely untouched (never a silent partial update) —
 * a caller that needs a genuinely different reference shape must build a
 * new dp_corr2d_state_t.  A general-path object accepts any (ny,nx) @p ref and
 * always succeeds.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference, flat row-major CF32, length ny*nx.
 * @return 0 on success, -1 if @p state is fast-path and @p ref is no longer
 *         single-row.
 */
int dp_corr2d_set_ref(dp_corr2d_state_t *state, const float _Complex *ref);

/** @brief Maximum output samples per execute call (@ref dp_corr2d_state_t::n_out
 *         -- ny_out*nx_out normally, or ny when a single @ref
 *         dp_corr2d_state_t::col_out was selected).
 *  @param state  Must be non-NULL.
 *  @return n_out, fixed at create(). */
size_t dp_corr2d_execute_max_out(dp_corr2d_state_t *state);

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
 * @param n_in   Number of input samples; must equal ny*nx; else REFUSED.
 * @param out    Output buffer for the correlation map (CF32, length n_out:
 *               ny_out*nx_out, or ny with @p col_out); written only on a
 *               dump call.
 * @param max_out Capacity of @p out in elements. Emission stops there, so the
 *               return value is the number actually written.
 * @return n_out on a dump (or @p max_out if smaller), 0 otherwise (None in
 *         Python).  If @p n_in is not the required length the call is
 *         REFUSED with nothing read, written or counted: SIZE_MAX in C,
 *         ValueError in Python (#1925).
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
size_t dp_corr2d_execute(dp_corr2d_state_t *state, const float _Complex *in,
                      size_t n_in, float _Complex *out, size_t max_out);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * running 2-D product-spectrum accumulator + frame count;
 * FFT plans + ref_spec are config, rebuilt by create. */
/** @brief State-blob magic ('CR2D'); set_state() rejects any other. */
#define CORR2D_STATE_MAGIC DP_FOURCC ('C','R','2','D')
/** @brief State-blob layout version; set_state() rejects any other. */
#define CORR2D_STATE_VERSION 1u
/** @brief The delay-Doppler flavor's state-blob magic ('DDMP'): a Corr2D
 *  blob and a delay-Doppler blob each refuse to restore into the other. */
#define DELAY_DOPPLER_STATE_MAGIC DP_FOURCC ('D','D','M','P')
/** @brief The delay-Doppler flavor's blob layout version. */
#define DELAY_DOPPLER_STATE_VERSION 1u
/**
 * @brief Size in bytes of this correlator's state blob.
 * @param state  Must be non-NULL.
 * @return Header + frame count (u64) + ny*nx complex-float accumulator;
 *         the delay-Doppler flavor adds its ny and ny_out (u64 each). 
 */
size_t dp_corr2d_state_bytes (const dp_corr2d_state_t *state);
/**
 * @brief Serialize the frame count and spectrum accumulator into @p blob.
 * @param state  Must be non-NULL.
 * @param blob   Destination; at least dp_corr2d_state_bytes() bytes.
 */
void dp_corr2d_get_state (const dp_corr2d_state_t *state, void *blob);
/**
 * @brief Restore a mid-dwell correlator from dp_corr2d_get_state()'s blob.
 * @param state  Must be non-NULL, created with the same (ny, nx) and
 *               reference (the reference is not in the blob).
 * @param blob   Source blob of dp_corr2d_state_bytes() bytes.
 * @return `DP_OK`, or `DP_ERR_INVALID` (state untouched) when the header's
 *         magic, version, endianness or size does not match -- including a
 *         blob of the other flavor -- or, for the delay-Doppler flavor, when
 *         the blob's ny or ny_out differs from @p state's or its frame count
 *         is not below @p state's dwell.
 */
int dp_corr2d_set_state (dp_corr2d_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
