#include "detection/detection_core.h"
#include <math.h>

/* Regularized incomplete beta I_x(a,b) via the standard continued
 * fraction (modified Lentz), the same numeric-kernel class as marcum_q.
 * Setup-path accuracy target ~1e-12; the CF converges in tens of terms
 * for the (a, b, x) ranges a quantile solve visits. */
static double
betacf (double a, double b, double x)
{
  const double eps = 1e-15, fpmin = 1e-300;
  double       qab = a + b, qap = a + 1.0, qam = a - 1.0;
  double       c = 1.0, d = 1.0 - qab * x / qap;
  if (fabs (d) < fpmin)
    d = fpmin;
  d        = 1.0 / d;
  double h = d;
  for (int m = 1; m <= 300; m++)
    {
      int    m2 = 2 * m;
      double aa = (double)m * (b - (double)m) * x
                  / ((qam + (double)m2) * (a + (double)m2));
      d         = 1.0 + aa * d;
      if (fabs (d) < fpmin)
        d = fpmin;
      c = 1.0 + aa / c;
      if (fabs (c) < fpmin)
        c = fpmin;
      d = 1.0 / d;
      h *= d * c;
      aa = -(a + (double)m) * (qab + (double)m) * x
           / ((a + (double)m2) * (qap + (double)m2));
      d  = 1.0 + aa * d;
      if (fabs (d) < fpmin)
        d = fpmin;
      c = 1.0 + aa / c;
      if (fabs (c) < fpmin)
        c = fpmin;
      d            = 1.0 / d;
      double delta = d * c;
      h *= delta;
      if (fabs (delta - 1.0) < eps)
        break;
    }
  return h;
}

static double
ibeta (double a, double b, double x)
{
  if (x <= 0.0)
    return 0.0;
  if (x >= 1.0)
    return 1.0;
  double lfront = lgamma (a + b) - lgamma (a) - lgamma (b) + a * log (x)
                  + b * log (1.0 - x);
  if (x < (a + 1.0) / (a + b + 2.0))
    return exp (lfront) * betacf (a, b, x) / a;
  return 1.0 - exp (lfront) * betacf (b, a, 1.0 - x) / b;
}

double
det_threshold_f (double pfa, int n)
{
  /* Upper quantile of F(n, n): the exact H0 law of a ratio-of-equal-DOF
   * chi-square statistic whose noise reference is ESTIMATED from the same
   * number of samples as the signal sum — the burst lock test's law.  A
   * chi-square gate (det_threshold_noncoherent) assumes a KNOWN noise
   * power and realizes tens of times the priced pfa here, because the
   * denominator's fluctuation fattens the ratio's tail.
   *
   * With X, Y ~ chi2(n) iid, P(X/Y > g) = P(Y/(X+Y) < 1/(1+g))
   * = I_{1/(1+g)}(n/2, n/2), so the quantile is a bisection on the
   * regularized incomplete beta — half-integer orders included, no
   * even-n restriction. */
  if (!(pfa > 0.0 && pfa < 1.0) || n < 1)
    return 0.0;
  double a = 0.5 * (double)n;
  /* Solve I_x(a, a) = pfa for x in (0, 1); g = (1 - x)/x.  I_x(a,a) is
   * strictly increasing in x, so plain bisection is unconditionally
   * robust; 200 halvings reach ~1e-60 in x, far past double precision. */
  double lo = 0.0, hi = 1.0;
  for (int i = 0; i < 200; i++)
    {
      double mid = 0.5 * (lo + hi);
      if (ibeta (a, a, mid) < pfa)
        lo = mid;
      else
        hi = mid;
    }
  double x = 0.5 * (lo + hi);
  return (1.0 - x) / x;
}
