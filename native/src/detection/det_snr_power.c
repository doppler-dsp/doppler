#include "detection/detection_core.h"
double
det_snr_power (int dwell, double pd_min, double pfa)
{
  double s = det_snr (dwell, pd_min, pfa);
  return s * s;
}
