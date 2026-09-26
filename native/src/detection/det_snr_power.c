#include "doppler/detection/detection_core.h"
#include <math.h>
double
dp_det_snr_power (int dwell, double pd_min, double pfa)
{
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd_min > 0.0 && pd_min < 1.0))
    return NAN;
  double s = dp_det_snr (dwell, pd_min, pfa);
  return s * s;
}
