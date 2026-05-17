/*
 * spectral_core.c — Kaiser/Hann windowing, magnitude conversion, peak finding.
 *
 * All functions are pure (no persistent state).  Reference:
 *   doppler/native/src/spectral/spectral_core.c
 */
#include "spectral/spectral_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* I0(x): zeroth-order modified Bessel function of the first kind.
 * Power series converges for all practical beta values (|x| < ~30). */
static double
_i0 (double x)
{
  double xh = x * 0.5;
  double sum = 1.0, term = 1.0;
  for (int k = 1; k <= 40; k++)
    {
      double r = xh / k;
      term *= r * r;
      sum += term;
      if (term < 1e-15 * sum)
        break;
    }
  return sum;
}

float
kaiser_enbw (const float *w, size_t w_len)
{
  double sum_sq = 0.0, sum = 0.0;
  for (size_t k = 0; k < w_len; k++)
    {
      double wk = (double)w[k];
      sum += wk;
      sum_sq += wk * wk;
    }
  return (float)((double)w_len * sum_sq / (sum * sum));
}

void
kaiser_window (float *w, size_t w_len, float beta)
{
  if (w_len == 1)
    {
      w[0] = 1.0f;
      return;
    }
  double b = (double)beta;
  double i0b = _i0 (b);
  double half = (double)(w_len - 1) * 0.5;
  for (size_t k = 0; k < w_len; k++)
    {
      double x = ((double)k - half) / half;
      double arg = b * sqrt (1.0 - x * x);
      w[k] = (float)(_i0 (arg) / i0b);
    }
}

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

void
magnitude_db_cf32 (const float complex *in, size_t n, float *out,
                   float lin_floor, float offset_db)
{
  for (size_t k = 0; k < n; k++)
    {
      float mag = cabsf (in[k]);
      if (mag < lin_floor)
        mag = lin_floor;
      out[k] = 20.0f * log10f (mag) + offset_db;
    }
}

void
magnitude_db_cf64 (const double complex *in, size_t n, float *out,
                   double lin_floor, float offset_db)
{
  for (size_t k = 0; k < n; k++)
    {
      double mag = cabs (in[k]);
      if (mag < lin_floor)
        mag = lin_floor;
      out[k] = (float)(20.0 * log10 (mag)) + offset_db;
    }
}

static int
_peak_cmp_desc (const void *a, const void *b)
{
  float da = ((const dp_peak_t *)a)->amplitude_db;
  float db = ((const dp_peak_t *)b)->amplitude_db;
  return (da < db) - (da > db);
}

size_t
find_peaks_f32 (const float *db, size_t n, size_t n_peaks, float min_db,
                dp_peak_t *out)
{
  if (n < 3 || n_peaks == 0)
    return 0;

  size_t max_cands = n / 2 + 1;
  dp_peak_t *cands = malloc (max_cands * sizeof (dp_peak_t));
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
      float beta = db[k];
      float gamma = db[k + 1];
      float denom = alpha - 2.0f * beta + gamma;
      float delta = (denom != 0.0f) ? 0.5f * (alpha - gamma) / denom : 0.0f;
      if (delta < -0.5f)
        delta = -0.5f;
      if (delta > 0.5f)
        delta = 0.5f;

      float freq_norm = ((float)k - (float)(size_t)(n / 2) + delta) / (float)n;
      float amp = beta - 0.25f * (alpha - gamma) * delta;

      if (nc < max_cands)
        {
          cands[nc].freq_norm = freq_norm;
          cands[nc].amplitude_db = amp;
          nc++;
        }
    }

  qsort (cands, nc, sizeof (dp_peak_t), _peak_cmp_desc);

  size_t nout = (nc < n_peaks) ? nc : n_peaks;
  memcpy (out, cands, nout * sizeof (dp_peak_t));
  free (cands);
  return nout;
}
