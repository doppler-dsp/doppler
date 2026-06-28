#include "ppe/ppe_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

/* Smallest power of two >= n (>= 1). */
static size_t
next_pow2 (size_t n)
{
  size_t p = 1;
  while (p < n)
    p <<= 1;
  return p;
}

ppe_state_t *
ppe_create (size_t max_len, size_t lag)
{
  if (max_len < 4)
    return NULL;
  ppe_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->max_len = max_len;
  s->nfft    = next_pow2 (max_len);
  s->lag     = lag;
  s->fft     = fft_create (s->nfft, -1, 1); /* forward */
  s->ac      = malloc (max_len * sizeof (float complex));
  s->buf     = malloc (s->nfft * sizeof (float complex));
  s->spec    = malloc (s->nfft * sizeof (float complex));
  s->mag     = malloc (s->nfft * sizeof (float));
  s->win     = malloc (max_len * sizeof (float));
  if (!s->fft || !s->ac || !s->buf || !s->spec || !s->mag || !s->win)
    {
      ppe_destroy (s);
      return NULL;
    }
  return s;
}

void
ppe_destroy (ppe_state_t *s)
{
  if (!s)
    return;
  if (s->fft)
    fft_destroy (s->fft);
  free (s->ac);
  free (s->buf);
  free (s->spec);
  free (s->mag);
  free (s->win);
  free (s);
}

void
ppe_reset (ppe_state_t *s)
{
  (void)s; /* no running state */
}

/* Window @p seq (length @p len) into the zero-padded FFT input, transform, and
 * return the dominant peak's DC-centred normalized frequency in [-0.5, 0.5).
 * Writes a rough peak-to-mean confidence (dB) to @p snr_db. */
static double
spectral_peak (ppe_state_t *s, const float complex *seq, size_t len,
               double *snr_db)
{
  const size_t nfft = s->nfft;
  /* A mild Kaiser taper suppresses sidelobes so the HAF/CFO peak is clean. */
  kaiser_window (s->win, len, (float)kaiser_beta_for_sidelobe (50.0));
  for (size_t i = 0; i < len; i++)
    s->buf[i] = seq[i] * s->win[i];
  for (size_t i = len; i < nfft; i++)
    s->buf[i] = 0.0f;

  fft_execute_cf32 (s->fft, s->buf, nfft, s->spec);
  magnitude_db_cf32 (s->spec, nfft, s->mag, 1e-20f, 0.0f);

  /* find_peaks_f32 expects a DC-centred spectrum (bin i -> (i-N/2)/N); the FFT
   * output is natural order (DC at bin 0), so swap halves in place. */
  const size_t h = nfft / 2;
  for (size_t i = 0; i < h; i++)
    {
      float t       = s->mag[i];
      s->mag[i]     = s->mag[i + h];
      s->mag[i + h] = t;
    }

  dp_peak_t pk;
  double    fn, peak_db;
  if (find_peaks_f32 (s->mag, nfft, 1, -1.0e30f, &pk) >= 1)
    {
      fn      = (double)pk.freq_norm; /* DC-centred + parabola-interpolated */
      peak_db = (double)pk.amplitude_db;
    }
  else
    {
      /* Degenerate spectrum: fall back to a plain argmax (DC-centred). */
      size_t bi = 0;
      for (size_t i = 1; i < nfft; i++)
        if (s->mag[i] > s->mag[bi])
          bi = i;
      fn      = ((double)bi - (double)h) / (double)nfft;
      peak_db = (double)s->mag[bi];
    }

  if (snr_db)
    {
      double sum = 0.0;
      for (size_t i = 0; i < nfft; i++)
        sum += (double)s->mag[i];
      *snr_db = peak_db - sum / (double)nfft;
    }
  return fn;
}

ppe_result_t
ppe_estimate (ppe_state_t *s, const float complex *y, size_t L)
{
  ppe_result_t r = { 0.0, 0.0, 0.0 };
  if (L < 4 || L > s->max_len)
    return r;

  /* Lag k for the instantaneous autocorrelation; k = L/2 maximizes the chirp
   * resolution (the autocorrelation tone frequency ν = r·k grows with k). */
  size_t k = s->lag ? s->lag : L / 2;
  if (k < 1)
    k = 1;
  if (k >= L)
    k = L - 1;
  const size_t Lc = L - k;

  /* 1) Chirp rate: c[m] = y[m+k]·conj(y[m]) is a tone at ν = r·k. */
  for (size_t m = 0; m < Lc; m++)
    s->ac[m] = y[m + k] * conjf (y[m]);
  double snr1 = 0.0;
  double nu   = spectral_peak (s, s->ac, Lc, &snr1);
  double rate = nu / (double)k; /* cycles/sample^2 */

  /* 2) Frequency: dechirp y by r̂ (remove the quadratic π·r·m² phase); the
   * residual is a single tone at f. Reduce the phase mod 2π for cexpf
   * accuracy.
   */
  for (size_t m = 0; m < L; m++)
    {
      double ph = M_PI * rate * (double)m * (double)m;
      ph -= 2.0 * M_PI * round (ph / (2.0 * M_PI));
      s->ac[m] = y[m] * cexpf (-(float)ph * I);
    }
  double snr2 = 0.0;
  double freq = spectral_peak (s, s->ac, L, &snr2);

  r.freq_norm = freq;
  r.rate_norm = rate;
  r.snr_db    = snr2;
  return r;
}
