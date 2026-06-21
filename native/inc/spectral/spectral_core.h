/**
 * @file spectral_core.h
 * @brief Spectral module — public C API.
 *
 * Provides windowing (Kaiser, Hann, Blackman-Harris), ENBW computation, magnitude conversion,
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
   * freq_norm is the DC-centred normalised frequency in `[−0.5, +0.5)`.
   * amplitude_db is the parabola-corrected peak value in the same dB units
   * as the input spectrum.
   */
  typedef struct
  {
    float freq_norm;    /**< Normalised frequency −0.5..+0.5 (DC-centred). */
    float amplitude_db; /**< Parabola-corrected peak amplitude in dB. */
  } dp_peak_t;

  /**
   * @brief Compute the equivalent noise bandwidth of a window in bins.
   * ENBW = N * sum(w²) / (sum(w))² quantifies how many noise bins the window
   * smears into the main lobe.  A rectangular window has ENBW = 1.0; tapered
   * windows are > 1.0.  Works with any window type, not just Kaiser.
   *
   * @param w      Float32 window coefficients array; any length >= 1.
   * @param w_len  Number of elements in @p w.
   * @return ENBW in bins (dimensionless).
   * @code
   * >>> from doppler.spectral import kaiser_enbw, hann_window
   * >>> import numpy as np
   * >>> w = np.zeros(8, dtype=np.float32)
   * >>> hann_window(w)
   * >>> round(kaiser_enbw(w), 4)
   * 1.7143
   * @endcode
   */
float kaiser_enbw(const float *w, size_t w_len);

  /**
   * @brief Fill @p w with a Kaiser window of shape parameter @p beta.
   * I0 is computed via the converging power-series expansion.  Increasing
   * @p beta raises sidelobe attenuation at the cost of a wider main lobe
   * (beta=0 → rectangular, beta≈6 → ~60 dB sidelobe rejection).  The
   * output is normalised so that `w[0]` = `w[N-1]` = I0(0)/I0(beta).
   *
   * @param w      Output buffer modified in-place; must be length >= 1.
   * @param w_len  Number of elements in @p w.
   * @param beta   Window shape parameter (float, >= 0).
   * @code
   * >>> from doppler.spectral import kaiser_window
   * >>> import numpy as np
   * >>> w = np.zeros(8, dtype=np.float32)
   * >>> kaiser_window(w, 6.0)
   * >>> [round(v, 4) for v in w.tolist()]
   * [0.0149, 0.1998, 0.5913, 0.9454, 0.9454, 0.5913, 0.1998, 0.0149]
   * @endcode
   */
void kaiser_window(float *w, size_t w_len, float beta);

  /**
   * @brief Fill @p w with a Hann (raised-cosine) window.
   * Computes w(k) = 0.5*(1 - cos(2π k/(N-1))) for k = 0..N-1.  The window
   * tapers smoothly to zero at both endpoints, providing ~31 dB first-sidelobe
   * rejection.  Takes no shape parameter; use Kaiser for adjustable roll-off.
   *
   * @param w      Output buffer modified in-place; must be length >= 1.
   * @param w_len  Number of elements in @p w.
   * @code
   * >>> from doppler.spectral import hann_window
   * >>> import numpy as np
   * >>> w = np.zeros(8, dtype=np.float32)
   * >>> hann_window(w)
   * >>> [round(v, 4) for v in w.tolist()]
   * [0.0, 0.1883, 0.6113, 0.9505, 0.9505, 0.6113, 0.1883, 0.0]
   * @endcode
   */
void hann_window(float *w, size_t w_len);

  /**
   * @brief Fill @p w with a 4-term Blackman-Harris window.
   * Computes the minimum 4-term Blackman-Harris window:
   * w(k) = 0.35875 - 0.48829*cos(2πk/(N-1))
   *               + 0.14128*cos(4πk/(N-1))
   *               - 0.01168*cos(6πk/(N-1))
   * for k = 0..N-1.  Provides approximately 92 dB first-sidelobe rejection,
   * far deeper than Hann (~31 dB) or Kaiser at β=8 (~80 dB).  Use for
   * quantization and decimation spectra where you need to see low-level
   * artefacts below the noise floor.
   *
   * @param w      Output buffer modified in-place; must be length >= 1.
   * @param w_len  Number of elements in @p w.
   * @code
   * >>> from doppler.spectral import blackman_harris_window
   * >>> import numpy as np
   * >>> w = np.zeros(8, dtype=np.float32)
   * >>> blackman_harris_window(w)
   * >>> [round(v, 4) for v in w.tolist()]
   * [0.0001, 0.0334, 0.3328, 0.8894, 0.8894, 0.3328, 0.0334, 0.0001]
   * @endcode
   */
