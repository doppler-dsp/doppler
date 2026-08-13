#include "detection/detection_core.h"
#include <math.h>

/* Inverse complementary error function via a Winitzki (2008) rational
 * initial guess refined to ~machine precision by Newton's method on
 * erfc(x) - y = 0 (erfc() is standard C99).
 *
 * Promoted from symsync_core.c, which carried it privately with the note
 * that it "belongs in the detection module" once a second consumer
 * appeared. The carrier lock detector is that consumer. */
static double
erfcinv_ (double y)
{
  double x      = 1.0 - y; /* erfcinv(y) == erfinv(1 - y) */
  double ln1mx2 = log (1.0 - x * x);
  double a      = 0.147;
  double t1     = 2.0 / (M_PI * a) + ln1mx2 / 2.0;
  double inner  = t1 * t1 - ln1mx2 / a;
  double r      = sqrt (fmax (inner, 0.0)) - t1;
  double guess  = copysign (sqrt (fmax (r, 0.0)), x);
  for (int i = 0; i < 4; i++)
    {
      double fx  = erfc (guess) - y;
      double dfx = -2.0 / sqrt (M_PI) * exp (-guess * guess);
      guess -= fx / dfx;
    }
  return guess;
}

/* SIGNED, and that is load-bearing: above the median the quantile is
 * NEGATIVE, and det_dwell_gauss's separation Q_inv(pfa) - Q_inv(pd) is a
 * sum of two tails only because Q_inv(pd) < 0 for the pd > 0.5 every
 * caller uses. Clamping this to 0 there silently halves the dwell -- it
 * is the first thing this function got wrong, caught by the sign check
 * in test_detection_core.c. Only p outside (0, 1) fails closed. */
double
det_q_inv (double p)
{
  if (!(p > 0.0) || !(p < 1.0))
    return 0.0;
  return M_SQRT2 * erfcinv_ (2.0 * p);
}
