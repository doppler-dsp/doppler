/*
 * blackman_harris_window.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <math.h>

/* 4-term minimum Blackman-Harris coefficients (Harris 1978). */
#define BH_A0 0.35875
#define BH_A1 0.48829
#define BH_A2 0.14128
#define BH_A3 0.01168

void
blackman_harris_window (float *w, size_t w_len)
{
  if (w_len == 1)
    {
      w[0] = 1.0f;
      return;
    }
  double scale = 2.0 * M_PI / (double)(w_len - 1);
  for (size_t k = 0; k < w_len; k++)
    {
      double x = scale * (double)k;
      w[k] = (float)(BH_A0
                     - BH_A1 * cos (x)
                     + BH_A2 * cos (2.0 * x)
                     - BH_A3 * cos (3.0 * x));
    }
}
