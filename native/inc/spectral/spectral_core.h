/**
 * @file spectral_core.h
 * @brief Spectral module — public C API.
 *
 * Provides windowing (Kaiser, Hann), ENBW computation, magnitude conversion,
 * and peak finding.  These are pure functions with no persistent state.
 */
#ifndef SPECTRAL_CORE_H
#define SPECTRAL_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief One spectral peak returned by find_peaks_f32().
   *
   * freq_norm is the DC-centred normalised frequency in [−0.5, +0.5).
   * amplitude_db is the parabola-corrected peak value in the same dB units
   * as the input spectrum.
   */
  typedef struct
  {
    float freq_norm;    /**< Normalised frequency −0.5..+0.5 (DC-centred). */
    float amplitude_db; /**< Parabola-corrected peak amplitude in dB. */
  } dp_peak_t;

  /**
   * @brief Equivalent noise bandwidth of window @p w.
   *
   * ENBW = N * sum(w^2) / sum(w)^2
   *
   * @param w      Float32 window coefficients, length @p w_len.
   * @param w_len  Number of window samples.
   * @return ENBW in bins.
   */
  float kaiser_enbw (const float *w, size_t w_len);

  /**
   * @brief Fill @p w with a Kaiser window of shape parameter @p beta.
   *
   * Uses the Bessel function I0 via a converging power series.
   *
   * @param w      Output buffer, length @p w_len (modified in-place).
   * @param w_len  Window length >= 1.
   * @param beta   Shape parameter (higher = more attenuation, wider main
   * lobe).
   */
  void kaiser_window (float *w, size_t w_len, float beta);

  /**
   * @brief Fill @p w with a Hann window.
   *
   * w(k) = 0.5 * (1 - cos(2π k / (N-1))), k = 0..N-1.
   *
   * @param w      Output buffer, length @p w_len (modified in-place).
   * @param w_len  Window length >= 1.
   */
  void hann_window (float *w, size_t w_len);

  /**
   * @brief Convert CF32 spectrum to F32 dB.
   *
   * out(k) = 20*log10(max(|in(k)|, lin_floor)) + offset_db
   *
   * @param in         CF32 spectrum, length @p n.
   * @param n          Number of bins.
   * @param out        F32 output, length @p n (caller-allocated).
   * @param lin_floor  Amplitude floor before log10 (e.g. 1e-12f).
   * @param offset_db  Calibration offset added to every bin.
   */
  void magnitude_db_cf32 (const float complex *in, size_t n, float *out,
                          float lin_floor, float offset_db);

  /**
   * @brief Convert CF64 spectrum to F32 dB.
   *
   * Same as magnitude_db_cf32() but accepts double-precision input.
   *
   * @param in         CF64 spectrum, length @p n.
   * @param n          Number of bins.
   * @param out        F32 output, length @p n (caller-allocated).
   * @param lin_floor  Amplitude floor (double precision).
   * @param offset_db  Calibration offset added to every bin.
   */
  void magnitude_db_cf64 (const double complex *in, size_t n, float *out,
                          double lin_floor, float offset_db);

  /**
   * @brief Find up to @p n_peaks local maxima in a DC-centred F32 dB spectrum.
   *
   * Algorithm:
   *   1. Local-max scan: db(k) > db(k-1) && db(k) >= db(k+1), above min_db.
   *   2. Parabolic interpolation for sub-bin frequency accuracy.
   *   3. Sort descending by amplitude; return top n_peaks.
   *
   * @param db      F32 dB spectrum, DC-centred, length @p n.  Must be >= 3.
   * @param n       Number of bins.
   * @param n_peaks Maximum number of peaks to return.
   * @param min_db  Amplitude threshold; bins below this are ignored.
   * @param out     Caller-allocated output array, length >= @p n_peaks.
   * @return        Number of peaks written (<= n_peaks).
   */
  size_t find_peaks_f32 (const float *db, size_t n, size_t n_peaks,
                         float min_db, dp_peak_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_CORE_H */
