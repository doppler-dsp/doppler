#include "doppler/clib_common.h"
#include "doppler/detection/detection_core.h"
#include <math.h>
#include <stdint.h>

/* The detection probability of a block of n at the threshold for (n,
   pfa): P(R > r) = P(W > 0), W = sum over the block of (1 - r) I^2 - (1 +
   r) Q^2, I and Q unit-variance Gaussians about A cos(theta) and A sin(theta)
   (A^2 = 2 gamma, theta Gaussian at the jitter, independent a update).
   Exact for that model, by inverting W's characteristic function
   (Gil-Pelaez): P(W > 0) = 1/2 + (1/pi) int_0^inf Im phi_W(t) / t dt. Given
   theta, a X^2 with X ~ N(mu, 1) has phi = (1 - 2iat)^(-1/2) exp(i a t
   mu^2 / (1 - 2iat)); the theta average is a quadrature over +/-8 sigma
   with the Gaussian's weights, and phi_W is the update's to the power len.
   The t axis is scaled by W's own sd and integrated by Simpson to 40 sd's
   reciprocal, past which |phi_W| is negligible (it falls as a power len of
   t). */
#define DET_RATIO_PD_NTH 33  /* theta nodes (odd: one at 0)         */
#define DET_RATIO_PD_NT 1500 /* Simpson panel pairs on the t axis  */
#define DET_RATIO_PD_U 40.0  /* the t axis's end, in 1 / sd(W)     */
double
dp_det_ratio_pd (int n, double pfa, double gamma, double phase_var)
{
  if (n < 1 || !(isfinite (gamma) && gamma >= 0.0)
      || !(isfinite (phase_var) && phase_var >= 0.0))
    return NAN;
  const double r = dp_det_ratio_threshold (pfa, n);
  if (!isfinite (r))
    return NAN;
  const uint32_t len = (uint32_t)n;
  const double   a = 1.0 - r, b = 1.0 + r, a2 = 2.0 * gamma;
  const double   sg = sqrt (phase_var);
  double c2[DET_RATIO_PD_NTH], s2[DET_RATIO_PD_NTH], wt[DET_RATIO_PD_NTH];
  int    nth = 1;
  c2[0] = 1.0, s2[0] = 0.0, wt[0] = 1.0;
  if (sg > 0.0)
    {
      double tot = 0.0;
      nth        = DET_RATIO_PD_NTH;
      for (int j = 0; j < nth; j++)
        {
          const double th = 8.0 * sg * (2.0 * j / (nth - 1.0) - 1.0);
          c2[j]           = cos (th) * cos (th);
          s2[j]           = sin (th) * sin (th);
          wt[j]           = exp (-th * th / (2.0 * phase_var));
          tot += wt[j];
        }
      for (int j = 0; j < nth; j++)
        wt[j] /= tot;
    }
  /* One update's mean and variance of (1 - r) I^2 - (1 + r) Q^2. */
  double ec = 0.0, es = 0.0;
  for (int j = 0; j < nth; j++)
    ec += wt[j] * c2[j], es += wt[j] * s2[j];
  const double m = a * (1.0 + a2 * ec) - b * (1.0 + a2 * es);
  double v = a * a * (2.0 + 4.0 * a2 * ec) + b * b * (2.0 + 4.0 * a2 * es);
  for (int j = 0; j < nth; j++)
    {
      const double dm = a2 * (a * (c2[j] - ec) - b * (s2[j] - es));
      v += wt[j] * dm * dm;
    }
  const double sdw = sqrt ((double)len * v);
  const double h   = DET_RATIO_PD_U / (2.0 * DET_RATIO_PD_NT);
  double       acc = (double)len * m / sdw; /* the integrand's limit at 0 */
  /* In real arithmetic, so no complex library is needed (dp_complex.h
     routes only the float names): with da = 1 - 2iat and db = 1 + 2ibt,
     a c2 / da - b s2 / db = X_r + i X_i, the update's theta term is
     exp(-t a2 X_i) (cos + i sin)(t a2 X_r), and phi_W is taken in polar
     form, its log-magnitude and its angle each times len. */
  for (int k = 1; k <= 2 * DET_RATIO_PD_NT; k++)
    {
      const double u = h * k, t = u / sdw;
      const double qa = 1.0 + 4.0 * a * a * t * t,
                   qb = 1.0 + 4.0 * b * b * t * t;
      double       er = 0.0, ei = 0.0;
      for (int j = 0; j < nth; j++)
        {
          const double xr = a * c2[j] / qa - b * s2[j] / qb;
          const double xi
              = 2.0 * t * (a * a * c2[j] / qa + b * b * s2[j] / qb);
          const double mg = wt[j] * exp (-t * a2 * xi);
          er += mg * cos (t * a2 * xr);
          ei += mg * sin (t * a2 * xr);
        }
      const double lmag
          = 0.5 * log (er * er + ei * ei) - 0.25 * (log (qa) + log (qb));
      const double ang
          = atan2 (ei, er)
            - 0.5 * (atan2 (-2.0 * a * t, 1.0) + atan2 (2.0 * b * t, 1.0));
      const double f = exp ((double)len * lmag) * sin ((double)len * ang) / u;
      acc += (k == 2 * DET_RATIO_PD_NT ? 1.0 : k % 2 ? 4.0 : 2.0) * f;
    }
  return 0.5 + h / 3.0 * acc / 3.141592653589793;
}
