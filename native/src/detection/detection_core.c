#include "detection/detection_core.h"
#include <math.h>

#ifndef M_SQRT1_2
#define M_SQRT1_2 0.70710678118654752440
#endif

double
marcum_q (int m, double a, double b)
{
  const double EPS = 1e-14;

  if (b <= 0.0)
    return 1.0;

  /*
   * Gaussian fast path: for a >> b the Rice CDF tail Q_1(a, b) approaches
   * 1 faster than the Poisson series can accumulate (exp(-a^2/2) underflows
   * before the weights peak at k ~ a^2/2).  When the erfc approximation
   * rounds to 1.0 in double precision, the true Q_M value is within 1 ULP
   * of 1.0 as well.
   */
  if (a > b)
    {
      double q_approx = 0.5 * erfc ((b - a) * M_SQRT1_2);
      if (q_approx >= 1.0 - EPS)
        return 1.0;
    }

  /* Q_M(0, b) = exp(-v) * sum_{j=0}^{M-1} v^j/j!  — exact, no series */
  if (a < EPS)
    {
      double v = 0.5 * b * b;
      double t = exp (-v);
      double s = 0.0;
      for (int j = 0; j < m; j++)
        {
          s += t;
          t *= v / (j + 1);
        }
      return s;
    }

  double u    = 0.5 * a * a;
  double v    = 0.5 * b * b;
  double expv = exp (-v);

  /*
   * Initialize the chi-sum Q_M(0, b) and its next increment chi_term.
   *
   * After the loop:
   *   chi_sum  = exp(-v) * sum_{j=0}^{M-1} v^j/j!  = Q_M(0, b)
   *   chi_term = exp(-v) * v^M / M!                 (first new term)
   *
   * Each outer iteration adds chi_term to chi_sum (advancing from
   * Q_{M+k} to Q_{M+k+1}) and multiplies chi_term by v/(M+k+1).
   */
  double chi_term = expv; /* exp(-v) * v^0 / 0! */
  double chi_sum  = 0.0;
  for (int j = 0; j < m; j++)
    {
      chi_sum  += chi_term;
      chi_term *= v / (j + 1);
    }

  double w      = exp (-u); /* Poisson weight for k=0: exp(-u)*u^0/0! */
  double result = 0.0;

  for (int k = 0; k < 600; k++)
    {
      double contrib = w * chi_sum;
      result        += contrib;

      if (k > m + 10 && contrib < EPS * result)
        break;

      /* chi_sum_{k+1} = chi_sum_k + exp(-v) * v^{M+k} / (M+k)! */
      chi_sum  += chi_term;
      chi_term *= v / (m + k + 1);
      w        *= u / (k + 1);
    }

  return result;
}

double
det_threshold (double pfa)
{
  return sqrt (-2.0 * log (pfa));
}

double
det_pd (double snr, int dwell, double threshold)
{
  double a = sqrt (2.0 * dwell) * snr;
  return marcum_q (1, a, threshold);
}

int
det_dwell (double snr, double pd_min, double pfa, int max_dwell)
{
  double eta = det_threshold (pfa);
  for (int m = 1; m <= max_dwell; m++)
    {
      if (det_pd (snr, m, eta) >= pd_min)
        return m;
    }
  return -1;
}

double
det_snr (int dwell, double pd_min, double pfa)
{
  double eta = det_threshold (pfa);
  double lo  = 0.0;
  double hi  = 1.0;

  /* Double hi until det_pd meets the target. */
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

/* ── Power detector ────────────────────────────────────────────────────── */

double
det_threshold_power (double pfa)
{
  return -log (pfa);
}

double
det_pd_power (double snr_power, int dwell, double power_threshold)
{
  /*
   * power_stat = |R[0]|² / mean(|R[τ]|²)
   * Under H1: power_stat relates to noncentral chi²(2, 2·dwell·snr_power).
   * Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·power_threshold))
   * Note: sqrt(2·power_threshold) == det_threshold(pfa) when
   * power_threshold == det_threshold_power(pfa), so the Marcum Q argument
   * is identical to the envelope detector at the same Pfa.
   */
  return marcum_q (1,
                   sqrt (2.0 * dwell * snr_power),
                   sqrt (2.0 * power_threshold));
}

int
det_dwell_power (double snr_power, double pd_min, double pfa, int max_dwell)
{
  /* Equivalent to det_dwell(sqrt(snr_power), ...) since the Q_1 arguments
   * reduce to the same values.  Implemented directly to avoid sqrt. */
  double p = det_threshold_power (pfa);
  for (int m = 1; m <= max_dwell; m++)
    {
      if (det_pd_power (snr_power, m, p) >= pd_min)
        return m;
    }
  return -1;
}

double
det_snr_power (int dwell, double pd_min, double pfa)
{
  double s = det_snr (dwell, pd_min, pfa);
  return s * s;
}
