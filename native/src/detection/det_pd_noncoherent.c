#include "detection/detection_core.h"
#include <math.h>
double
det_pd_noncoherent (double snr, int n_coh, int n_noncoh, double threshold)
{
  /* n_noncoh non-coherent looks, each a coherent integration of n_coh samples.
   * Per look the peak power is a noncentral chi-square (2 dof) with non-
   * centrality 2*n_coh*snr^2; summing n_noncoh looks gives 2*n_noncoh dof with
   * total non-centrality 2*n_coh*n_noncoh*snr^2.  Hence
   *   Pd = Q_{n_noncoh}(sqrt(2*n_coh*n_noncoh)*snr, threshold).
   * At n_noncoh == 1 this is exactly det_pd(snr, n_coh, threshold). */
  double a = sqrt (2.0 * (double)n_coh * (double)n_noncoh) * snr;
  return marcum_q (n_noncoh, a, threshold);
}
