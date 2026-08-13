#include "detection/detection_core.h"
#include <math.h>

/* thresh = Q_inv(pfa) * mean / (Q_inv(pfa) - Q_inv(pd)): the point where
 * the H0 tail above it and the H1 tail below it meet both budgets at once.
 * Independent of the variance -- that sets how many looks are needed to
 * reach this point, not where the point is. */
double
det_threshold_gauss (double mean, double pd, double pfa)
{
  if (!(mean > 0.0))
    return 0.0;
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0) || !(pd > pfa))
    return 0.0;
  double qa = det_q_inv (pfa), sep = qa - det_q_inv (pd);
  if (!(sep > 0.0))
    return 0.0;
  return qa * mean / sep;
}
