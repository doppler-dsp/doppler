#include "ppe/ppe_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

/* Rate-grid oversampling (vs the 1/L^2 resolution) and a hard bin cap. */
#define PPE_OVERSAMPLE 2.0
#define PPE_MAX_RATE_BINS 8192

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
ppe_create (size_t max_len, double max_rate)
{
  if (max_len < 4 || max_rate < 0.0)
    return NULL;
  ppe_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->max_len = max_len;
  /* 4x zero-pad: finer frequency grid + accurate parabolic peak interpolation
   * (the input is often short — preamble partials / symbol streams). */
  s->nfft     = next_pow2 (max_len) << 2;
  s->max_rate = max_rate;

  /* Chirp-rate grid: resolution ~ 1/L^2 (a rate error r smears the dechirped
   * tone once r*L^2 ~ 1); oversample, cap, and force an odd count so r = 0 is
   * a grid node (and max_rate = 0 collapses to a single FFT). */
  if (max_rate <= 0.0)
    {
      s->n_rate = 1;
      s->drate  = 0.0;
    }
  else
    {
      double dr = 1.0 / (PPE_OVERSAMPLE * (double)max_len * (double)max_len);
      size_t nr = (size_t)(2.0 * max_rate / dr) + 1;
      if (nr < 3)
        nr = 3;
      if (nr > PPE_MAX_RATE_BINS)
        nr = PPE_MAX_RATE_BINS;
      if ((nr & 1u) == 0u)
        nr++;
      s->n_rate = nr;
      s->drate  = 2.0 * max_rate / (double)(nr - 1);
    }

  s->fft    = fft_create (s->nfft, -1, 1); /* forward */
  s->buf    = malloc (s->nfft * sizeof (float complex));
  s->spec   = malloc (s->nfft * sizeof (float complex));
  s->mag    = malloc (s->nfft * sizeof (float));
  s->win    = malloc (max_len * sizeof (float));
  s->rowpk  = malloc (s->n_rate * sizeof (double));
  s->rowfrq = malloc (s->n_rate * sizeof (double));
  if (!s->fft || !s->buf || !s->spec || !s->mag || !s->win || !s->rowpk
      || !s->rowfrq)
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
  free (s->buf);
  free (s->spec);
  free (s->mag);
  free (s->win);
  free (s->rowpk);
  free (s->rowfrq);
  free (s);
}

void
ppe_reset (ppe_state_t *s)
{
  (void)s; /* no running state */
}

/* Dechirp @p y (length @p len) by rate @p r using the pre-computed window in
 * s->win, FFT, and return the dominant peak's DC-centred normalized frequency
 * (@p freq) and its peak-to-mean prominence in dB (@p ptm). */
static void
row_peak (ppe_state_t *s, const float complex *y, size_t len, double r,
          double *freq, double *peak_out, double *ptm)
{
  const size_t nfft = s->nfft;
  for (size_t m = 0; m < len; m++)
    {
      double ph = M_PI * r * (double)m * (double)m; /* quadratic phase */
      ph -= 2.0 * M_PI * round (ph / (2.0 * M_PI)); /* wrap for cexpf */
      s->buf[m] = y[m] * s->win[m] * cexpf (-(float)ph * I);
    }
  for (size_t m = len; m < nfft; m++)
    s->buf[m] = 0.0f;

  fft_execute_cf32 (s->fft, s->buf, nfft, s->spec);
  magnitude_db_cf32 (s->spec, nfft, s->mag, 1e-20f, 0.0f);

  /* find_peaks_f32 expects a DC-centred spectrum; swap halves in place. */
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
      fn      = (double)pk.freq_norm;
      peak_db = (double)pk.amplitude_db;
    }
  else
    {
      size_t bi = 0;
      for (size_t i = 1; i < nfft; i++)
        if (s->mag[i] > s->mag[bi])
          bi = i;
      fn      = ((double)bi - (double)h) / (double)nfft;
      peak_db = (double)s->mag[bi];
    }

  double sum = 0.0;
  for (size_t i = 0; i < nfft; i++)
    sum += (double)s->mag[i];
  *freq     = fn;
  *peak_out = peak_db;                      /* coherent gain: max at true r */
  *ptm      = peak_db - sum / (double)nfft; /* prominence (confidence)      */
}

ppe_result_t
ppe_estimate (ppe_state_t *s, const float complex *y, size_t L)
{
  ppe_result_t r = { 0.0, 0.0, 0.0 };
  if (L < 4 || L > s->max_len)
    return r;

  /* The taper is identical for every rate row — compute it once. */
  kaiser_window (s->win, L, (float)kaiser_beta_for_sidelobe (50.0));

  /* Scan the chirp-rate hypotheses; the dechirp that concentrates the spectrum
   * best (max peak-to-mean) is the true rate. */
  size_t best      = 0;
  double best_peak = -1.0e300, best_ptm = 0.0;
  for (size_t i = 0; i < s->n_rate; i++)
    {
      double ri
          = (s->n_rate == 1) ? 0.0 : (-s->max_rate + (double)i * s->drate);
      double fn, peak, ptm;
      row_peak (s, y, L, ri, &fn, &peak, &ptm);
      s->rowfrq[i] = fn;
      s->rowpk[i]  = peak; /* coherent peak height — matched-filter metric */
      if (peak > best_peak)
        {
          best_peak = peak;
          best_ptm  = ptm;
          best      = i;
        }
    }

  /* Sub-grid chirp rate: parabolic interpolation on peak-to-mean vs rate. */
  double frac = 0.0;
  if (best > 0 && best + 1 < s->n_rate)
    {
      double ym1 = s->rowpk[best - 1], y0 = s->rowpk[best],
             yp1 = s->rowpk[best + 1];
      double den = ym1 - 2.0 * y0 + yp1;
      if (den != 0.0)
        frac = 0.5 * (ym1 - yp1) / den;
      if (frac > 0.5)
        frac = 0.5;
      if (frac < -0.5)
        frac = -0.5;
    }

  r.rate_norm = (s->n_rate == 1)
                    ? 0.0
                    : (-s->max_rate + ((double)best + frac) * s->drate);
  r.freq_norm = s->rowfrq[best];
  r.snr_db    = best_ptm;
  return r;
}
