/*
 * kaiser_window.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <math.h>

static double
_i0 (double x)
{
  double xh = x * 0.5;
  double sum = 1.0, term = 1.0;
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

void
kaiser_window(float *w, size_t w_len, float beta)
{
  if (w_len == 1)
    {
      w[0] = 1.0f;
      return;
    }
  double b = (double)beta;
  double i0b = _i0 (b);
  double half = (double)(w_len - 1) * 0.5;
  for (size_t k = 0; k < w_len; k++)
    {
      double x = ((double)k - half) / half;
      double arg = b * sqrt (1.0 - x * x);
      w[k] = (float)(_i0 (arg) / i0b);
    }
}
