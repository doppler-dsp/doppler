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
  /* Batch scratch (dp_corr2d_execute_fold and the typed batch calls),
   * allocated on the first batch, one slot per worker.  A batch makes its
   * maps here and never touches accum or count, so it is not state and is
   * not in the blob. */
  float _Complex  *bat_acc;   /**< Per-worker accumulators, nworkers x n;
                                   zero between maps.                    */
  float _Complex  *bat_work;  /**< Per-worker forward spectra, nworkers x n. */
  float _Complex  *bat_pad;   /**< Per-worker row zero-pad, nworkers x ny x
                                   nx_out; fast path with nx_out > nx only. */
  float _Complex  *bat_map;   /**< Per-worker maps, nworkers x n_out.    */
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

/* ── Batches: many frames in one call (#2683) ──────────────────────────────
 *
 * A batch is H frames back to back, n_in = H * ny * nx.  Map h is exactly
 * what a fresh object with dwell 1 returns for frame h -- the same
 * arithmetic, bit for bit, on every path (general 2-D, single-row fast
 * path, col_out, delay-Doppler) and at every thread count.  A batch never
 * reads or writes the dwell: accum, count and the state blob are the same
 * after it as before, so a batch may run in the middle of a dwell.
 *
 * Each typed call folds the H maps into a caller-owned result, in h order.
 * Every one refuses, with nothing written and SIZE_MAX returned, an empty
 * batch (n_in == 0), a length that is not a whole number of frames, or an
 * output with room for less than the whole result: a truncated reduction
 * has no meaning, so a batch never truncates the way execute() does.
 *
 * Threads: with two frames or more, each worker makes whole maps on its own
 * plans and scratch, and the maps are folded in h order, because splitting
 * one short frame across a pool costs more than it saves
 * (corr2d-hypotheses.md section 5).  A batch of one frame is split as
 * execute() splits it.  The coherent sum gives each worker a fixed range of
 * rows through every frame.  The result is the same at every thread
 * count. */

/**
 * @brief A C reducer: called once per map, in h order, by
 *        dp_corr2d_execute_fold().
 *
 * @p map holds @p n_out values laid out as execute() writes them, and is
 * valid only for the duration of the call.
 */
typedef struct
{
  /** Fold map @p h into the result @p ctx names. */
  void (*fold) (void *ctx, size_t h, const float _Complex *map, size_t n_out);
  void *ctx; /**< Passed to every call of @ref fold. */
} dp_corr2d_fold_t;

/**
 * @brief Make the map of every frame in a batch and hand each to a reducer.
 *
 * The one loop every typed batch call runs on.  Maps are made as described
 * above and passed to `fold->fold(fold->ctx, h, map, n_out)` for h = 0 ..
 * H-1, in that order, from the calling thread.
 *
 * @param state  Must be non-NULL.
 * @param in     H frames, each ny * nx samples (CF32), back to back.
 * @param n_in   Samples in @p in: a nonzero multiple of ny * nx.
 * @param fold   The reducer; must be non-NULL.
 * @return H, or SIZE_MAX (nothing folded) when @p n_in is 0 or not a
 *         multiple of the frame.
 * @code
 * #include "doppler/corr2d/corr2d_core.h"
 * #include <stdio.h>
 *
 * // A reducer: the energy of each map, one value per frame.
 * static void
 * energy (void *ctx, size_t h, const float _Complex *m, size_t n_out)
 * {
 *   double e = 0.0;
 *   for (size_t k = 0; k < n_out; k++)
 *     e += crealf (m[k]) * crealf (m[k]) + cimagf (m[k]) * cimagf (m[k]);
 *   ((double *)ctx)[h] = e;
 * }
 *
 * int
 * main (void)
 * {
 *   const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 *   dp_corr2d_state_t   *dd
 *       = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1, 0);
 *   float _Complex frames[3 * 2 * 4]; // 3 frames of ny x nx = 2 x 4
 *   for (size_t k = 0; k < 3 * 2 * 4; k++)
 *     frames[k] = code[k % 4] * (float)(1 + k / 8); // frame h is (h+1) x
 *   double           e[3];
 *   dp_corr2d_fold_t f = { energy, e };
 *   size_t           H = dp_corr2d_execute_fold (dd, frames, 3 * 2 * 4, &f);
 *   printf ("%zu maps, energy ratios %.0f %.0f\n", H, e[1] / e[0],
 *           e[2] / e[0]); // 3 maps, energy ratios 4 9
 *   dp_corr2d_destroy (dd);
 *   return !(H == 3 && e[1] > 3.99 * e[0] && e[1] < 4.01 * e[0]);
 * }
 * @endcode
 */
