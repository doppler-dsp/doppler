/*
 * imdmeas_core.c — two-tone intermodulation (IMD2/IMD3) and intercept.
 *
 * Same one-sided cg^2-normalised spectrum convention as tonemeas.  Finds the
 * two strongest lobes (the fundamentals), integrates them and the folded IM
 * products over their main lobes, and forms the third/second-order intercepts.
 */
#include "imdmeas/imdmeas_core.h"

#include "spectral/spectral_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define IMD_EPS 1e-30

static size_t
next_pow2 (size_t x)
{
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

/* Reflect a frequency into [0, fs/2] (real one-sided band). */
static double
fold_real (double f, double fs)
{
  double g = fmod (fabs (f), fs);
  return (g <= fs / 2.0) ? g : (fs - g);
}

imdmeas_state_t *
imdmeas_create (size_t n, double fs, int window, float beta, size_t pad,
                double full_scale)
{
  if (n < 2 || fs <= 0.0 || (window != 0 && window != 1) || full_scale <= 0.0)
    return NULL;
  if (pad < 1)
    pad = 1;
  imdmeas_state_t *s = (imdmeas_state_t *)calloc (1, sizeof (*s));
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
      imdmeas_destroy (s);
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
  s->cg        = cg;
  s->s2        = s2;
  s->enbw      = (double)n * s2 / (cg * cg);
  double p_eff = (double)s->nfft / (double)n;
  double lobe_un
      = (window == 1) ? sqrt (1.0 + (beta / M_PI) * (beta / M_PI)) : 2.0;
  s->lobe_bins = (size_t)ceil (lobe_un * p_eff) + 1;
  return s;
}

void
imdmeas_destroy (imdmeas_state_t *state)
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
imdmeas_reset (imdmeas_state_t *state)
{
  (void)state;
}

/* Integrate pwr over the lobe centred on frequency f (folded into band). */
static double
lobe_at (const imdmeas_state_t *s, size_t nbins, double f, double df,
         long *bin_out)
{
  long L = (long)s->lobe_bins;
  long c = (long)lround (fold_real (f, s->fs) / df);
  if (bin_out)
    *bin_out = c;
  long lo = c - L, hi = c + L;
  if (lo < 0)
    lo = 0;
  if (hi > (long)nbins - 1)
    hi = (long)nbins - 1;
  double p = 0.0;
  for (long k = lo; k <= hi; k++)
    p += (double)s->pwr[k];
  return p;
}

imd_meas_t
imdmeas_analyze (imdmeas_state_t *s, const float *x, size_t n_in)
{
  imd_meas_t r;
  memset (&r, 0, sizeof (r));
  size_t nuse = (n_in < s->n) ? n_in : s->n;
  for (size_t i = 0; i < s->nfft; i++)
    s->frame[i] = (i < nuse) ? (float complex) (s->w[i] * x[i]) : 0.0f;
  fft_execute_cf32 (s->fft, s->frame, s->nfft, s->spec);
  size_t half    = s->nfft / 2;
  size_t nbins   = half + 1;
  double inv_cg2 = 1.0 / (s->cg * s->cg);
  for (size_t k = 0; k <= half; k++)
    {
      double re = crealf (s->spec[k]);
      double im = cimagf (s->spec[k]);
      double p  = (re * re + im * im) * inv_cg2;
      s->pwr[k] = (float)((k == 0 || k == half) ? p : 2.0 * p);
    }
  double df = s->fs / (double)s->nfft;
  long   L  = (long)s->lobe_bins;

  /* find the two strongest lobes (the fundamentals) */
  long   ka = -1;
  double pa = -1.0;
  for (long k = L + 1; k < (long)nbins; k++)
    if ((double)s->pwr[k] > pa)
      {
        pa = (double)s->pwr[k];
        ka = k;
      }
  long   kb = -1;
  double pb = -1.0;
  for (long k = L + 1; k < (long)nbins; k++)
    {
      if (labs (k - ka) <= 2 * L)
        continue;
      if ((double)s->pwr[k] > pb)
        {
          pb = (double)s->pwr[k];
          kb = k;
        }
    }
  if (ka < 0 || kb < 0)
    return r; /* degenerate (zeroed) */
  long   k1 = ka < kb ? ka : kb;
  long   k2 = ka < kb ? kb : ka;
  double f1 = (double)k1 * df, f2 = (double)k2 * df;

  double P1 = lobe_at (s, nbins, f1, df, NULL);
  double P2 = lobe_at (s, nbins, f2, df, NULL);
  double Pf = 0.5 * (P1 + P2);
  if (Pf < IMD_EPS)
    Pf = IMD_EPS;

  long   b;
  double P_im3lo = lobe_at (s, nbins, 2 * f1 - f2, df, &b);
  r.imd3_lo_freq = fold_real (2 * f1 - f2, s->fs);
  double P_im3hi = lobe_at (s, nbins, 2 * f2 - f1, df, &b);
  r.imd3_hi_freq = fold_real (2 * f2 - f1, s->fs);
  double P_im2   = lobe_at (s, nbins, f2 - f1, df, &b);
  r.imd2_freq    = fold_real (f2 - f1, s->fs);

  double P_im3 = P_im3lo > P_im3hi ? P_im3lo : P_im3hi;
  if (P_im3 < IMD_EPS)
    P_im3 = IMD_EPS;
  if (P_im2 < IMD_EPS)
    P_im2 = IMD_EPS;

  double cal = (double)s->nfft / (double)s->n * s->enbw;
  double ref = s->full_scale * s->full_scale / 2.0 * cal;

  r.f1           = f1;
  r.f2           = f2;
  r.p1_dbfs      = 10.0 * log10 (P1 / ref);
  r.p2_dbfs      = 10.0 * log10 (P2 / ref);
  r.imd2_dbc     = 10.0 * log10 (P_im2 / Pf);
  r.imd3_dbc     = 10.0 * log10 (P_im3 / Pf);
  double pf_dbfs = 10.0 * log10 (Pf / ref);
  r.toi_dbfs     = pf_dbfs + fabs (r.imd3_dbc) / 2.0;
  r.soi_dbfs     = pf_dbfs + fabs (r.imd2_dbc);
  r.rbw_hz       = s->enbw * s->fs / (double)s->n;
  return r;
}
