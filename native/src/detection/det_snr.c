#include "detection/detection_core.h"
double
det_snr (int dwell, double pd_min, double pfa)
{
  double eta = det_threshold (pfa), lo = 0.0, hi = 1.0;
  while (det_pd (hi, dwell, eta) < pd_min)
    hi *= 2.0;
  for (int i = 0; i < 64; i++)
    {
      double mid = 0.5 * (lo + hi);
      if (det_pd (mid, dwell, eta) >= pd_min)
        hi = mid;
      else
        lo = mid;
    }
  return 0.5 * (lo + hi);
}
