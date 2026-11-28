/*
 * correlator_same_code_phase.c — correlation module-level function: the
 * one key that says two code phases are one signal. The contract lives on
 * the declaration in correlation/correlation_core.h.
 */
#include "doppler/correlation/correlation_core.h"
#include "doppler/correlator/correlator_core.h"
#include <math.h>

int
dp_correlator_same_code_phase (double a, double b, double code_len)
{
  double d = fabs (a - b);
  if (d > code_len / 2.0)
    d = code_len - d;
  return d <= DP_CORRELATOR_SAME_PHASE_CHIPS;
}
