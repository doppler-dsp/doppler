#include "detection/detection_core.h"
int
det_n_noncoh (double snr, int n_coh, double pd_min, double pfa,
              int max_n_noncoh)
{
  /* Smallest number of non-coherent looks meeting Pd, for a fixed coherent
   * depth n_coh.  The CFAR threshold grows with the look count (more degrees
   * of freedom under H0), so it is recomputed per candidate. */
  for (int k = 1; k <= max_n_noncoh; k++)
    {
      double eta = det_threshold_noncoherent (pfa, k);
      if (det_pd_noncoherent (snr, n_coh, k, eta) >= pd_min)
        return k;
    }
  return -1;
}
