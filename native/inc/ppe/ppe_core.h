/**
 * @file ppe_core.h
 * @brief Feedforward polynomial-phase estimator (frequency + chirp rate).
 *
 * Estimates the linear and quadratic phase terms of a complex sequence — its
 * normalized frequency @c f (cycles/sample) and chirp rate @c r
 * (cycles/sample^2) — via the 2-lag Higher-order Ambiguity Function (HAF, a.k.a.
 * Discrete Polynomial Transform). For a 2nd-order polynomial-phase signal
 * @c y[m] = A·exp(j2π(f·m + ½·r·m²)), the instantaneous autocorrelation
 * @c c[m] = y[m+k]·conj(y[m]) collapses to a single tone at @c ν = r·k, so one
 * FFT locates the chirp rate; dechirping @c y by @c r̂ and a second FFT locates
 * the frequency. One-shot, feedforward — no tracking loop.
 *
 * The caller strips modulation first: data-aided (multiply by conj of the known
 * symbols) keeps full SNR; non-data-aided raises an M-PSK stream to the M-th
 * power (BPSK: square) — which doubles @c f and @c r, so the caller halves the
 * result.
 *
 * Stateless by-value analyzer (the measure-suite pattern). Composes fft_core +
 * the spectral_core window / find_peaks free functions.
 *
 * Lifecycle: ppe_create -> (estimate)* -> ppe_destroy.
 *
 * @code
 * ppe_state_t *p = ppe_create(4096, 0);
 * ppe_result_t e = ppe_estimate(p, y, n);   // e.freq_norm, e.rate_norm
 * ppe_destroy(p);
 * @endcode
 */
#ifndef PPE_CORE_H
#define PPE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Polynomial-phase estimate (one HAF analysis).
   */
  typedef struct
  {
    double freq_norm; /**< frequency, cycles/sample, in [-0.5, 0.5).        */
    double rate_norm; /**< chirp rate, cycles/sample^2.                     */
    double snr_db;    /**< final-spectrum peak-to-mean (rough confidence).  */
  } ppe_result_t;

  /**
   * @brief PolyPhaseEstimator state (FFT plan + scratch).
   *
   * Allocate with ppe_create().
   */
  typedef struct
  {
    size_t max_len; /**< max input length (sizes the plan/scratch).         */
    size_t nfft;    /**< zero-padded transform length (next pow2 of max_len).*/
    size_t lag;     /**< HAF lag k; 0 selects L/2 per call.                 */
    fft_state_t   *fft;  /**< forward plan, size nfft.                      */
    float complex *ac;   /**< autocorrelation / dechirp scratch, max_len.   */
    float complex *buf;  /**< windowed, zero-padded FFT input, nfft.        */
    float complex *spec; /**< FFT output, nfft.                             */
    float         *mag;  /**< dB magnitude scratch, nfft.                   */
    float         *win;  /**< window scratch, max_len.                      */
  } ppe_state_t;

  /**
   * @brief Create a polynomial-phase estimator.
   * @param max_len  Maximum input sequence length (>= 4).
   * @param lag      HAF lag k; 0 selects k = L/2 per call.
   * @return Heap state, or NULL on bad args / allocation failure.
   */
  ppe_state_t *ppe_create (size_t max_len, size_t lag);

  /** @brief Destroy an estimator.  @param state May be NULL. */
  void ppe_destroy (ppe_state_t *state);

  /** @brief No-op (the estimator carries no running state). */
  void ppe_reset (ppe_state_t *state);

  /**
   * @brief Estimate (frequency, chirp rate) of @p in via the 2-lag HAF.
   * @param state  Must be non-NULL.
   * @param in     Complex sequence (modulation already stripped by the caller).
   * @param n_in   Length, in [4, max_len].
   * @return The estimate; zeroed if @p n_in is out of range.
   */
  ppe_result_t ppe_estimate (ppe_state_t *state, const float complex *in,
                             size_t n_in);

#ifdef __cplusplus
}
#endif

#endif /* PPE_CORE_H */
