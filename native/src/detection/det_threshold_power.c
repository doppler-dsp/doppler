#include "doppler/detection/detection_core.h"
#include <math.h>
double
dp_det_threshold_power (double pfa)
{
  if (!(pfa > 0.0 && pfa < 1.0))
    return NAN;
  return -log (pfa);
}