size_t dp_corr2d_execute_fold (dp_corr2d_state_t *state,
                               const float _Complex *in, size_t n_in,
                               const dp_corr2d_fold_t *fold);

/**
 * @brief Samples dp_corr2d_execute_all() writes for @p n_in input samples:
 *        H * n_out, H = n_in / (ny * nx).
 * @param state  Must be non-NULL.
 * @param n_in   Input samples about to be passed.
 * @return H * n_out, or SIZE_MAX if that overflows.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_all_max_out(len(frames))   # 2 maps of 8 x 32
 * 512
 * @endcode
 */
size_t dp_corr2d_execute_all_max_out (const dp_corr2d_state_t *state,
                                      size_t n_in);

/**
 * @brief Every map of a batch, unreduced: map h at `out + h * n_out`.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      H * n_out values (CF32).
 * @param max_out  Capacity of @p out; at least
 *                 dp_corr2d_execute_all_max_out().
 * @return H * n_out, or SIZE_MAX (nothing written) for a refused batch.
 *         From Python, an (H, n_out) array, a view of `out=` when given.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> maps = dd.execute_all(frames)
 * >>> maps.shape
 * (2, 256)
 * >>> [int(np.argmax(np.abs(m))) % nx for m in maps]   # each map's delay
 * [3, 11]
 * >>> dd.count                    # the dwell never moved
 * 0
 * @endcode
 */
size_t dp_corr2d_execute_all (dp_corr2d_state_t *state,
                              const float _Complex *in, size_t n_in,
                              float _Complex *out, size_t max_out);

/**
 * @brief Samples dp_corr2d_execute_coherent() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_coherent_max_out()
 * 256
 * @endcode
 */
size_t dp_corr2d_execute_coherent_max_out (const dp_corr2d_state_t *state);

/**
 * @brief The coherent sum of a batch's maps, inverted once.
 *
 * The frames' product spectra are summed and the sum is inverted, as a
 * dwell does: the result is bit-identical to what a fresh object with
 * dwell = H dumps for the same frames, and equal to the sum of the H maps
 * up to rounding.  The object's own dwell is untouched.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out values (CF32).
 * @param max_out  Capacity of @p out; at least n_out.
 * @return n_out, or SIZE_MAX (nothing written) for a refused batch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> x = np.tile(code, ny)
 * >>> total = dd.execute_coherent(np.concatenate([x, -x]))
 * >>> bool(np.abs(total).max() < 1e-3)          # x and -x cancel
 * True
 * @endcode
 */
size_t dp_corr2d_execute_coherent (dp_corr2d_state_t *state,
                                   const float _Complex *in, size_t n_in,
                                   float _Complex *out, size_t max_out);

/**
 * @brief Powers dp_corr2d_execute_power_sum() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> dd.execute_power_sum_max_out()
 * 256
 * @endcode
 */
size_t dp_corr2d_execute_power_sum_max_out (const dp_corr2d_state_t *state);

/**
 * @brief The power of a batch's maps, summed cell by cell: each output
 *        cell is the sum over h of that cell's `|map_h|^2`, summed in h
 *        order in float.
 *
 * Unlike the coherent sum, phase does not cancel: frames x and -x sum to
 * twice one map's power, where their coherent sum is zero.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out powers (float).
 * @param max_out  Capacity of @p out; at least n_out.
 * @return n_out, or SIZE_MAX (nothing written) for a refused batch.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> x = np.tile(code, ny)
 * >>> power = dd.execute_power_sum(np.concatenate([x, -x]))
 * >>> one = np.abs(dd.execute_all(x)[0]) ** 2
 * >>> bool(np.allclose(power, 2 * one))        # x and -x do not cancel
 * True
 * @endcode
 */
