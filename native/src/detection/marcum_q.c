/*
 * marcum_q.c — detection module-level function.
 */
#include "detection/detection_core.h"
#include <math.h>

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

/* Regularized upper incomplete gamma Q(a,x) = Gamma(a,x)/Gamma(a), via
 * the standard continued fraction / series pair (modified Lentz),
 * log-domain prefactor -- the SAME numeric-kernel class as
 * det_threshold_f.c's regularized incomplete beta (ibeta/betacf),
 * reused here rather than re-derived. This is what fixes marcum_q's
 * own underflow bug: the previous chi-square/Poisson CDF term was
 * built by a term-by-term recurrence starting from a raw exp(-v) --
 * for v large enough that exp(-v) underflows to exactly 0.0 (v
 * gtrsim 745, i.e. b gtrsim ~39, which real det_n_noncoh() calls at
 * large dwell counts reach routinely), EVERY subsequent term stayed
 * stuck at exactly 0 (0 times any finite ratio is still 0), silently
 * corrupting the result -- observed directly as Pd collapsing from
 * ~0.96 to exactly 0.0 between two adjacent dwell counts. Computing
 * the log-domain prefactor BEFORE exponentiating (as ibeta already
 * does) never touches an already-underflowed intermediate value. */
static double
gcf (double a, double x)
{
  const double eps = 1e-15, fpmin = 1e-300;
  double       b = x + 1.0 - a, c = 1.0 / fpmin, d = 1.0 / b, h = d;
  for (int i = 1; i <= 300; i++)
    {
      double ai = (double)i;
      double an = -ai * (ai - a);
      b += 2.0;
      d = an * d + b;
      if (fabs (d) < fpmin)
        d = fpmin;
      c = b + an / c;
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
gser (double a, double x)
{
  double sum = 1.0 / a, term = sum;
  for (int i = 1; i <= 300; i++)
    {
      term *= x / (a + (double)i);
      sum += term;
      if (fabs (term) < fabs (sum) * 1e-15)
        break;
    }
  return sum;
}

static double
gammaq (double a, double x)
{
  if (x <= 0.0)
    return 1.0;
  double lfront = -x + a * log (x) - lgamma (a);
  if (x < a + 1.0)
    return 1.0 - exp (lfront) * gser (a, x);
  return exp (lfront) * gcf (a, x);
}

/* P(Poisson(v) <= n-1) = Q(n, v) exactly (standard Poisson/gamma
 * duality, integer shape). */
static double
_poisson_cdf (int n, double v)
{
  return gammaq ((double)n, v);
}

/* exp(-v)*v^n/n! (the Poisson(v) PMF at n), a single log-domain
 * evaluation -- always safe (never built from a possibly-underflowed
 * predecessor the way the old recurrence's seed term was). */
static double
_poisson_pmf (double v, int n)
{
  return exp ((double)n * log (v) - v - lgamma ((double)n + 1.0));
}

double
marcum_q (int m, double a, double b)
{
  const double EPS = 1e-14;
  if (b <= 0.0)
    return 1.0;
  if (a > b)
    {
      double q_approx = 0.5 * erfc ((b - a) * M_SQRT1_2);
      if (q_approx >= 1.0 - EPS)
        return 1.0;
    }
  double v = 0.5 * b * b;
  if (a < EPS)
    return _poisson_cdf (m, v);

  /* The outer sum Q_M(a,b) = sum_k w_k * Q_{M+k}(0,b), w_k =
   * Poisson(u) pmf at k (u = a^2/2), had the SAME underflow failure
   * as the v-side (starting w from a raw exp(-u) that can underflow
   * to exactly 0.0 for large u -- reachable at large dwell/design_snr
   * combinations), PLUS a second, independent bug: the old fixed
   * `k < 600` window always started scanning at k=0, but a Poisson(u)
   * distribution's mass sits at k ~ u with std ~ sqrt(u) -- for large
   * u that mass is entirely OUTSIDE a 600-wide window started at 0,
   * so the sum silently missed it. Fixed the same way as the v-side:
   * anchor at the distribution's own mode (k0 = round(u), computed via
   * a single safe log-domain term) and walk OUTWARD in both
   * directions, with a window that scales with the mode's own spread
   * (sqrt(u)) instead of a fixed constant. */
  double u  = 0.5 * a * a;
  int    k0 = (int)(u + 0.5);
  if (k0 < 0)
    k0 = 0;
  int margin = (int)(12.0 * sqrt (u + 1.0)) + 60;

  double w0       = _poisson_pmf (u, k0);
  double chi_sum0 = _poisson_cdf (m + k0, v);
  double result   = w0 * chi_sum0;

  /* Walk UP (k > k0): w_k and chi_sum both extend via cheap forward
   * ratios, seeded from the safe anchor above. */
  {
    double w = w0, chi_sum = chi_sum0;
    double chi_term = _poisson_pmf (v, m + k0);
    for (int k = k0 + 1; k <= k0 + margin; k++)
      {
        w *= u / (double)k;
        chi_sum += chi_term;
        chi_term *= v / (double)(m + k);
        double contrib = w * chi_sum;
        result += contrib;
        if (contrib < EPS * result)
          break;
      }
  }
  /* Walk DOWN (k < k0): w_k via the reverse ratio; chi_sum via the
   * reverse relation chi_sum(k) = chi_sum(k+1) - pmf(v, m+k). */
  {
    double w = w0, chi_sum = chi_sum0;
    for (int k = k0 - 1; k >= 0 && k >= k0 - margin; k--)
      {
        w *= (double)(k + 1) / u;
        chi_sum -= _poisson_pmf (v, m + k);
        if (chi_sum < 0.0)
          chi_sum = 0.0;
        double contrib = w * chi_sum;
        result += contrib;
        if (contrib < EPS * result)
          break;
      }
  }
  return result;
}
