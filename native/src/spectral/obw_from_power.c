/*
 * obw_from_power.c — spectral module-level function.
 *
 * Occupied bandwidth from a DC-centred linear-power spectrum: the bandwidth of
 * the central interval that holds a given fraction of the total power.  The
 * (1-frac)/2 of power below and above the interval is excluded symmetrically
 * by power, the standard occupied-bandwidth definition.  Works on any
 * non-negative power array; any constant per-bin normalisation cancels in the
 * ratio.
 */
#include "spectral/spectral_core.h"

double
obw_from_power (const double *pwr, size_t pwr_len, double fs, double frac)
{
  if (pwr_len == 0)
    return 0.0;

  double total = 0.0;
  for (size_t i = 0; i < pwr_len; i++)
    total += pwr[i];
  if (total <= 0.0)
    return 0.0;

  const double lower = (1.0 - frac) * 0.5 * total;
  const double upper = (1.0 + frac) * 0.5 * total;

  double cum    = 0.0;
  size_t ilo    = 0;
  size_t ihi    = pwr_len - 1;
  int    got_lo = 0;
  for (size_t i = 0; i < pwr_len; i++)
    {
      cum += pwr[i];
      if (!got_lo && cum >= lower)
        {
          ilo    = i;
          got_lo = 1;
        }
      if (cum >= upper)
        {
          ihi = i;
          break;
        }
    }

  /* Inclusive bin span times the bin width (fs / pwr_len). */
  return ((double)(ihi - ilo) + 1.0) * fs / (double)pwr_len;
}
