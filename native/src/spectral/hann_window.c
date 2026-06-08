/*
 * hann_window.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <math.h>

void
hann_window (float *w, size_t w_len)
{
  if (w_len == 1)
    {
      w[0] = 1.0f;
      return;
    }
  double scale = 2.0 * M_PI / (double)(w_len - 1);
  for (size_t k = 0; k < w_len; k++)
    w[k] = 0.5f * (1.0f - (float)cos (scale * (double)k));
}
