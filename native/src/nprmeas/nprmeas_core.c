/*
 * nprmeas_core.c — notched-noise Noise Power Ratio.
 *
 * Builds the one-sided cg^2-normalised power spectrum (same convention as
 * tonemeas), then averages the per-bin noise power in the active band and
 * inside the notch; NPR is their ratio.  A `guard` keep-out around the notch
 * edges avoids contaminating either average with the notch skirts.
 */
#include "nprmeas/nprmeas_core.h"

#include "spectral/spectral_core.h" /* kaiser_beta_for_sidelobe */

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NPR_EPS 1e-30

nprmeas_state_t *
nprmeas_create (size_t n, double fs, double full_scale, size_t bits,
                double dynamic_range_db)
{
  if (n < 2 || fs <= 0.0)
    return NULL;
  nprmeas_state_t *s = (nprmeas_state_t *)calloc (1, sizeof (*s));
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
      nprmeas_destroy (s);
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
      nprmeas_destroy (s);
      return NULL;
    }
  /* Minimum notch keep-out (bins): the window main lobe + one sidelobe width,
   * so active-band noise cannot fold into the notch average through the skirt
   * even when the caller passes guard_hz = 0. */
  double p_eff   = (double)s->nfft / (double)n;
  double lobe_un = sqrt (1.0 + (beta / M_PI) * (beta / M_PI));
  size_t lobe    = (size_t)ceil (lobe_un * p_eff) + 1;
  s->spur_guard_bins
      = lobe + (size_t)ceil (MEASURE_SPUR_SIDELOBES * lobe_un * p_eff);
  return s;
}

void
nprmeas_destroy (nprmeas_state_t *state)
{
  if (!state)
    return;
  if (state->psd)
    psd_destroy (state->psd);
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
  memset (&r, 0, sizeof (r));
  /* average the capture's segments -> one-sided cg^2-normalised power */
  psd_reset (s->psd);
  psd_accumulate_real (s->psd, x, n_in);
  size_t nbins = psd_power_onesided (s->psd, s->nfft / 2 + 1, s->pwr);
  if (nbins == 0)
    return r; /* capture holds no full frame */
  size_t half = s->nfft / 2;

  double df    = s->fs / (double)s->nfft;
  double in_lo = active_lo, in_hi = active_hi;
  /* Honour the caller's guard, but never less than the window's own skirt
   * (spur_guard_bins): otherwise active-band leakage folds into the notch. */
  double min_guard = (double)s->spur_guard_bins * df;
  double guard     = (guard_hz > min_guard) ? guard_hz : min_guard;
  double nt_lo = notch_lo - guard, nt_hi = notch_hi + guard;
  double ni_lo = notch_lo + guard, ni_hi = notch_hi - guard;
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
  double ref = s->psd->full_scale * s->psd->full_scale / 2.0 * cal;

  r.npr_db          = 10.0 * log10 (in_mean / nt_mean);
  r.inband_psd_dbfs = 10.0 * log10 (in_mean / ref);
  r.notch_psd_dbfs  = 10.0 * log10 (nt_mean / ref);
  r.n_inband_bins   = in_cnt;
  r.n_notch_bins    = nt_cnt;
  r.rbw_hz          = s->enbw * s->fs / (double)s->n;
  return r;
}

size_t
nprmeas_spectrum_dbfs_max_out (nprmeas_state_t *state)
{
  return state->nfft;
}

size_t
nprmeas_spectrum_dbfs (nprmeas_state_t *state, const float *x, size_t x_len,
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
    out[i] = (float)(10.0 * log10 ((double)state->pwr[i] / ref + NPR_EPS));
  return nfft;
}
