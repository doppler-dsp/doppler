/**
 * @file ppe_core.h
 * @brief Feedforward polynomial-phase estimator (frequency + chirp rate).
 *
 * Estimates the normalized frequency @c f (cycles/sample) and chirp rate @c r
 * (cycles/sample^2) of a complex sequence by a **coherent 2-D matched-filter
 * search**. For each chirp-rate hypothesis @c r_i in `[-max_rate, +max_rate]` the
 * sequence is dechirped (multiplied by @c exp(-j*pi*r_i*m^2)) and FFT-ed; the
 * resulting (chirp-rate x frequency) surface peaks at the true (r, f), refined
 * sub-bin in both axes by parabolic interpolation. Being fully coherent it is
 * the matched-filter-optimal estimator (holds at low SNR), and it collapses to
 * a single FFT — pure Doppler — when @c max_rate = 0. One knob therefore spans
 * near-static Doppler through severe LEO chirp.
 *
 * The caller strips modulation first: data-aided (multiply by conj of the known
 * symbols) keeps full SNR; non-data-aided raises an M-PSK stream to the M-th
 * power (BPSK: square) — which doubles @c f and @c r, so the caller halves them.
 *
 * Stateless by-value analyzer (the measure-suite pattern). Composes fft_core +
 * the spectral_core window / find_peaks free functions.
 *
 * @code
 * ppe_state_t *p = ppe_create(4096, 0.0);     // Doppler only (single FFT)
 * ppe_result_t e = ppe_estimate(p, y, n);     // e.freq_norm, e.rate_norm
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
   * @brief Polynomial-phase estimate (one search).
   */
  typedef struct
  {
    double freq_norm; /**< frequency, cycles/sample, in [-0.5, 0.5).        */
    double rate_norm; /**< chirp rate, cycles/sample^2.                     */
    double snr_db;    /**< winning-row peak-to-mean (rough confidence).     */
  } ppe_result_t;

  /**
   * @brief PolynomialPhaseEstimator state (FFT plan + rate grid + scratch).
   *
   * Allocate with ppe_create().
   */
  typedef struct
  {
    size_t max_len;  /**< max input length (sizes the plan/scratch).        */
    size_t nfft;     /**< zero-padded transform length (next pow2 of max_len).*/
    double max_rate; /**< chirp-rate search half-span (cycles/sample^2).    */
    size_t n_rate;   /**< number of chirp-rate hypotheses (1 if max_rate=0).*/
    double drate;    /**< chirp-rate grid step.                             */
    fft_state_t   *fft;    /**< forward plan, size nfft.                    */
    float complex *buf;    /**< windowed, dechirped, zero-padded input, nfft.*/
    float complex *spec;   /**< FFT output, nfft.                           */
    float         *mag;    /**< dB magnitude scratch, nfft.                 */
    float         *win;    /**< window scratch, max_len.                    */
    double        *rowpk;  /**< per-rate winning peak dB, n_rate.           */
    double        *rowfrq; /**< per-rate winning frequency, n_rate.         */
  } ppe_state_t;

  /**
   * @brief Create a polynomial-phase estimator.
   * @param max_len   Maximum input sequence length (>= 4).
   * @param max_rate  Chirp-rate search half-span (cycles/sample^2); 0 searches
   *                  frequency only (a single FFT — near-static Doppler).
   * @return Heap state, or NULL on bad args / allocation failure.
   */
  ppe_state_t *ppe_create (size_t max_len, double max_rate);

  /** @brief Destroy an estimator.  @param state May be NULL. */
  void ppe_destroy (ppe_state_t *state);

  /**
   * @brief Do nothing — the estimator keeps no running state between calls.
   *
   * A feedforward analyzer computes each estimate purely from the segment it is
   * handed, so there is nothing to clear. The method exists only to satisfy the
   * common object protocol; calling it is always safe and has no effect.
   *
   * @param state  Estimator handle (unused).
   * @code
   * >>> from doppler.dsss import PolynomialPhaseEstimator
   * >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=0.0)
   * >>> p.reset()   # no-op: a fresh estimate depends only on the next segment
   *
   * @endcode
   */
  void ppe_reset (ppe_state_t *state);

  /**
   * @brief Estimate the normalized frequency and chirp rate of a complex
   *        segment via the coherent (chirp-rate x frequency) surface.
   *
   * Runs the full 2-D matched-filter search in one shot: for each chirp-rate
   * hypothesis the segment is dechirped and FFT-ed, and the peak of the
   * resulting surface — refined sub-bin by parabolic interpolation on both
   * axes — gives the estimate. With @c max_rate = 0 the rate axis collapses to
   * a single FFT (pure Doppler) and the returned rate is forced to exactly 0.
   *
   * Feed a segment whose modulation has already been stripped (data-aided by
   * the known symbols, or non-data-aided by the M-th-power trick — remembering
   * that raising to the M-th power scales both returned values by M). The
   * result carries @c freq_norm (cycles/sample), @c rate_norm
   * (cycles/sample^2), and @c snr_db (a rough peak-to-mean confidence).
   *
   * @param state  Estimator handle; must be non-NULL.
   * @param x      Complex segment (modulation already stripped by the caller).
   * @param n_in   Segment length, in `[4, max_len]`.
   * @return The estimate; all fields are zeroed if @p n_in is out of range.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import PolynomialPhaseEstimator
   * >>> m = np.arange(512)
   * >>> f, r = 0.05, 1e-5                        # true Doppler + chirp rate
   * >>> x = np.exp(2j*np.pi*(f*m + 0.5*r*m*m)).astype(np.complex64)
   * >>> p = PolynomialPhaseEstimator(max_len=512, max_rate=5e-5)
   * >>> e = p.estimate(x)                        # one-shot coherent search
   * >>> round(e.freq_norm, 4), round(e.rate_norm, 7)
   * (0.0501, 1e-05)
   *
   * @endcode
   */
  ppe_result_t ppe_estimate (ppe_state_t *state, const float complex *x,
                             size_t n_in);

#ifdef __cplusplus
}
#endif

#endif /* PPE_CORE_H */