size_t dp_corr2d_execute_power_sum (dp_corr2d_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, size_t max_out);

/**
 * @brief Values per output dp_corr2d_execute_power_max() writes: n_out.
 * @param state  Must be non-NULL.
 * @return n_out, fixed at create().
 * @code
 * const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 * dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1,
 *                                                          0);
 * size_t n = dp_corr2d_execute_power_max_max_out (dd); // 2 x 4 = 8
 * dp_corr2d_destroy (dd);
 * return n != 8;
 * @endcode
 */
size_t dp_corr2d_execute_power_max_max_out (const dp_corr2d_state_t *state);

/**
 * @brief The largest power over a batch's maps, cell by cell, and the map
 *        it came from.
 *
 * Each cell of @p out is the largest `|map_h|^2` that cell takes over the
 * maps, and the same cell of @p out1 is the h it came from.  A tie goes to
 * the lowest h: a later map replaces the running maximum only when it is
 * strictly greater.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32); H at most 2^32.
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      n_out powers (float).
 * @param out1     n_out map indices (uint32).
 * @param max_out  Capacity of each of @p out and @p out1; at least n_out.
 * @return n_out, or SIZE_MAX (nothing written) for a refused batch or
 *         H above 2^32.  From Python, the tuple (power, index).
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> power, index = dd.execute_power_max(frames)
 * >>> int(index[4 * nx + 3]), int(index[4 * nx + 11])  # which map peaks
 * (0, 1)
 * @endcode
 */
size_t dp_corr2d_execute_power_max (dp_corr2d_state_t *state,
                                    const float _Complex *in, size_t n_in,
                                    float *out, uint32_t *out1,
                                    size_t max_out);

/**
 * @brief Records dp_corr2d_execute_peaks() writes for @p n_in samples: H.
 * @param state  Must be non-NULL.
 * @param n_in   Input samples about to be passed.
 * @return n_in / (ny * nx).
 * @code
 * const float _Complex code[4] = { 1.0f, 1.0f, -1.0f, 1.0f };
 * dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 4, 2, 1, 1,
 *                                                          0);
 * size_t h = dp_corr2d_execute_peaks_max_out (dd, 3 * 2 * 4); // 3 frames
 * dp_corr2d_destroy (dd);
 * return h != 3;
 * @endcode
 */
size_t dp_corr2d_execute_peaks_max_out (const dp_corr2d_state_t *state,
                                        size_t n_in);

/**
 * @brief One record per map of a batch: its peak power, where the peak is,
 *        and its mean power.
 *
 * Entry h of each output describes map h: @p out its largest `|map|^2`,
 * @p out1 and @p out2 the row and column of that cell (the first in
 * row-major order on a tie), and @p out3 the mean `|map|^2` over its n_out
 * cells.  A map is ny_out x nx_out; with col_out it is ny x 1, and the
 * column reported is col_out.  The peak-to-mean of each map is @p out over
 * @p out3, entry by entry: a statistic of each map, which the same
 * statistic of a pointwise maximum over maps does not give.
 *
 * @param state    Must be non-NULL.
 * @param in       H frames back to back (CF32).
 * @param n_in     A nonzero multiple of ny * nx.
 * @param out      H peak powers (float).
 * @param out1     H peak rows (uint32).
 * @param out2     H peak columns (uint32).
 * @param out3     H mean powers (float).
 * @param max_out  Capacity of each output; at least H.
 * @return H, or SIZE_MAX (nothing written) for a refused batch.  From
 *         Python, the tuple (peak, row, col, mean).
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx = 8, 32
 * >>> code = np.exp(2j * np.pi * rng.random(nx)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> frames = np.concatenate([np.tile(np.roll(code, d), ny)
 * ...                          for d in (3, 11)])   # delays 3 and 11
 * >>> peak, row, col, mean = dd.execute_peaks(frames)
 * >>> row.tolist(), col.tolist()     # zero Doppler is row ny / 2
 * ([4, 4], [3, 11])
 * >>> bool(np.all(peak / mean > 100))   # each map's own peak-to-mean
 * True
 * @endcode
 */
