/*
 * magnitude_db_cf64.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <complex.h>
#include <math.h>

void
magnitude_db_cf64 (const double complex *x, size_t x_len, float *out,
                   double lin_floor, float offset_db)
{
  for (size_t k = 0; k < x_len; k++)
    {
      double mag = cabs (x[k]);
      if (mag < lin_floor)
        mag = lin_floor;
      out[k] = (float)(20.0 * log10 (mag)) + offset_db;
    }
}
