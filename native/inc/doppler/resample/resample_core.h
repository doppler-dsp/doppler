/**
 * @file resample_core.h
 * @brief Resample module — public C API.
 */
#ifndef DP_RESAMPLE_CORE_H
#define DP_RESAMPLE_CORE_H

#include "doppler/clib_common.h"
#include <math.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

/**
 * @brief The Kaiser beta formula itself: the one implementation behind
 *        dp_kaiser_beta() and resamp's built-in bank.
 *
 * Header-inline so that resamp_core, which sits below this module in the
 * link graph, computes beta with this code and not a private copy of it,
 * without a link edge to resample_core (doppler#2652). dp_kaiser_beta() is
 * a call to it; prefer that public function everywhere else.
 *
 * @param atten  Desired stopband attenuation in dB.
 * @return       Kaiser beta: 0.1102 * (atten - 8.7) above 50 dB,
 *               0.5842 * (atten - 21)^0.4 + 0.07886 * (atten - 21) from
 *               21 to 50 dB, and 0.0 below 21 dB.
 */
static inline double
dp_kaiser_beta_eval (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

/**
 * @brief The Kaiser taps-per-phase formula itself: the one implementation
 *        behind dp_kaiser_num_taps() and resamp's built-in bank.
 *
 * Header-inline for the same reason as dp_kaiser_beta_eval() (doppler#2652).
 * @p num_phases must be at least 1, which dp_kaiser_num_taps() checks before
 * calling it. Every other argument is total: the prototype length floors at
 * one tap where the formula falls below it (an @p atten under 8 dB, or a
 * NaN) and caps at 2^52, so the conversion to size_t is always defined.
 *
 * @param num_phases  Number of polyphase branches, >= 1.
 * @param atten       Desired stopband attenuation in dB; below 8 dB the
 *                    prototype is one tap.
 * @param pb          Passband edge, as for dp_kaiser_num_taps().
 * @param sb          Stopband edge, same units as @p pb.
 * @return            Taps per polyphase branch: the Kaiser length of the
 *                    per-phase prototype, made odd, over @p num_phases,
 *                    plus one.
 */
static inline size_t
dp_kaiser_num_taps_eval (size_t num_phases, double atten, double pb,
                         double sb)
{
  const double pi      = 3.14159265358979323846;
  double       pb_ph   = pb / (double)num_phases;
  double       sb_ph   = sb / (double)num_phases;
  double       tw      = sb_ph - pb_ph;
  double       len     = 1.0 + (atten - 8.0) / (2.285 * (2.0 * pi * tw));
  /* Total over every double: below 8 dB (or for a NaN) the formula falls
     under one tap, and casting a negative double to size_t is undefined
     behaviour, so the prototype floors at one tap, the formula's own value
     at 8 dB. The cap keeps an absurd attenuation's cast defined too; no
     bank comes near 2^52 taps. */
  size_t proto = !(len > 1.0) ? 1u
                 : len >= 4503599627370496.0 ? (size_t)4503599627370496.0
                                             : (size_t)len;
  size_t       halflen = proto / 2;
  size_t       htaps   = 2 * halflen + 1;
  return htaps / num_phases + 1;
}

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
double dp_kaiser_beta(double atten);

/**
 * @brief Estimate the taps-per-phase count for a polyphase Kaiser FIR bank.
 * Applies the Kaiser length formula to the per-phase normalised prototype
 * (pb/num_phases, sb/num_phases), rounds up to the next odd symmetrical
 * length, then divides by num_phases to give taps per branch. The result
 * is the minimum num_taps argument to pass to dp_Resampler_create_custom().
 *
 * @param num_phases  Number of polyphase branches (power of two). A value
 *                    below 1 is not a bank; the function returns 0.
 * @param atten       Desired stopband attenuation in dB. Below 8 dB the
 *                    Kaiser length formula falls under one tap, and the
 *                    prototype is floored at one.
 * @param pb          Passband edge in cycles per sample at the branch
 *                    (input) rate, so Nyquist is 0.5 (0 < pb < sb < 1).
 * @param sb          Stopband edge, same units as @p pb.
 * @return            Taps per polyphase branch (>= 1).
 *
 * @code
 * >>> from doppler.resample import kaiser_num_taps
 * >>> kaiser_num_taps(4096, 60.0, 0.4, 0.6)
 * 19
 * @endcode
 */
int dp_kaiser_num_taps(int num_phases, double atten, double pb, double sb);

  /**
   * @brief Design a CIC passband-droop compensator FIR filter.
   * Implements the closed-form Bernoulli-series maximally-flat-error
   * method from Molnar & Vucic (IEEE TCAS-II 58(12):926-930, 2011,
   * DOI 10.1109/TCSII.2011.2172522). The compensator runs at the
   * *decimated* (output) rate and should be applied after the CIC stage.
   * DC gain is exactly 1.0. Odd M gives symmetric linear-phase taps;
   * even M gives half-sample-shifted linear-phase taps.
   *
   * @param out  Output buffer; must hold at least M doubles. ALWAYS M --
   *             an out-of-range M is written as M zeros, not left alone,
   *             so a caller cannot pre-fill a fallback design here and
   *             expect it to survive. Size by M, never by the table bound.
   * @param N    CIC filter order (number of integrator/comb stages, >= 1).
   * @param R    CIC decimation factor (>= 2).
   * @param M    Number of compensator taps: odd M in `[1, 19]`, even M in
   *             `[1, 18]`. The Bernoulli table is nine entries, so the two
   *             parities do not reach the same length; anything outside
   *             its own range yields the all-zero filter above.
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
void dp_ciccompmf(double *out, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
