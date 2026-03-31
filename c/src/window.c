/**
 * @file window.c
 * @brief Kaiser window and ENBW computation.
 */

#include "dp/window.h"

#include <math.h>

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

/**
 * Zeroth-order modified Bessel function of the first kind, I0(x).
 *
 * Computed via the power series:
 *
 *   I0(x) = sum_{k=0}^{inf} ((x/2)^k / k!)^2
 *
 * Converges quickly for |x| < 30 (all practical beta values).
 * The loop exits when the relative term is below double precision.
 */
static double
_i0 (double x)
{
  double xh = x * 0.5;
  double sum = 1.0;
  double term = 1.0;
  for (int k = 1; k <= 40; k++)
    {
      double r = xh / k;
      term *= r * r;
      sum += term;
      if (term < 1e-15 * sum)
        break;
    }
  return sum;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

void
dp_kaiser_window (float *w, size_t n, float beta)
{
  if (n == 1)
    {
      w[0] = 1.0f;
      return;
    }

  double b = (double)beta;
  double i0b = _i0 (b);
  double half = (double)(n - 1) * 0.5;

  for (size_t k = 0; k < n; k++)
    {
      double x = ((double)k - half) / half; /* normalised to [-1, 1] */
      double arg = b * sqrt (1.0 - x * x);
      w[k] = (float)(_i0 (arg) / i0b);
    }
}

float
dp_kaiser_enbw (const float *w, size_t n)
{
  double sum_sq = 0.0;
  double sum = 0.0;

  for (size_t k = 0; k < n; k++)
    {
      double wk = (double)w[k];
      sum += wk;
      sum_sq += wk * wk;
    }

  /* ENBW in bins: N * sum(w^2) / sum(w)^2 */
  return (float)((double)n * sum_sq / (sum * sum));
}
