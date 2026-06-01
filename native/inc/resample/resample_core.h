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

double kaiser_beta(double atten);

int kaiser_num_taps(int num_phases, double atten, double pb, double sb);

  /**
   * Design a CIC passband-droop compensator FIR filter.
   *
   * Implements the closed-form Bernoulli-series (maximally-flat error)
   * method described in:
   *
   *   Molnar & Vucic, "Closed-Form Design of CIC Compensators Based on
   *   Maximally Flat Error Criterion," IEEE TCAS-II, 58(12):926-930, 2011.
   *   DOI: 10.1109/TCSII.2011.2172522
   *
   * The compensator runs at the *decimated* (output) rate and should be
   * applied after the CIC decimator.
   *
   * Bernoulli table has 9 entries (B_2 ... B_18). Valid tap counts:
   *   odd  M: 1, 3, 5, ... 19  (half = (M-1)/2 <= 9)
   *   even M: 2, 4, 6, ... 18  (half = M/2     <= 9)
   * M outside these ranges -> h is left unmodified.
   *
   * @param h  Output buffer, caller-allocated, M elements. DC gain = 1.0.
   * @param N  CIC filter order (number of integrator/comb stages, >= 1).
   * @param R  CIC decimation factor (>= 2).
   * @param M  Number of compensator taps. Odd M -> symmetric linear-phase;
   *           even M -> half-sample-shifted linear-phase.
   */
void ciccompmf(double *out, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
