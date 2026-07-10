/*
 * snr_m2m4_db.c — snr module-level function.
 *
 * snr_m2m4_db_series.c calls this directly on slices, so the algorithm
 * lives exactly once (C-first) without a separate kernel file jm's
 * CMakeLists reconciliation would otherwise drop on regeneration.
 */
#include "snr/snr_core.h"

#include <math.h>

double
snr_m2m4_db (const float complex *x, size_t x_len)
{
  if (x_len == 0)
    return NAN;
  double m2 = 0.0, m4 = 0.0;
  for (size_t i = 0; i < x_len; i++)
    {
      double re = (double)crealf (x[i]);
      double im = (double)cimagf (x[i]);
      double p  = re * re + im * im;
      m2 += p;
      m4 += p * p;
    }
  m2 /= (double)x_len;
  m4 /= (double)x_len;
  if (m2 <= 0.0)
    return NAN; /* no signal, no noise: undefined */
  double disc = 2.0 * m2 * m2 - m4;
  if (disc < 0.0)
    disc = 0.0; /* finite-sample noise can push this slightly negative */
  double root = sqrt (disc);
  if (m2 <= root) /* noiseless (or numerical tie): no noise term */
    return INFINITY;
  return 10.0 * log10 (root / (m2 - root));
}