void blackman_harris_window(float *w, size_t w_len);

  /**
   * @brief Convert a CF32 complex spectrum to F32 dB magnitudes.
   * Computes out(k) = 20*log10(max(|x(k)|, lin_floor)) + offset_db for
   * each bin.  The @p lin_floor guard prevents log10(0); a value of 1e-12
   * corresponds to a -240 dB noise floor.  @p offset_db shifts the entire
   * output for calibration (e.g., normalise to 0 dBFS).
   *
   * @param x          CF32 complex spectrum array, length @p x_len.
   * @param x_len      Number of elements in @p x.
   * @param out        Output F32 buffer, length >= @p x_len; caller-allocated.
   * @param lin_floor  Linear amplitude floor (must be > 0, e.g. 1e-12).
   * @param offset_db  Calibration offset added to every output bin.
   * @code
   * >>> from doppler.spectral import magnitude_db_cf32
   * >>> import numpy as np
   * >>> x = np.array([1+0j, 0.1+0j, 0+0j], dtype=np.complex64)
   * >>> magnitude_db_cf32(x, 1e-12, 0.0).tolist()
   * [0.0, -20.0, -240.0]
   * @endcode
   */
void magnitude_db_cf32(const float complex *x, size_t x_len, float *out, float lin_floor, float offset_db);

  /**
   * @brief Convert a CF64 complex spectrum to F32 dB magnitudes.
   * Double-precision variant of magnitude_db_cf32().  Accepts a CF64 input
   * array and a double @p lin_floor; output is still F32 because downstream
   * display code typically works in single precision.  The formula and
   * @p offset_db semantics are identical.
   *
   * @param x          CF64 complex spectrum array, length @p x_len.
   * @param x_len      Number of elements in @p x.
   * @param out        Output F32 buffer, length >= @p x_len; caller-allocated.
   * @param lin_floor  Linear amplitude floor (double, must be > 0).
   * @param offset_db  Calibration offset added to every output bin.
   * @code
   * >>> from doppler.spectral import magnitude_db_cf64
   * >>> import numpy as np
   * >>> x = np.array([1+0j, 10+0j], dtype=np.complex128)
   * >>> magnitude_db_cf64(x, 1e-12, 0.0).tolist()
   * [0.0, 20.0]
   * @endcode
   */
void magnitude_db_cf64(const double complex *x, size_t x_len, float *out, double lin_floor, float offset_db);

  /**
   * @brief Find up to @p n_peaks local maxima in a DC-centred F32 dB spectrum.
   * Three-step algorithm: (1) local-max scan — `db[k]` > `db[k-1]` && `db[k]` >=
   * `db[k+1]` with `db[k]` > min_db; (2) parabolic interpolation on each local
   * maximum to produce sub-bin freq_norm accuracy; (3) sort descending and
   * return the top @p n_peaks.  freq_norm is DC-centred: bin i maps to
   * freq_norm = (i - N/2) / N so DC (bin N/2) → 0.0 and the first negative
   * frequency bin → −0.5.  The spectrum must have at least 3 bins.
   *
   * @param db      F32 dB spectrum, DC-centred, length >= 3.
   * @param db_len  Number of elements in @p db.
   * @param n_peaks Maximum number of peaks to return.
   * @param min_db  Amplitude gate; local maxima below this are discarded.
   * @param result  Caller-supplied dp_peak_t array of length >= @p n_peaks;
   *                filled with up to @p n_peaks results sorted descending.
   * @return Number of dp_peak_t entries written to @p result.
   * @code
   * >>> from doppler.spectral import find_peaks_f32
   * >>> import numpy as np
   * >>> db = np.full(32, -60.0, dtype=np.float32)
   * >>> db[7] = -15.0; db[8] = -10.0; db[9] = -15.0
   * >>> peaks = find_peaks_f32(db, 2, -30.0)
   * >>> peaks
   * [(-0.25, -10.0)]
   * @endcode
   */
size_t find_peaks_f32(const float *db, size_t db_len, size_t n_peaks, float min_db, dp_peak_t *result);

double obw_from_power(const double *pwr, size_t pwr_len, double fs, double frac);
double noise_floor_db(const float *db, size_t db_len);
#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_CORE_H */
