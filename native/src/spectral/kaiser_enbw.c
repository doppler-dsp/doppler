/*
 * kaiser_enbw.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"

float
kaiser_enbw (const float *w, size_t w_len)
{
  double sum_sq = 0.0, sum = 0.0;
  for (size_t k = 0; k < w_len; k++)
    {
      double wk = (double)w[k];
      sum += wk;
      sum_sq += wk * wk;
    }
  return (float)((double)w_len * sum_sq / (sum * sum));
}
