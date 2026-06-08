/**
 * @file fft2d_core.h
 * @brief Per-instance 2-D FFT using pocketfft directly.
 *
 * Holds two pocketfft plans — one CF64, one CF32 — for an ny × nx row-major
 * transform.  Input and output arrays are flat buffers of length ny*nx;
 * the Python wrapper class reshapes them.  nthreads is accepted for API
 * compatibility but ignored.
 *
 * Lifecycle:
 * @code
 * fft2d_state_t *fft = fft2d_create(64, 64, -1, 1);
 * double complex out[64 * 64];
 * fft2d_execute_cf64(fft, in, 64 * 64, out);
 * fft2d_destroy(fft);
 * @endcode
 */
#ifndef FFT2D_CORE_H
#define FFT2D_CORE_H

#include "clib_common.h"
#include "pocketfft/pocketfft.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    pocketfft_plan *plan_f64; /**< CF64 2-D plan. */
    pocketfft_plan *plan_f32; /**< CF32 2-D plan. */
    size_t ny;                /**< Row count.     */
    size_t nx;                /**< Column count.  */
    int sign;                 /**< -1 forward, +1 inverse. */
  } fft2d_state_t;

  /**
   * @brief Allocate a reusable 2-D FFT engine for a fixed ny×nx grid.
   * Two pocketfft 2-D plans are built at construction time — one CF64, one
   * CF32.  All execute calls accept and return flat row-major arrays of
   * length ny*nx; the Python layer may reshape them with .reshape(ny, nx).
   * @p nthreads is accepted for API parity but ignored.
   *
   * @param ny        Number of rows (outer dimension).
   * @param nx        Number of columns (inner dimension).
   * @param sign      -1 for the forward DFT, +1 for the inverse DFT.
   * @param nthreads  Accepted for API compatibility; ignored.
   * @return Heap-allocated state, or NULL on failure.
   * @code
   * >>> from doppler.spectral import FFT2D
   * >>> import numpy as np
   * >>> fft2d = FFT2D(ny=4, nx=4, sign=-1, nthreads=1)
   * >>> fft2d.ny, fft2d.nx, fft2d.sign
   * (4, 4, -1)
   * >>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
   * >>> out = fft2d.execute_cf32(x)
   * >>> out.shape, out.dtype
   * ((16,), dtype('complex64'))
   * >>> bool(np.allclose(out, 1.0))
   * True
   * @endcode
   */
  fft2d_state_t *fft2d_create (size_t ny, size_t nx, int sign, int nthreads);

  /** @brief Destroy and free an fft2d instance. @param state May be NULL. */
  void fft2d_destroy (fft2d_state_t *state);

  /** @brief No-op reset (plans are immutable after creation). */
  void fft2d_reset (fft2d_state_t *state);

  /** @brief Maximum output samples per execute call (ny * nx). */
  size_t fft2d_execute_cf64_max_out (fft2d_state_t *state);

  /**
   * @brief Compute an out-of-place 2-D DFT on a double-precision complex grid.
   * @p in is a flat row-major CF64 array of length ny*nx.  The output is
   * written to the caller-supplied @p out buffer (also ny*nx); the two must
   * not alias.  The transform is unnormalised.
   *
   * @param state  Allocated FFT2D engine (non-NULL).
   * @param in     Flat row-major CF64 input, length ny*nx.
   * @param n_in   Number of input samples; must equal ny*nx.
   * @param out    Flat row-major CF64 output, length >= ny*nx (caller-allocated).
   * @return ny*nx (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT2D
   * >>> import numpy as np
   * >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
   * >>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
   * >>> out = fft2d.execute_cf64(x)
   * >>> out.shape, out.dtype
   * ((16,), dtype('complex128'))
   * >>> bool(np.allclose(out, 1.0))
   * True
   * @endcode
   */
  size_t fft2d_execute_cf64 (fft2d_state_t *state, const double complex *in,
                             size_t n_in, double complex *out);

  /** @brief Maximum output samples for CF32 execute (ny * nx). */
  size_t fft2d_execute_cf32_max_out (fft2d_state_t *state);

  /**
   * @brief Compute an out-of-place 2-D DFT on a single-precision complex grid.
   * Single-precision variant of fft2d_execute_cf64().  Accepts and returns
   * flat row-major CF32 arrays of length ny*nx.  Output is unnormalised;
   * @p in and @p out must not alias.
   *
   * @param state  Allocated FFT2D engine (non-NULL).
   * @param in     Flat row-major CF32 input, length ny*nx.
   * @param n_in   Number of input samples; must equal ny*nx.
   * @param out    Flat row-major CF32 output, length >= ny*nx (caller-allocated).
   * @return ny*nx (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT2D
   * >>> import numpy as np
   * >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
   * >>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
   * >>> out = fft2d.execute_cf32(x)
   * >>> out.shape, out.dtype
   * ((16,), dtype('complex64'))
   * >>> bool(np.allclose(out, 1.0))
   * True
   * @endcode
   */
  size_t fft2d_execute_cf32 (fft2d_state_t *state, const float complex *in,
                             size_t n_in, float complex *out);

  /** @brief Maximum output samples for inplace CF64 execute (ny * nx). */
  size_t fft2d_execute_inplace_cf64_max_out (fft2d_state_t *state);

  /**
   * @brief Copy @p in into @p out, then transform @p out in-place (CF64 2-D).
   * The ny*nx CF64 samples from @p in are first memcpy'd to @p out; the 2-D
   * DFT is then applied to @p out in-place.  @p in is left unmodified.
   * Useful when the caller owns @p out and wants to preserve @p in.
   *
   * @param state  Allocated FFT2D engine (non-NULL).
   * @param in     Source, ny*nx CF64 flat row-major; not modified.
   * @param n_in   Number of input samples; must equal ny*nx.
   * @param out    Destination, length >= ny*nx; must not alias in.
   * @return ny*nx (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT2D
   * >>> import numpy as np
   * >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
   * >>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
   * >>> out = fft2d.execute_inplace_cf64(x)
   * >>> bool(np.allclose(out, 1.0))
   * True
   * @endcode
   */
  size_t fft2d_execute_inplace_cf64 (fft2d_state_t *state,
                                     const double complex *in, size_t n_in,
                                     double complex *out);

  /** @brief Maximum output samples for inplace CF32 execute (ny * nx). */
  size_t fft2d_execute_inplace_cf32_max_out (fft2d_state_t *state);

  /**
   * @brief Copy @p in into @p out, then transform @p out in-place (CF32 2-D).
   * Single-precision variant of fft2d_execute_inplace_cf64().  Copies
   * ny*nx CF32 samples then applies the CF32 2-D pocketfft plan to @p out.
   *
   * @param state  Allocated FFT2D engine (non-NULL).
   * @param in     Source, ny*nx CF32 flat row-major; not modified.
   * @param n_in   Number of input samples; must equal ny*nx.
   * @param out    Destination, length >= ny*nx; must not alias in.
   * @return ny*nx (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT2D
   * >>> import numpy as np
   * >>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
   * >>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
   * >>> out = fft2d.execute_inplace_cf32(x)
   * >>> bool(np.allclose(out, 1.0))
   * True
   * @endcode
   */
  size_t fft2d_execute_inplace_cf32 (fft2d_state_t *state,
                                     const float complex *in, size_t n_in,
                                     float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT2D_CORE_H */