size_t dp_corr2d_execute_peaks (dp_corr2d_state_t *state,
                                const float _Complex *in, size_t n_in,
                                float *out, uint32_t *out1, uint32_t *out2,
                                float *out3, size_t max_out);

/* ── The hypothesis bank (#2683 phase 3) ───────────────────────────────── */

/**
 * @brief Run one frame through a bank of carrier and rate hypotheses and hand
 *        each map to a reducer.
 *
 * A bank is what a frame batch of corrected copies would compute, without
 * the copies: carrier g corrects an offset of `g / (L * nx)` cycles/sample,
 * for every g from @p g_min to @p g_max, and each rate r weights the
 * slow-time samples by the r-th ny-long vector of @p rates.  Map h, for
 * `h = (g - g_min) * R + r` (R = `rates_len / ny`, or 1 with no rates), is
 * what a fresh dwell-1 object's execute() returns for the frame with period
 * i multiplied by element i of rate vector r and by
 * `exp(-j 2 pi g n / (L nx))`, where n
 * counts samples WITHIN period i (0 .. nx-1), up to float rounding.
 *
 * The correction restarts at every period on purpose: the phase a carrier
 * adds between periods is slow-time Doppler, which the slow-time FFT
 * measures, so a signal stays on its own absolute Doppler row under every
 * carrier hypothesis (docs/design/corr2d-hypotheses.md, rule 4).
 *
 * Every path runs a bank, and only the single-row fast path and DelayDoppler
 * share work: whole bins (g a multiple of L) are a roll of the row spectrum,
 * each fraction `g mod L` mixes and transforms the rows once for every whole
 * bin that shares it, and each carrier inverts its rows once for all its
 * rates.  On DelayDoppler a rate pays only the slow-time FFT; on Corr2D,
 * whose rows are its periods, it scales the carrier's rows.  The general 2-D
 * path and a known column run each hypothesis's corrected frame through
 * their own frame code, which is the definition with nothing shared.
 *
 * Maps reach @p fold in h order, so a reducer's tie rule is the batch's.  The
 * stages split across the object's workers exactly as execute() does, and
 * every map is bit-identical for any worker count.  The dwell accumulator,
 * `count` and the state blob are never read or written.
 *
 * @param state      Any Corr2D or DelayDoppler object.
 * @param x          One frame, @p x_len samples.
 * @param x_len      Must be exactly `ny * nx`.
 * @param L          Fractions per whole bin; at least 1.
 * @param g_min      Lowest carrier, in units of `1 / (L * nx)` cycles/sample.
 * @param g_max      Highest carrier; at least @p g_min.
 * @param rates      Per-period weights, R vectors of ny, or NULL with
 *                   @p rates_len 0 (one unit vector).
 * @param rates_len  A whole multiple of ny, 0 included.
 * @param fold       The reducer, called once per map.
 * @return The number of maps, `(g_max - g_min + 1) * R`; or SIZE_MAX, with
 *         nothing folded, for a frame of another length, `L == 0`,
 *         `g_min > g_max`, a @p rates_len that is not a whole number of
 *         ny-long vectors, a count past 2^32, or scratch that cannot be
 *         allocated.
 * @code
 * #include "doppler/corr2d/corr2d_core.h"
 * #include <stdio.h>
 * static void count_maps (void *ctx, size_t h, const float _Complex *map,
 *                         size_t n_out)
 * {
 *   (void)h, (void)map, (void)n_out;
 *   ++*(size_t *)ctx;
 * }
 * int main (void)
 * {
 *   float _Complex code[8] = { 1, -1, 1, 1, -1, 1, -1, -1 }, x[4 * 8] = { 0 };
 *   dp_corr2d_state_t *dd = dp_corr2d_create_delay_doppler (code, 8, 4, 1, 1, 0);
 *   size_t maps = 0;
 *   dp_corr2d_fold_t f = { count_maps, &maps };
 *   size_t h = dp_corr2d_bank_fold (dd, x, 4 * 8, 4, -4, 4, NULL, 0, &f);
 *   printf ("%zu %zu\n", h, maps); // 9 carriers, one rate: 9 maps
 *   dp_corr2d_destroy (dd);
 *   return !(h == 9 && maps == 9);
 * }
 * @endcode
 */
