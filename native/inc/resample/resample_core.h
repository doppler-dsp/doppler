/**
 * @file resample_core.h
 * @brief Resample module — public C API.
 */
#ifndef RESAMPLE_CORE_H
#define RESAMPLE_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

/**
 * @brief Compute the Kaiser window beta parameter from stopband attenuation.
 * Uses the standard Kaiser-Hamming formulae:
 *   atten > 50  dB: beta = 0.1102 * (atten - 8.7)
 *   21 <= atten <= 50 dB: beta = 0.5842*(atten-21)^0.4 + 0.07886*(atten-21)
 *   atten < 21  dB: beta = 0.0 (rectangular window)
 *
 * @param atten  Desired stopband attenuation in dB (positive value).
 * @return       Kaiser beta parameter (>= 0.0).
 *
 * @code
 * >>> from doppler.resample import kaiser_beta
 * >>> round(kaiser_beta(60.0), 4)
 * 5.6533
 * >>> kaiser_beta(20.0)
 * 0.0
 * @endcode
 */
double kaiser_beta(double atten);

/**
 * @brief Estimate the taps-per-phase count for a polyphase Kaiser FIR bank.
 * Applies the Kaiser length formula to the per-phase normalised prototype
 * (pb/num_phases, sb/num_phases), rounds up to the next odd symmetrical
 * length, then divides by num_phases to give taps per branch. The result
 * is the minimum num_taps argument to pass to Resampler_create_custom().
 *
 * @param num_phases  Number of polyphase branches (power of two).
 * @param atten       Desired stopband attenuation in dB.
 * @param pb          Normalised passband edge (0 < pb < sb < 1).
 * @param sb          Normalised stopband edge.
 * @return            Taps per polyphase branch (>= 1).
 *
 * @code
 * >>> from doppler.resample import kaiser_num_taps
 * >>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)
 * 19
 * @endcode
 */
int kaiser_num_taps(int num_phases, double atten, double pb, double sb);

  /**
   * @brief Design a CIC passband-droop compensator FIR filter.
   * Implements the closed-form Bernoulli-series maximally-flat-error
   * method from Molnar & Vucic (IEEE TCAS-II 58(12):926-930, 2011,
   * DOI 10.1109/TCSII.2011.2172522). The compensator runs at the
   * *decimated* (output) rate and should be applied after the CIC stage.
   * DC gain is exactly 1.0. Odd M gives symmetric linear-phase taps;
   * even M gives half-sample-shifted linear-phase taps.
   *
   * @param out  Output buffer; must hold at least M doubles.
   *             M outside the Bernoulli table range leaves out unmodified.
   * @param N    CIC filter order (number of integrator/comb stages, >= 1).
   * @param R    CIC decimation factor (>= 2).
   * @param M    Number of compensator taps in `[1, 19]` (odd or even).
   *
   * @code
   * >>> from doppler.resample import ciccompmf
   * >>> import numpy as np
   * >>> h = ciccompmf(4, 16, 5)
   * >>> h.shape, h.dtype
   * ((5,), dtype('float64'))
   * >>> [round(float(v), 4) for v in h]
   * [0.029, -0.282, 1.5061, -0.282, 0.029]
   * @endcode
   */
void ciccompmf(double *out, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
