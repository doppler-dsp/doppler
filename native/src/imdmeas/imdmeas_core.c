/*
 * imdmeas_core.c — two-tone intermodulation (IMD2/IMD3) and intercept.
 *
 * Same one-sided cg^2-normalised spectrum convention as tonemeas.  Finds the
 * two strongest lobes (the fundamentals), integrates them and the folded IM
 * products over their main lobes, and forms the third/second-order intercepts.
 */
#include "imdmeas/imdmeas_core.h"

#include "spectral/spectral_core.h" /* kaiser_beta_for_sidelobe */

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define IMD_EPS 1e-30

/* Reflect a frequency into [0, fs/2] (real one-sided band). */
static double
fold_real (double f, double fs)
{
  double g = fmod (fabs (f), fs);
  return (g <= fs / 2.0) ? g : (fs - g);
}

imdmeas_state_t *
imdmeas_create (size_t n, double fs, double full_scale, size_t bits,
                double dynamic_range_db)
{
  if (n < 2 || fs <= 0.0)
    return NULL;
  imdmeas_state_t *s = (imdmeas_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  /* Auto-window: minimum Kaiser beta meeting the dynamic-range target. */
  double dr   = measure_resolve_dr (dynamic_range_db, bits);
  double beta = kaiser_beta_for_sidelobe (dr);

  /* The PSD core owns the dBFS reference (full_scale / bits); read it back as
   * s->psd->full_scale so it is defined exactly once. */
  s->psd = psd_create (n, fs, 1 /* Kaiser */, (float)beta, MEASURE_PAD,
                       full_scale, bits, ACC_TRACE_MEAN, 0.0);
  if (!s->psd)
    {
      imdmeas_destroy (s);
      return NULL;
    }
  s->n    = n;
  s->nfft = s->psd->nfft;
  s->fs   = fs;
  s->enbw = s->psd->enbw;
  s->beta = beta;
  s->pwr  = (float *)malloc (s->nfft * sizeof (float));
  if (!s->pwr)
    {
      imdmeas_destroy (s);
      return NULL;
    }
  /* L: main-lobe half-width for power integration; spur_guard_bins (>= L) is
   * the wider tone keep-out so a tone's sidelobe is never taken for the other
   * tone (or for an IM product near it). */
  double p_eff   = (double)s->nfft / (double)n;
  double lobe_un = sqrt (1.0 + (beta / M_PI) * (beta / M_PI));
  s->lobe_bins   = (size_t)ceil (lobe_un * p_eff) + 1;
  s->spur_guard_bins
      = s->lobe_bins + (size_t)ceil (MEASURE_SPUR_SIDELOBES * lobe_un * p_eff);
  return s;
}

void
imdmeas_destroy (imdmeas_state_t *state)
{
  if (!state)
    return;
  if (state->psd)
    psd_destroy (state->psd);
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
  psd_reset (s->psd);
  psd_accumulate_real (s->psd, x, n_in);
  size_t nbins = psd_power_onesided (s->psd, s->nfft / 2 + 1, s->pwr);
  if (nbins == 0)
    return r; /* capture holds no full frame */
  double df = s->fs / (double)s->nfft;
  long   G  = (long)s->spur_guard_bins;

  /* find the two strongest lobes (the fundamentals).  Skip the first G bins so
   * a tone's near-DC sidelobe is not picked, and keep the two tones at least G
   * apart so one tone's sidelobe is never mistaken for the other. */
  long   ka = -1;
  double pa = -1.0;
  for (long k = G + 1; k < (long)nbins; k++)
    if ((double)s->pwr[k] > pa)
      {
        pa = (double)s->pwr[k];
        ka = k;
      }
  long   kb = -1;
  double pb = -1.0;
  for (long k = G + 1; k < (long)nbins; k++)
    {
      if (labs (k - ka) <= G)
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
  double ref = s->psd->full_scale * s->psd->full_scale / 2.0 * cal;

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

size_t
imdmeas_spectrum_dbfs_max_out (imdmeas_state_t *state)
{
  return state->nfft;
}

size_t
imdmeas_spectrum_dbfs (imdmeas_state_t *state, const float *x, size_t x_len,
                       float *out)
{
  /* DC-centred two-sided dBFS view of the capture (analyzer display): the same
   * averaged PSD the metrics use, scaled to the shared 0-dBFS reference. */
  psd_reset (state->psd);
  psd_accumulate_real (state->psd, x, x_len);
  size_t nfft = psd_power_twosided (state->psd, state->nfft, state->pwr);
  if (nfft == 0)
    return 0;
  double ref = state->psd->full_scale * state->psd->full_scale;
  for (size_t i = 0; i < nfft; i++)
    out[i] = (float)(10.0 * log10 ((double)state->pwr[i] / ref + IMD_EPS));
  return nfft;
}
