/*
 * marcum_q.c — detection module-level function.
 */
#include "detection/detection_core.h"
#include <math.h>

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

double
marcum_q(int m, double a, double b)
{
  const double EPS = 1e-14;
  if (b <= 0.0) return 1.0;
  if (a > b) {
      double q_approx = 0.5 * erfc((b - a) * M_SQRT1_2);
      if (q_approx >= 1.0 - EPS) return 1.0;
  }
  if (a < EPS) {
      double v = 0.5 * b * b, t = exp(-v), s = 0.0;
      for (int j = 0; j < m; j++) { s += t; t *= v / (j + 1); }
      return s;
  }
  double u = 0.5*a*a, v = 0.5*b*b, expv = exp(-v);
  double chi_term = expv, chi_sum = 0.0;
  for (int j = 0; j < m; j++) { chi_sum += chi_term; chi_term *= v/(j+1); }
  double w = exp(-u), result = 0.0;
  for (int k = 0; k < 600; k++) {
      double contrib = w * chi_sum;
      result += contrib;
      if (k > m+10 && contrib < EPS * result) break;
      chi_sum += chi_term; chi_term *= v/(m+k+1); w *= u/(k+1);
  }
  return result;
}
