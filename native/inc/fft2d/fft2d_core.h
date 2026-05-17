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
   * @brief Create a 2-D FFT instance.
   *
   * @param ny        Number of rows.
   * @param nx        Number of columns.
   * @param sign      -1 for forward DFT, +1 for inverse.
   * @param nthreads  Ignored (pocketfft is single-threaded).
   * @return Heap-allocated state, or NULL on failure.
   */
  fft2d_state_t *fft2d_create (size_t ny, size_t nx, int sign, int nthreads);

  /** @brief Destroy and free an fft2d instance. @param state May be NULL. */
  void fft2d_destroy (fft2d_state_t *state);

  /** @brief No-op reset (plans are immutable after creation). */
  void fft2d_reset (fft2d_state_t *state);

  /** @brief Maximum output samples per execute call (ny * nx). */
  size_t fft2d_execute_cf64_max_out (fft2d_state_t *state);

  /** @brief Out-of-place 2-D CF64 FFT.  Returns ny*nx. */
  size_t fft2d_execute_cf64 (fft2d_state_t *state, const double complex *in,
                             size_t n_in, double complex *out);

  /** @brief Maximum output samples for CF32 execute (ny * nx). */
  size_t fft2d_execute_cf32_max_out (fft2d_state_t *state);

  /** @brief Out-of-place 2-D CF32 FFT.  Returns ny*nx. */
  size_t fft2d_execute_cf32 (fft2d_state_t *state, const float complex *in,
                             size_t n_in, float complex *out);

  /** @brief Maximum output samples for inplace CF64 execute (ny * nx). */
  size_t fft2d_execute_inplace_cf64_max_out (fft2d_state_t *state);

  /** @brief In-place 2-D CF64 FFT (copies in→out, then transforms). */
  size_t fft2d_execute_inplace_cf64 (fft2d_state_t *state,
                                     const double complex *in, size_t n_in,
                                     double complex *out);

  /** @brief Maximum output samples for inplace CF32 execute (ny * nx). */
  size_t fft2d_execute_inplace_cf32_max_out (fft2d_state_t *state);

  /** @brief In-place 2-D CF32 FFT (copies in→out, then transforms). */
  size_t fft2d_execute_inplace_cf32 (fft2d_state_t *state,
                                     const float complex *in, size_t n_in,
                                     float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT2D_CORE_H */
