#include "doppler/detection/detection_core.h"
#include "doppler/util/util_core.h"
#include <math.h>

/* Gauss-Hermite nodes over the reference's Gaussian spread: 2 match 6 to
   5e-5 on this Pd, far under anything a certification resolves, and a
   burst acquisition's sizing evaluates it ~13k times a depth
   (doppler#1501). */
#define DET_CFAR_GH_NODES 2

double
det_pd_cfar (double snr, int dwell, double threshold, double k, double leak,
             double leak_cells)
{
  /* The gate in mean-magnitude units: a Rayleigh cell's mean is
     sqrt(pi/2), so eta on the known-noise statistic is T = eta*sqrt(2/pi)
     times the reference's mean. */
  const double t = threshold * sqrt (2.0 / M_PI);
  if (!(k > t + 1.0))
    return det_pd (snr, dwell, threshold);
  const double kk = k - 1.0;
  const double eo = leak > 0.0 ? leak : 0.0;
  const double m  = leak_cells > 0.0 ? fmin (leak_cells, kk) : kk;
  /* The mean magnitude of the OTHER k-1 cells: m of them share the leaked
     energy eo, each ~ sqrt(pi/2 + eo/m); the rest are noise, sqrt(pi/2). */
  const double mu
      = (m * sqrt (M_PI / 2.0 + eo / m) + (kk - m) * sqrt (M_PI / 2.0)) / kk;
  const double sd = sqrt ((4.0 - M_PI) / 2.0 / kk);
  /* The peak's own share of the mean moved to the left of the gate:
     R (1 - T/k) > T (k-1)/k S, so the peak faces T (k-1)/(k-T) S. */
  const double g = t * kk / (k - t);
  double       z[DET_CFAR_GH_NODES], p[DET_CFAR_GH_NODES];
  (void)gauss_hermite (z, DET_CFAR_GH_NODES, p, DET_CFAR_GH_NODES);
  double acc = 0.0;
  for (int j = 0; j < DET_CFAR_GH_NODES; j++)
    acc += p[j] * det_pd (snr, dwell, g * (mu + sd * z[j]));
  return acc;
}
