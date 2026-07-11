/*
 * snr_data_aided_db.c — snr module-level function.
 *
 * Clamps to the common length when soft/sign_bits differ (a DLL boundary
 * slip can leave them a symbol or two apart -- see BurstDespreader's
 * docs). snr_data_aided_db_series.c calls this directly on slices, so the
 * algorithm lives exactly once (C-first) without a separate kernel file
 * jm's CMakeLists reconciliation would otherwise drop on regeneration.
 */
#include "snr/snr_core.h"

#include <math.h>

double
snr_data_aided_db (const float complex *soft, size_t soft_len,
                   const uint8_t *sign_bits, size_t sign_bits_len)
{
  size_t n = (soft_len < sign_bits_len) ? soft_len : sign_bits_len;
  if (n == 0)
    return NAN;
  double sum_re = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      float s = (sign_bits[i] & 1u) ? -1.0f : 1.0f;
      sum_re += (double)(crealf (soft[i]) * s);
    }
  double a         = sum_re / (double)n;
  double noise_sum = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      float  s   = (sign_bits[i] & 1u) ? -1.0f : 1.0f;
      double dre = (double)(crealf (soft[i]) * s) - a;
      double dim = (double)(cimagf (soft[i]) * s);
      noise_sum += dre * dre + dim * dim;
    }
  double noise_pow = noise_sum / (double)n;
  return (noise_pow > 0.0) ? 10.0 * log10 ((a * a) / noise_pow) : NAN;
}
