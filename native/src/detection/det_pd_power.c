#include "detection/detection_core.h"
#include <math.h>
double
det_pd_power (double snr_power, int dwell, double power_threshold)
{
  return marcum_q (1, sqrt (2.0 * dwell * snr_power),
                   sqrt (2.0 * power_threshold));
}
