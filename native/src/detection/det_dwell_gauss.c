#include "detection/detection_core.h"
#include <limits.h>
#include <math.h>

/* n = var * ((Q_inv(pfa) - Q_inv(pd)) / mean)^2.
 *
 * Q_inv(pd) is negative for pd > 0.5, so the difference is the total
 * separation both tails must fit inside; block-averaging n looks shrinks
 * the H0 spread as 1/n until it does. */
int
det_dwell_gauss (double mean, double var, double pd, double pfa)
{
  if (!(mean > 0.0) || !(var > 0.0))
    return -1;
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0) || !(pd > pfa))
    return -1;
  double sep = det_q_inv (pfa) - det_q_inv (pd);
  double n   = var * (sep / mean) * (sep / mean);
  if (!(n >= 1.0))
    return 1;
  if (n > (double)INT_MAX)
    return INT_MAX;
  return (int)ceil (n);
}