size_t dp_corr2d_bank_fold (const dp_corr2d_state_t *state, const float _Complex *x,
                            size_t x_len, size_t L, int g_min, int g_max,
                            const float _Complex *rates, size_t rates_len,
                            const dp_corr2d_fold_t *fold);

/**
 * @brief Capacity dp_corr2d_bank_power_max() needs: one map.
 * @param state  The correlator.
 * @param x_len  The frame's length (one map whatever it is).
 * @return `n_out`.
 */
size_t dp_corr2d_bank_power_max_max_out (dp_corr2d_state_t *state,
                                         size_t x_len);

/**
 * @brief The strongest power at every cell over a bank, and which
 *        hypothesis it came from.
 *
 * dp_corr2d_bank_fold() with the `POWER_MAX` reducer: at each cell, @p out
 * holds the largest |map_h|^2 over every hypothesis h, and @p out1 that h,
 * `(g - g_min) * R + r`, the lowest h on a tie.  Every argument and refusal
 * is dp_corr2d_bank_fold()'s; in addition @p max_out below `n_out` is
 * refused with nothing written.
 *
 * @param state, x, x_len, L, g_min, g_max, rates, rates_len
 *                  As dp_corr2d_bank_fold().
 * @param out       `n_out` powers.
 * @param out1      `n_out` hypothesis indices.
 * @param max_out   Capacity of each output; at least `n_out`.
 * @return `n_out`, or SIZE_MAX with nothing written.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d = 8, 32, 5                  # code delayed by 5 samples
 * >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
 * >>> m = np.arange(ny * nx)
 * >>> x = (np.tile(np.roll(code, d), ny)    # on a carrier 1.5 bins off
 * ...      * np.exp(2j * np.pi * 1.5 / nx * m)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> no_rates = np.empty(0, np.complex64)
 * >>> pw, h = dd.bank_power_max(x, 2, -4, 4, no_rates)  # g / (2 nx), -4..4
 * >>> cell = int(np.argmax(pw))
 * >>> cell % nx, int(h[cell]) - 4           # (delay, g = h + g_min)
 * (5, 3)
 * @endcode
 */
size_t dp_corr2d_bank_power_max (const dp_corr2d_state_t *state,
                                 const float _Complex *x, size_t x_len,
                                 size_t L, int g_min, int g_max,
                                 const float _Complex *rates,
                                 size_t rates_len, float *out, uint32_t *out1,
                                 size_t max_out);

/**
 * @brief Capacity dp_corr2d_bank_detect() needs: one record per map cell.
 * @param state  The correlator.
 * @param x_len  The frame's length (the bound does not depend on it).
 * @return `n_out`.
 */
size_t dp_corr2d_bank_detect_max_out (dp_corr2d_state_t *state,
                                      size_t x_len);

