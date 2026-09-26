#include "doppler/detection/detection_core.h"
int
dp_det_dwell_power (double snr_power, double pd_min, double pfa, int max_dwell)
{
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd_min > 0.0 && pd_min < 1.0))
    return -1;
  double p = dp_det_threshold_power (pfa);
  for (int m = 1; m <= max_dwell; m++)
    if (dp_det_pd_power (snr_power, m, p) >= pd_min)
      return m;
  return -1;
}
