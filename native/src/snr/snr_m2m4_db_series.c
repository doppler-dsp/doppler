/*
 * snr_m2m4_db_series.c — snr module-level function.
 *
 * Sliding-window wrapper: one snr_m2m4_db() call per output index, over a
 * [i - window/2, i + window/2] span clamped at the edges.
 */
#include "snr/snr_core.h"

void
snr_m2m4_db_series (const float complex *x, size_t x_len, size_t window,
                    double *out)
{
  size_t half = window / 2;
  for (size_t i = 0; i < x_len; i++)
    {
      size_t lo = (i > half) ? i - half : 0;
      size_t hi = (i + half + 1 < x_len) ? i + half + 1 : x_len;
      out[i]    = snr_m2m4_db (x + lo, hi - lo);
    }
}
