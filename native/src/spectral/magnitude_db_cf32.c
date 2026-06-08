/*
 * magnitude_db_cf32.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <complex.h>
#include <math.h>

void
magnitude_db_cf32 (const float complex *x, size_t x_len, float *out,
                   float lin_floor, float offset_db)
{
  for (size_t k = 0; k < x_len; k++)
    {
      float mag = cabsf (x[k]);
      if (mag < lin_floor)
        mag = lin_floor;
      out[k] = 20.0f * log10f (mag) + offset_db;
    }
}
