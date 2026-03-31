/**
 * @file window.h
 * @brief Window functions for spectral analysis and filter design.
 *
 * Provides the Kaiser window, which is parameterised by a shape
 * factor @f$\beta@f$ that trades main-lobe width (resolution) for
 * side-lobe rejection.
 *
 * ### Kaiser window
 *
 * @f[
 *   w[n] = \frac{I_0\!\left(\beta\sqrt{1-(2n/(N-1)-1)^2}\right)}{I_0(\beta)},
 *   \quad n = 0, \ldots, N-1
 * @f]
 *
 * where @f$I_0@f$ is the zeroth-order modified Bessel function of the
 * first kind.  @f$\beta = 0@f$ gives a rectangular window; larger
 * values increase side-lobe suppression at the cost of wider main
 * lobe.
 *
 * ### Equivalent noise bandwidth (ENBW)
 *
 * The ENBW quantifies how much noise power a window passes relative to
 * a rectangular window of the same length.  For a spectrum analyser,
 * it is the displayed resolution bandwidth (RBW) per bin:
 *
 * @f[
 *   \mathrm{ENBW}_\mathrm{bins} = N \cdot
 *   \frac{\sum_n w[n]^2}{\left(\sum_n w[n]\right)^2}
 * @f]
 *
 * To convert to Hz: @f$\mathrm{RBW} = \mathrm{ENBW}_\mathrm{bins}
 * \times F_s / N@f$.
 *
 * ### Typical β values
 *
 * β follows the NumPy / SciPy convention: it is the direct argument
 * to I₀, **not** scaled by π.  The Harris (1978) table uses α = β/π.
 *
 * | β    | Side-lobe (dB) | ENBW (bins) | Use case               |
 * |------|----------------|-------------|------------------------|
 * | 0    | −13            | 1.00        | Rectangular (no window) |
 * | 5    | −57            | 1.36        | General-purpose        |
 * | 6    | −69            | 1.47        | Spectrum analyser      |
 * | 8.6  | −90            | 1.72        | High dynamic range     |
 * | 13.3 | −120           | 2.11        | Very high DR           |
 *
 * ### Usage
 *
 * ```c
 * #include <dp/window.h>
 *
 * float w[1024];
 * dp_kaiser_window(w, 1024, 6.0f);
 * float enbw = dp_kaiser_enbw(w, 1024);
 * // enbw ≈ 1.79 bins → RBW = enbw * fs / N  Hz
 * ```
 */

#ifndef DP_WINDOW_H
#define DP_WINDOW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Fill @p w with a Kaiser window of length @p n and shape @p beta.
   *
   * @param w     Output buffer, length @p n.  Must be non-NULL.
   * @param n     Window length (number of samples).  Must be ≥ 1.
   * @param beta  Shape factor @f$\beta \geq 0@f$.  0 → rectangular.
   */
  void dp_kaiser_window (float *w, size_t n, float beta);

  /**
   * @brief Compute the equivalent noise bandwidth (ENBW) of a window.
   *
   * Returns the ENBW in units of FFT bins.  Multiply by @f$F_s / N@f$
   * to convert to Hz.
   *
   * @param w  Window coefficients, length @p n.  Must be non-NULL.
   * @param n  Window length.  Must be ≥ 1.
   * @return   ENBW in bins.
   */
  float dp_kaiser_enbw (const float *w, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_WINDOW_H */
