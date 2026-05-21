/**
 * @file corr2d_core.h
 * @brief 2-D FFT-based cross-correlator with coherent integrate-and-dump.
 *
 * Two-dimensional extension of corr_core: all buffers are ny×nx row-major
 * flat arrays of length ny*nx.  The correlation theorem extends naturally:
 *
 *   R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)
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
  fft2d_state_t *fwd;       /**< Forward 2-D plan (sign = -1). */
  fft2d_state_t *inv;       /**< Inverse 2-D plan (sign = +1). */
  float complex *ref_spec;  /**< conj(FFT2(ref)), pre-computed.           */
  float complex *work_fft;  /**< Scratch: FFT2(in) output.                */
  float complex *work_ifft; /**< Scratch: IFFT2 output before accumulate. */
  float complex *accum;     /**< Coherent integration accumulator.        */
  size_t ny;                /**< Row count.                               */
  size_t nx;                /**< Column count.                            */
  size_t n;                 /**< ny * nx — total element count.           */
  size_t dwell;             /**< Integration depth.                       */
  size_t count;             /**< Frames accumulated (0 … dwell-1).        */
} corr2d_state_t;

/**
 * @brief Create a 2-D FFT correlator.
 *
 * @param ref       Reference image, flat row-major CF32, length ny*nx.
 * @param ny        Number of rows.
 * @param nx        Number of columns.
 * @param dwell     Integration depth; must be >= 1.
 * @param nthreads  Ignored (pocketfft is single-threaded).
 * @return Heap-allocated state, or NULL on failure.
 */
corr2d_state_t *corr2d_create(const float complex *ref, size_t ny, size_t nx,
                              size_t dwell, int nthreads);

/** @brief Destroy and free a corr2d instance.  @param state May be NULL. */
void corr2d_destroy(corr2d_state_t *state);

/**
 * @brief Zero the accumulator and reset the integration counter to 0.
 * @param state Must be non-NULL.
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
 * @brief Correlate one 2-D frame and optionally dump the accumulator.
 *
 * Steps:
 *   1. FFT2(in) → work_fft
 *   2. work_fft[k] *= ref_spec[k]
 *   3. IFFT2(work_fft) → work_ifft  (divide by ny*nx)
 *   4. accum[k] += work_ifft[k] / (ny*nx)
 *   5. If count == dwell: copy accum → out, zero, reset, return ny*nx.
 *   6. Otherwise: return 0.
 *
 * @param state Must be non-NULL.
 * @param in    Flat row-major CF32 frame, length n_in (must equal ny*nx).
 * @param n_in  Total number of input samples.
 * @param out   Output buffer of length >= ny*nx.  Only written on dump.
 * @return ny*nx on a dump, 0 otherwise.
 */
size_t corr2d_execute(corr2d_state_t *state, const float complex *in,
                      size_t n_in, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* CORR2D_CORE_H */
