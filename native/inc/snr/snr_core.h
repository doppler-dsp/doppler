/**
 * @file snr_core.h
 * @brief Stateless SNR / Es-N0 estimators, data-aided and non-data-aided.
 *
 * Two independent, pure (no persistent state) estimators over a block of
 * complex baseband samples:
 *
 * - snr_data_aided_db(): known-symbol estimator. Strip the known
 *   transmitted sign, then Es/N0 = (mean signal amplitude)^2 / (mean
 *   residual power) -- the classic pilot/known-sequence SNR estimate.
 *   Needs ground truth (or trusted decisions), but is simple and unbiased.
 * - snr_m2m4_db(): moment-based (M2M4) blind estimator (Pauluzzi &
 *   Beaulieu, "A comparison of SNR estimation techniques for the AWGN
 *   channel", IEEE Trans. Commun. 48(10), 2000) for a constant-modulus
 *   signal (BPSK/QPSK/M-PSK) in circular complex AWGN. No known symbols
 *   needed. SNR = sqrt(2*M2^2 - M4) / (M2 - sqrt(2*M2^2 - M4)), where
 *   M2/M4 are the 2nd/4th moments of |x|. Degenerates to 0 dB-equivalent
 *   (linear 0) for pure noise and +inf for a noiseless constant-modulus
 *   signal.
 *
 * Each has a *_db_series() sliding-window sibling, for visualizing SNR
 * drift vs time/index rather than reading one block-average scalar.
 *
 * @code
 * double snr = snr_data_aided_db(soft, n_soft, sign_bits, n_bits);
 * double blind = snr_m2m4_db(x, n);
 * @endcode
 */
#ifndef SNR_CORE_H
#define SNR_CORE_H

#include "clib_common.h"
#include <complex.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Data-aided Es/N0 (dB) over a block of despread symbols.
   *
   * Strips the known transmitted sign (``soft[i] * (sign_bits[i] ? -1 :
   * 1)``), then Es/N0 = (mean signal amplitude)^2 / (mean residual
   * power). Scale-invariant (works regardless of the caller's symbol
   * normalization) and polarity-invariant (a global sign flip in
   * ``soft`` changes nothing, since the amplitude is squared) -- so it
   * needs no resolution of an absolute-phase ambiguity a tracking loop
   * may carry.
   *
   * @param soft         Despread complex symbols.
   * @param soft_len     Length of @p soft.
   * @param sign_bits    Known transmitted bits (0/1; 0 -> +1, 1 -> -1).
   * @param sign_bits_len Length of @p sign_bits.
   * @return Es/N0 in dB over ``min(soft_len, sign_bits_len)`` paired
   *         samples, or NaN if that count is 0 or the residual power is
   *         exactly 0.
   * @code
   * >>> import numpy as np
   * >>> from doppler.snr import snr_data_aided_db
   * >>> rng = np.random.default_rng(0)
   * >>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
   * >>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
   * >>> noise = (0.1 * (rng.standard_normal(2000)
   * ...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
   * >>> soft = (sign + noise).astype(np.complex64)
   * >>> round(float(snr_data_aided_db(soft, bits)), 1)
   * 17.1
   * @endcode
   */
  double snr_data_aided_db (const float complex *soft, size_t soft_len,
                            const uint8_t *sign_bits, size_t sign_bits_len);

  /**
   * @brief Non-data-aided (blind) moment-based Es/N0 (dB) over a block.
   *
   * M2M4 estimator (Pauluzzi & Beaulieu 2000) for a constant-modulus
   * signal (BPSK/QPSK/M-PSK) in circular complex AWGN: no known symbols
   * required.
   *
   * @param x      Complex baseband samples (post-carrier-lock; residual
   *               phase does not bias the moment-based estimate).
   * @param x_len  Length of @p x.
   * @return Es/N0 in dB, 0-linear for pure noise, +inf for a noiseless
   *         constant-modulus signal, or NaN if @p x_len is 0 or the
   *         block has zero power.
   * @code
   * >>> import numpy as np
   * >>> from doppler.snr import snr_m2m4_db
   * >>> rng = np.random.default_rng(0)
   * >>> bits = (rng.random(2000) > 0.5).astype(np.uint8)
   * >>> sign = np.where(bits, -1.0, 1.0).astype(np.complex64)
   * >>> noise = (0.1 * (rng.standard_normal(2000)
   * ...          + 1j * rng.standard_normal(2000))).astype(np.complex64)
   * >>> x = (sign + noise).astype(np.complex64)
   * >>> round(float(snr_m2m4_db(x)), 1)
   * 17.1
   * @endcode
   */
  double snr_m2m4_db (const float complex *x, size_t x_len);

  /**
   * @brief Sliding-window data-aided Es/N0 (dB), one estimate per index.
   *
   * Same estimator as snr_data_aided_db(), applied to a
   * ``[i - window/2, i + window/2]`` window centered (clamped at the
   * edges) on each output index -- for visualizing SNR drift vs
   * time/index rather than reading one block-average scalar.
   *
   * @param soft          Despread complex symbols.
   * @param soft_len      Length of @p soft; also the output length.
   * @param sign_bits     Known transmitted bits (0/1).
   * @param sign_bits_len Length of @p sign_bits; indices at or beyond
   *                      this length have no known sign and are set to
   *                      NaN.
   * @param window        Window width in samples.
   * @param out           Output, length @p soft_len.
   */
  void snr_data_aided_db_series (const float complex *soft, size_t soft_len,
                                 const uint8_t *sign_bits,
                                 size_t sign_bits_len, size_t window,
                                 double *out);

  /**
   * @brief Sliding-window blind (M2M4) Es/N0 (dB), one estimate per index.
   *
   * Same estimator as snr_m2m4_db(), applied to a
   * ``[i - window/2, i + window/2]`` window centered (clamped at the
   * edges) on each output index.
   *
   * @param x       Complex baseband samples.
   * @param x_len   Length of @p x; also the output length.
   * @param window  Window width in samples.
   * @param out     Output, length @p x_len.
   */
  void snr_m2m4_db_series (const float complex *x, size_t x_len,
                           size_t window, double *out);

#ifdef __cplusplus
}
#endif
#endif /* SNR_CORE_H */
