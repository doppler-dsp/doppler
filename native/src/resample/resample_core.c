/*
 * resample_core.c — Module-level design helpers for the resample module.
 *
 * These scalar functions are exposed to Python via resample_ext.c.
 * The polyphase engine lives in resamp_core.c (linked as resamp_core OBJECT).
 */
#include "resample/resample_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief Kaiser window beta from stopband attenuation.
 *
 * Classic Parks-McClellan / Kaiser empirical formula.
 *
 * @param atten  Stopband attenuation in dB (positive).
 * @return beta parameter for scipy.signal.kaiser or build_bank.
 */
double
kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

/**
 * @brief Taps-per-phase from Kaiser design spec.
 *
 * Computes the prototype filter length using the Kaiser order estimate,
 * then divides by num_phases to get taps per polyphase branch.
 *
 * @param num_phases  Number of polyphase branches (must be >= 1).
 * @param atten       Stopband attenuation in dB.
 * @param pb          Normalised passband edge (0 < pb < sb < 0.5).
 * @param sb          Normalised stopband edge.
 * @return Taps per phase (>= 1).
 */
int
kaiser_num_taps (int num_phases, double atten, double pb, double sb)
{
  double pb_ph = pb / (double)num_phases;
  double sb_ph = sb / (double)num_phases;
  double tw = sb_ph - pb_ph; /* transition width per phase */
  size_t proto = (size_t)(1.0 + (atten - 8.0) / (2.285 * (2.0 * M_PI * tw)));
  size_t halflen = proto / 2;
  size_t htaps = 2 * halflen + 1;
  return (int)(htaps / (size_t)num_phases + 1);
}