/**
 * @brief Detect on a coarse carrier grid, refine each candidate at its own
 *        cell on a fine one, and keep what confirms.
 *
 * The bank searches on @p L_d and resolves on @p L_r
 * (docs/design/corr2d-hypotheses.md section 6):
 *
 * 1. **Detect.** dp_corr2d_bank_power_max() over carriers @p g_min ..
 *    @p g_max on the `1 / (L_d nx)` grid, with every rate.
 * 2. **Candidates.** Every cell whose power exceeds @p t_candidate and is
 *    at least each of its 8 neighbors, both axes circular (a delay lag and a
 *    Doppler bin both wrap).
 * 3. **Refine.** For a candidate found by coarse carrier g_d and rate r, the
 *    map value at its cell under every fine carrier g_r from
 *    `m g_d - (m - 1)` to `m g_d + (m - 1)`, m = `L_r / L_d`: the carriers
 *    between its coarse neighbors, on the `1 / (L_r nx)` grid.  On the fast
 *    path and DelayDoppler with a native delay axis each is a time-domain
 *    correlation at the cell's lag and a slow-time DFT at its Doppler row, in
 *    double, with no FFT: `2 ny nx` multiply-adds a carrier.  Any other
 *    object makes that hypothesis's map and reads the cell.  The best g_r
 *    wins, the lowest on a tie.
 * 4. **Confirm.** A candidate whose refined power exceeds @p t_confirm is a
 *    record.
 *
 * Records come out in cell order (row-major), one entry of each output per
 * record: @p out the refined power, @p out1 g_r on the fine grid, @p out2
 * the rate r, @p out3 the row and @p out4 the column, the cell as execute()
 * lays out the map.  Measured at 64 x 512 against an oracle
 * that corrects the exact offset, @p L_d = 2 with @p L_r = 4 holds L = 4's
 * sensitivity within 0.16 dB, for about half the bank (the design page).
 * @p L_d = @p L_r is the one-stage bank.  Thresholds are powers in the
 * map's own units; the caller sets them for its noise, as for any detector.
 *
 * @param state, x, x_len, g_min, g_max, rates, rates_len
 *                     As dp_corr2d_bank_fold(); @p g_min and @p g_max are on
 *                     the coarse grid.
 * @param L_d          The search grid; at least 1.
 * @param L_r          The resolution grid; a multiple of @p L_d.
 * @param t_candidate  Candidate threshold, on the coarse power.
 * @param t_confirm    Confirm threshold, on the refined power.
 * @param out          Refined powers.
 * @param out1         Refined carriers, on the `1 / (L_r nx)` grid.
 * @param out2         Rates.
 * @param out3         Rows.
 * @param out4         Columns.
 * @param max_out      Capacity of each output; at least `n_out`.
 * @return The record count (0 included), or SIZE_MAX with nothing written
 *         for anything dp_corr2d_bank_fold() refuses, `L_d == 0`, an
 *         @p L_r that @p L_d does not divide, a refined carrier past int32,
 *         or @p max_out below `n_out`.
 * @code
 * >>> import numpy as np
 * >>> from doppler.spectral import DelayDoppler
 * >>> rng = np.random.default_rng(0)
 * >>> ny, nx, d = 8, 32, 5
 * >>> code = rng.choice([-1.0, 1.0], nx).astype(np.complex64)
 * >>> m = np.arange(ny * nx)
 * >>> x = (np.tile(np.roll(code, d), ny)    # 1.25 bins: between L_d = 2
 * ...      * np.exp(2j * np.pi * 1.25 / nx * m)).astype(np.complex64)
 * >>> dd = DelayDoppler(code, ny=ny)
 * >>> no_rates = np.empty(0, np.complex64)
 * >>> coarse, _ = dd.bank_power_max(x, 2, -4, 4, no_rates)
 * >>> t = 0.5 * coarse.max()
 * >>> power, g, rate, row, col = dd.bank_detect(x, 2, 4, -4, 4, t, t,
 * ...                                           no_rates)
 * >>> len(power), int(g[0]), int(col[0])    # g on the 1/(4 nx) grid
 * (1, 5, 5)
 * >>> round(float(power[0] / coarse.max()), 3)  # the straddle recovered
 * 1.233
 * @endcode
 */
size_t dp_corr2d_bank_detect (const dp_corr2d_state_t *state,
                              const float _Complex *x, size_t x_len,
                              size_t L_d, size_t L_r, int g_min, int g_max,
                              float t_candidate, float t_confirm,
                              const float _Complex *rates, size_t rates_len,
                              float *out, int32_t *out1, uint32_t *out2,
                              uint32_t *out3, uint32_t *out4, size_t max_out);

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
