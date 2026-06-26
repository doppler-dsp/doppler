#include "detection/detection_core.h"
#include <math.h>
double
det_threshold_noncoherent (double pfa, int n_noncoh)
{
  /* Order-1 has the exact closed form; reuse it so n_noncoh == 1 is bit-
   * identical to det_threshold(). */
  if (n_noncoh <= 1)
    return det_threshold (pfa);

  /* The non-coherent null statistic R = sqrt(sum of n_noncoh unit |z|^2) has
   * P(R > b) = marcum_q(n_noncoh, 0, b), monotone decreasing in b.  Solve
   * marcum_q(n_noncoh, 0, b) = pfa by bisection.  Bracket [0, hi]: f(0) = 1
   * > pfa, double hi until f(hi) < pfa. */
  double lo = 0.0;
  double hi = sqrt (-2.0 * log (pfa));
  if (hi < 1.0)
    hi = 1.0;
  while (marcum_q (n_noncoh, 0.0, hi) > pfa)
    hi *= 2.0;
  for (int i = 0; i < 100; i++)
    {
      double mid = 0.5 * (lo + hi);
      if (marcum_q (n_noncoh, 0.0, mid) > pfa)
        lo = mid;
      else
        hi = mid;
    }
  return 0.5 * (lo + hi);
}
