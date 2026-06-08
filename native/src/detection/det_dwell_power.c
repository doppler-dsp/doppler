#include "detection/detection_core.h"
int
det_dwell_power (double snr_power, double pd_min, double pfa, int max_dwell)
{
  double p = det_threshold_power (pfa);
  for (int m = 1; m <= max_dwell; m++)
    if (det_pd_power (snr_power, m, p) >= pd_min)
      return m;
  return -1;
}
