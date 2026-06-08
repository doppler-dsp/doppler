/*
 * find_peaks_f32.c — spectral module-level function.
 */
#include "spectral/spectral_core.h"
#include <stdlib.h>
#include <string.h>

static int
_peak_cmp_desc (const void *a, const void *b)
{
  float da = ((const dp_peak_t *)a)->amplitude_db;
  float db = ((const dp_peak_t *)b)->amplitude_db;
  return (da < db) - (da > db);
}

size_t
find_peaks_f32 (const float *db, size_t db_len, size_t n_peaks, float min_db,
                dp_peak_t *result)
{
  size_t n = db_len;
  if (n < 3 || n_peaks == 0)
    return 0;

  size_t     max_cands = n / 2 + 1;
  dp_peak_t *cands     = malloc (max_cands * sizeof (dp_peak_t));
  if (!cands)
    return 0;

  size_t nc = 0;
  for (size_t k = 1; k < n - 1; k++)
    {
      if (db[k] <= min_db)
        continue;
      if (!(db[k] > db[k - 1] && db[k] >= db[k + 1]))
        continue;

      float alpha = db[k - 1];
      float beta  = db[k];
      float gamma = db[k + 1];
      float denom = alpha - 2.0f * beta + gamma;
      float delta = (denom != 0.0f) ? 0.5f * (alpha - gamma) / denom : 0.0f;
      if (delta < -0.5f)
        delta = -0.5f;
      if (delta > 0.5f)
        delta = 0.5f;

      float freq_norm = ((float)k - (float)(size_t)(n / 2) + delta) / (float)n;
      float amp       = beta - 0.25f * (alpha - gamma) * delta;

      if (nc < max_cands)
        {
          cands[nc].freq_norm    = freq_norm;
          cands[nc].amplitude_db = amp;
          nc++;
        }
    }

  qsort (cands, nc, sizeof (dp_peak_t), _peak_cmp_desc);

  size_t nout = (nc < n_peaks) ? nc : n_peaks;
  memcpy (result, cands, nout * sizeof (dp_peak_t));
  free (cands);
  return nout;
}
