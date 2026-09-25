#include "doppler/detection/detection_core.h"
int
det_dwell (double snr, double pd_min, double pfa, int max_dwell)
{
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd_min > 0.0 && pd_min < 1.0))
    return -1;
  double eta = det_threshold (pfa);
  for (int m = 1; m <= max_dwell; m++)
    if (det_pd (snr, m, eta) >= pd_min)
      return m;
  return -1;
}
