#include "doppler/detection/detection_core.h"
#include <math.h>
double
det_threshold (double pfa)
{
  /* A NaN threshold never fires: every comparison against it is false. */
  if (!(pfa > 0.0 && pfa < 1.0))
    return NAN;
  return sqrt (-2.0 * log (pfa));
}
