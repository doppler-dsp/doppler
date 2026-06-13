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
   * @brief Allocate a reusable 1-D FFT engine for a fixed length and sign.
   * Two pocketfft plans are created at construction time — one for CF64 and
   * one for CF32 — so execute calls carry no plan-setup overhead.  The same
   * instance may be called repeatedly for independent input vectors of the
   * same length.  @p nthreads is accepted for API parity but is ignored;
   * pocketfft plans are single-threaded.
   *
   * @param n         Transform length in samples (power of two recommended).
   * @param sign      -1 for the forward DFT, +1 for the inverse DFT.
   * @param nthreads  Accepted for API compatibility; ignored.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.spectral import FFT
   * >>> import numpy as np
   * >>> fft = FFT(n=4, sign=-1, nthreads=1)
   * >>> fft.n, fft.sign
   * (4, -1)
   * >>> x = np.array([1, 0, 0, 0], dtype=np.complex64)
   * >>> fft.execute_cf32(x).tolist()
   * [(1+0j), (1+0j), (1+0j), (1+0j)]
   * @endcode
   */
  fft_state_t *fft_create (size_t n, int sign, int nthreads);

  /** @brief Destroy and free an fft instance. @param state May be NULL. */
  void fft_destroy (fft_state_t *state);

  /** @brief No-op reset (plans are immutable after creation). */
  void fft_reset (fft_state_t *state);

  /** @brief Maximum output samples per execute call (always == n). */
  size_t fft_execute_cf64_max_out (fft_state_t *state);

  /**
   * @brief Compute an out-of-place 1-D DFT on a double-precision complex input.
   * The output is written to a fresh caller-supplied buffer; @p in and @p out
   * must not alias.  The transform is unnormalised: the inverse DFT (sign=+1)
   * does NOT divide by n.  Both buffers must be exactly state->n elements long.
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Input buffer of length state->n (CF64, row-major).
   * @param n_in   Number of input samples; must equal state->n.
   * @param out    Output buffer of length >= state->n (CF64, caller-allocated).
   * @return n (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT
   * >>> import numpy as np
   * >>> fft = FFT(n=4, sign=-1)
   * >>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
   * >>> fft.execute_cf64(x).tolist()
   * [(1+0j), (1+0j), (1+0j), (1+0j)]
   * @endcode
   */
  size_t fft_execute_cf64 (fft_state_t *state, const double complex *in,
                           size_t n_in, double complex *out);

  /** @brief Maximum output samples for CF32 execute (always == n). */
  size_t fft_execute_cf32_max_out (fft_state_t *state);

  /**
   * @brief Compute an out-of-place 1-D DFT on a single-precision complex input.
   * Identical to fft_execute_cf64() but operates on float complex (CF32)
   * buffers, halving memory bandwidth relative to the double-precision variant.
   * Output is unnormalised; @p in and @p out must not alias.
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Input buffer of length state->n (CF32, row-major).
   * @param n_in   Number of input samples; must equal state->n.
   * @param out    Output buffer of length >= state->n (CF32, caller-allocated).
   * @return n (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT
   * >>> import numpy as np
   * >>> fft = FFT(n=4, sign=-1)
   * >>> x = np.ones(4, dtype=np.complex64)
   * >>> fft.execute_cf32(x).tolist()
   * [(4+0j), 0j, 0j, 0j]
   * @endcode
   */
  size_t fft_execute_cf32 (fft_state_t *state, const float complex *in,
                           size_t n_in, float complex *out);

  /** @brief Maximum output samples for inplace CF64 (always == n). */
  size_t fft_execute_inplace_cf64_max_out (fft_state_t *state);

  /**
   * @brief Copy @p in into @p out, then transform @p out in-place (CF64).
   * The copy step lets callers preserve their input while keeping the output
   * buffer hot in cache.  Semantically identical to fft_execute_cf64() for
   * separate @p in / @p out pointers; use this variant when the caller already
   * owns @p out and wants the result there without a second allocation.
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Source buffer, state->n CF64 samples; not modified.
   * @param n_in   Number of input samples; must equal state->n.
   * @param out    Destination buffer, length >= state->n; must not alias in.
   * @return n (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT
   * >>> import numpy as np
   * >>> fft = FFT(n=4, sign=-1)
   * >>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
   * >>> fft.execute_inplace_cf64(x).tolist()
   * [(1+0j), (1+0j), (1+0j), (1+0j)]
   * @endcode
   */
  size_t fft_execute_inplace_cf64 (fft_state_t *state,
                                   const double complex *in, size_t n_in,
                                   double complex *out);

  /** @brief Maximum output samples for inplace CF32 (always == n). */
  size_t fft_execute_inplace_cf32_max_out (fft_state_t *state);

  /**
   * @brief Copy @p in into @p out, then transform @p out in-place (CF32).
   * Single-precision variant of fft_execute_inplace_cf64().  Copies
   * state->n CF32 samples from @p in to @p out, then transforms @p out
   * with the CF32 pocketfft plan.  @p in is left unmodified.
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Source buffer, state->n CF32 samples; not modified.
   * @param n_in   Number of input samples; must equal state->n.
   * @param out    Destination buffer, length >= state->n; must not alias in.
   * @return n (number of samples written).
   * @code
   * >>> from doppler.spectral import FFT
   * >>> import numpy as np
   * >>> fft = FFT(n=4, sign=-1)
   * >>> x = np.array([1, 0, 0, 0], dtype=np.complex64)
   * >>> fft.execute_inplace_cf32(x).tolist()
   * [(1+0j), (1+0j), (1+0j), (1+0j)]
   * @endcode
   */
  size_t fft_execute_inplace_cf32 (fft_state_t *state, const float complex *in,
                                   size_t n_in, float complex *out);

  /** @brief Maximum output samples for the ci16 execute (always == n). */
  size_t fft_execute_ci16_max_out (fft_state_t *state);

  /**
   * @brief Compute an out-of-place 1-D DFT directly on integer IQ (ci16).
   * @p in is interleaved int16 I/Q (2 ints per complex sample, length 2*n);
   * the result is float complex (CF32).  The int->float scale (v/32768,
   * full-scale ±1.0, matching the cvt module) is folded into the transform's
   * input read, so this is a single fused pass — faster than a separate
   * i16_to_f32 conversion followed by fft_execute_cf32().  Output is
   * unnormalised.
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Interleaved int16 I/Q, 2*state->n samples.
   * @param n_in   Number of complex samples; must equal state->n.
   * @param out    Output buffer of length >= state->n (CF32, caller-allocated).
   * @return n (number of complex samples written).
   * @code
   * >>> import numpy as np
   * >>> from doppler.spectral import FFT
   * >>> fft = FFT(n=4, sign=-1)
   * >>> iq = np.full(8, 32768 // 4, dtype=np.int16)   # ~0.25 + 0.25j, full-scale
   * >>> np.round(fft.execute_ci16(iq).real, 3).tolist()
   * [1.0, 0.0, 0.0, 0.0]
   * @endcode
   */
  size_t fft_execute_ci16 (fft_state_t *state, const int16_t *in, size_t n_in,
                           float complex *out);

  /** @brief Maximum output samples for the ci8 execute (always == n). */
  size_t fft_execute_ci8_max_out (fft_state_t *state);

  /**
   * @brief Compute an out-of-place 1-D DFT directly on integer IQ (ci8).
   * As fft_execute_ci16() but @p in is interleaved int8 I/Q (scale v/128).
   *
   * @param state  Allocated FFT engine (non-NULL).
   * @param in     Interleaved int8 I/Q, 2*state->n samples.
   * @param n_in   Number of complex samples; must equal state->n.
   * @param out    Output buffer of length >= state->n (CF32, caller-allocated).
   * @return n (number of complex samples written).
   * @code
   * >>> import numpy as np
   * >>> from doppler.spectral import FFT
   * >>> fft = FFT(n=4, sign=-1)
   * >>> iq = np.full(8, 32, dtype=np.int8)            # 0.25 + 0.25j, full-scale
   * >>> np.round(fft.execute_ci8(iq).real, 3).tolist()
   * [1.0, 0.0, 0.0, 0.0]
   * @endcode
   */
  size_t fft_execute_ci8 (fft_state_t *state, const int8_t *in, size_t n_in,
                          float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FFT_CORE_H */
