/*
 * noise_floor_db.c — spectral module-level function.
 *
 * Median of a dB spectrum, used as a robust noise-floor estimate: tones and
 * spurs sit well above the median, so it tracks the noise pedestal rather than
 * the signal.  The median is taken on a sorted copy; the input is not
 * modified.
 */
#include "spectral/spectral_core.h"

static int
cmp_float (const void *a, const void *b)
{
  const float fa = *(const float *)a;
  const float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

double
noise_floor_db (const float *db, size_t db_len)
{
  if (db_len == 0)
    return 0.0;

  float *tmp = (float *)malloc (db_len * sizeof (float));
  if (!tmp)
    return 0.0;
  memcpy (tmp, db, db_len * sizeof (float));
  qsort (tmp, db_len, sizeof (float), cmp_float);

  double median = (double)tmp[db_len / 2];
  if ((db_len & 1u) == 0u)
    median = 0.5 * (median + (double)tmp[db_len / 2 - 1]);

  free (tmp);
  return median;
}
