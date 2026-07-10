/*
 * snr_data_aided_db_series.c — snr module-level function.
 *
 * Sliding-window wrapper: one snr_data_aided_db() call per output index,
 * over a [i - window/2, i + window/2] span clamped at the edges.
 */
#include "snr/snr_core.h"

#include <math.h>

void
snr_data_aided_db_series (const float complex *soft, size_t soft_len,
                          const uint8_t *sign_bits, size_t sign_bits_len,
                          size_t window, double *out)
{
  size_t n    = (soft_len < sign_bits_len) ? soft_len : sign_bits_len;
  size_t half = window / 2;
  for (size_t i = 0; i < soft_len; i++)
    {
      if (i >= n)
        {
          out[i] = NAN; /* no known sign at this index */
          continue;
        }
      size_t lo = (i > half) ? i - half : 0;
      size_t hi = (i + half + 1 < n) ? i + half + 1 : n;
      out[i] = snr_data_aided_db (soft + lo, hi - lo, sign_bits + lo, hi - lo);
    }
}
