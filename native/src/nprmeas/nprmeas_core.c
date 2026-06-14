/*
 * nprmeas_core.c — notched-noise Noise Power Ratio.
 *
 * Builds the one-sided cg^2-normalised power spectrum (same convention as
 * tonemeas), then averages the per-bin noise power in the active band and
 * inside the notch; NPR is their ratio.  A `guard` keep-out around the notch
 * edges avoids contaminating either average with the notch skirts.
 */
#include "nprmeas/nprmeas_core.h"

#include "spectral/spectral_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>

#define NPR_EPS 1e-30

static size_t
next_pow2 (size_t x)
{
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

nprmeas_state_t *
nprmeas_create (size_t n, double fs, int window, float beta, size_t pad,
                double full_scale)
{
  if (n < 2 || fs <= 0.0 || (window != 0 && window != 1) || full_scale <= 0.0)
    return NULL;
  if (pad < 1)
    pad = 1;
  nprmeas_state_t *s = (nprmeas_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->n          = n;
  s->fs         = fs;
  s->full_scale = full_scale;
  s->nfft       = next_pow2 (n * pad);
  s->w          = (float *)malloc (n * sizeof (float));
  s->frame      = (float complex *)malloc (s->nfft * sizeof (float complex));
  s->spec       = (float complex *)malloc (s->nfft * sizeof (float complex));
  s->pwr        = (float *)malloc (s->nfft * sizeof (float));
  s->fft        = fft_create (s->nfft, -1, 1);
  if (!s->w || !s->frame || !s->spec || !s->pwr || !s->fft)
    {
      nprmeas_destroy (s);
      return NULL;
    }
  if (window == 1)
    kaiser_window (s->w, n, beta);
  else
    hann_window (s->w, n);
  double cg = 0.0, s2 = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      cg += (double)s->w[i];
      s2 += (double)s->w[i] * (double)s->w[i];
    }
  s->cg   = cg;
  s->s2   = s2;
  s->enbw = (double)n * s2 / (cg * cg);
  return s;
}

void
nprmeas_destroy (nprmeas_state_t *state)
{
  if (!state)
    return;
  if (state->fft)
    fft_destroy (state->fft);
  free (state->w);
  free (state->frame);
  free (state->spec);
  free (state->pwr);
  free (state);
}

void
nprmeas_reset (nprmeas_state_t *state)
{
  (void)state;
}

npr_meas_t
nprmeas_analyze (nprmeas_state_t *s, const float *x, size_t n_in,
                 double active_lo, double active_hi, double notch_lo,
                 double notch_hi, double guard_hz)
{
  npr_meas_t r;
  /* window + zero-pad + FFT -> one-sided cg^2-normalised power */
  size_t nuse = (n_in < s->n) ? n_in : s->n;
  for (size_t i = 0; i < s->nfft; i++)
    s->frame[i] = (i < nuse) ? (float complex) (s->w[i] * x[i]) : 0.0f;
  fft_execute_cf32 (s->fft, s->frame, s->nfft, s->spec);
  size_t half    = s->nfft / 2;
  double inv_cg2 = 1.0 / (s->cg * s->cg);
  for (size_t k = 0; k <= half; k++)
    {
      double re = crealf (s->spec[k]);
      double im = cimagf (s->spec[k]);
      double p  = (re * re + im * im) * inv_cg2;
      s->pwr[k] = (float)((k == 0 || k == half) ? p : 2.0 * p);
    }

  double df    = s->fs / (double)s->nfft;
  double in_lo = active_lo, in_hi = active_hi;
  double nt_lo = notch_lo - guard_hz, nt_hi = notch_hi + guard_hz;
  double ni_lo = notch_lo + guard_hz, ni_hi = notch_hi - guard_hz;
  double in_sum = 0.0, nt_sum = 0.0;
  size_t in_cnt = 0, nt_cnt = 0;
  for (size_t k = 0; k <= half; k++)
    {
      double f = (double)k * df;
      if (f >= ni_lo && f <= ni_hi) /* notch interior */
        {
          nt_sum += (double)s->pwr[k];
          nt_cnt++;
        }
      else if (f >= in_lo && f <= in_hi && (f < nt_lo || f > nt_hi))
        {
          in_sum += (double)s->pwr[k]; /* active band, outside notch+guard */
          in_cnt++;
        }
    }
  double in_mean = in_sum / (double)(in_cnt ? in_cnt : 1);
  double nt_mean = nt_sum / (double)(nt_cnt ? nt_cnt : 1);
  if (in_mean < NPR_EPS)
    in_mean = NPR_EPS;
  if (nt_mean < NPR_EPS)
    nt_mean = NPR_EPS;

  /* per-bin power referenced to a full-scale tone (same cal as tonemeas) */
  double cal = (double)s->nfft / (double)s->n * s->enbw;
  double ref = s->full_scale * s->full_scale / 2.0 * cal;

  r.npr_db          = 10.0 * log10 (in_mean / nt_mean);
  r.inband_psd_dbfs = 10.0 * log10 (in_mean / ref);
  r.notch_psd_dbfs  = 10.0 * log10 (nt_mean / ref);
  r.n_inband_bins   = in_cnt;
  r.n_notch_bins    = nt_cnt;
  r.rbw_hz          = s->enbw * s->fs / (double)s->n;
  return r;
}
