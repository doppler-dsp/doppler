/**
 * @file fft_core.h
 * @brief Per-instance 1-D FFT using pocketfft directly.
 *
 * Holds two pocketfft plans — one for CF64, one for CF32 — allocated at
 * create time for the requested transform length and sign.  nthreads is
 * accepted for API compatibility but ignored; pocketfft is single-threaded.
 *
 * Lifecycle:
 * @code
 * fft_state_t *fft = fft_create(1024, -1, 1);
 * double complex out[1024];
 * fft_execute_cf64(fft, in, 1024, out);
 * fft_destroy(fft);
 * @endcode
 */
#ifndef FFT_CORE_H
#define FFT_CORE_H

#include "clib_common.h"
#include "pocketfft/pocketfft.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    pocketfft_plan *plan_f64; /**< CF64 1-D plan. */
    pocketfft_plan *plan_f32; /**< CF32 1-D plan. */
    size_t n;                 /**< Transform length (samples). */
    int sign;                 /**< -1 forward, +1 inverse.    */
  } fft_state_t;

  /**
   * @brief Create a 1-D FFT instance.
   *
   * Allocates one CF64 and one CF32 pocketfft plan for length @p n.
   * nthreads is accepted for API compatibility but ignored.
   *
   * @param n         Transform length in samples.
   * @param sign      -1 for forward DFT, +1 for inverse.
   * @param nthreads  Ignored (pocketfft is single-threaded).
   * @return Heap-allocated state, or NULL on allocation failure.
   */
  fft_state_t *fft_create (size_t n, int sign, int nthreads);

  /** @brief Destroy and free an fft instance. @param state May be NULL. */
  void fft_destroy (fft_state_t *state);

  /** @brief No-op reset (plans are immutable after creation). */
  void fft_reset (fft_state_t *state);

  /** @brief Maximum output samples per execute call (always == n). */
  size_t fft_execute_cf64_max_out (fft_state_t *state);

  /**
   * @brief Out-of-place 1-D CF64 FFT.
   * @param state  Must be non-NULL.
   * @param in     Input buffer of length n_in (must equal state->n).
   * @param n_in   Number of input samples.
   * @param out    Output buffer of length >= n.
   * @return n (samples written).
   */
  size_t fft_execute_cf64 (fft_state_t *state, const double complex *in,
                           size_t n_in, double complex *out);

  /** @brief Maximum output samples for CF32 execute (always == n). */
  size_t fft_execute_cf32_max_out (fft_state_t *state);

  /**
   * @brief Out-of-place 1-D CF32 FFT.
   * @param state  Must be non-NULL.
   * @param in     Input buffer of length n_in.
   * @param n_in   Number of input samples.
   * @param out    Output buffer of length >= n.
   * @return n (samples written).
   */
  size_t fft_execute_cf32 (fft_state_t *state, const float complex *in,
                           size_t n_in, float complex *out);

  /** @brief Maximum output samples for inplace CF64 (always == n). */
  size_t fft_execute_inplace_cf64_max_out (fft_state_t *state);

  /**
   * @brief In-place 1-D CF64 FFT (copies in→out, then transforms in out).
   * @param state  Must be non-NULL.
   * @param in     Source; copied into out before the transform.
   * @param n_in   Number of input samples.
   * @param out    Buffer of length >= n; must not alias in.
   * @return n (samples written).
   */
  size_t fft_execute_inplace_cf64 (fft_state_t *state,
                                   const double complex *in, size_t n_in,
                                   double complex *out);

  /** @brief Maximum output samples for inplace CF32 (always == n). */
  size_t fft_execute_inplace_cf32_max_out (fft_state_t *state);

  /**
   * @brief In-place 1-D CF32 FFT (copies in→out, then transforms in out).
   * @param state  Must be non-NULL.
   * @param in     Source; copied into out before the transform.
   * @param n_in   Number of input samples.
   * @param out    Buffer of length >= n; must not alias in.
   * @return n (samples written).
   */
  size_t fft_execute_inplace_cf32 (fft_state_t *state, const float complex *in,
                                   size_t n_in, float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT_CORE_H */
