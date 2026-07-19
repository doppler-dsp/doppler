#include "detection/detection_core.h"
int
det_n_noncoh (double snr, int n_coh, double pd_min, double pfa,
              int max_n_noncoh)
{
  /* Smallest number of non-coherent looks meeting Pd, for a fixed coherent
   * depth n_coh. Pd(k) (the CFAR threshold is recomputed per candidate, so
   * the degrees-of-freedom growth is priced in) is monotonically non-
   * decreasing in k -- true in theory, and now true in this
   * implementation too: marcum_q's own underflow bug (see marcum_q.c)
   * used to break monotonicity outright at large k, which made a linear
   * scan the only safe option. With that fixed, binary search turns what
   * was an O(max_n_noncoh) scan (each step itself O(k) before the
   * marcum_q fix) into O(log max_n_noncoh) steps. */
  double eta_max = det_threshold_noncoherent (pfa, max_n_noncoh);
  if (det_pd_noncoherent (snr, n_coh, max_n_noncoh, eta_max) < pd_min)
    return -1;

  int lo = 1, hi = max_n_noncoh;
  while (lo < hi)
    {
      int    mid = lo + (hi - lo) / 2;
      double eta = det_threshold_noncoherent (pfa, mid);
      if (det_pd_noncoherent (snr, n_coh, mid, eta) >= pd_min)
        hi = mid;
      else
        lo = mid + 1;
    }
  return lo;
}
