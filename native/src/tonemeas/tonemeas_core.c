/*
 * tonemeas_core.c — single-tone ADC / converter spectral measurement.
 *
 * Owns a window + zero-padded FFT.  Each component's power is integrated over
 * its window MAIN LOBE (±L bins); the noise/distortion sums exclude the
 * leakage bins around DC, the fundamental and each harmonic.  This is the IEEE
 * Std 1241 windowed-tone method: a full-scale tone reads ~0 dBFS regardless of
 * where it lands between FFT bins, because the lobe integral captures the
 * spread energy.
 *
 * Conventions (see docs/design/measurement-suite.md):
 *   - power spectrum P[k] = |X[k]|^2 / cg^2  (cg = sum(window))
 *   - real capture  -> one-sided fold (x2 on non-DC/non-Nyquist bins),
 *                      band [0, fs/2], 0 dBFS = peak-full_scale sine
 *   - complex       -> two-sided DC-centred, band [-fs/2, fs/2),
 *                      0 dBFS = full_scale exponential
 *   - ratios (SNR/SINAD/THD) are normalisation-independent; only the absolute
 *     *_dbfs levels use the per-type full-scale reference.
 */
#include "tonemeas/tonemeas_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TM_EPS 1e-30 /* power floor: keeps log10 finite on clean tones */

/* Sub-bin peak offset in [-0.5, 0.5] from a 3-point parabola on dB values. */
static double
parabolic_delta (double ym1, double y0, double yp1)
{
  double denom = ym1 - 2.0 * y0 + yp1;
  if (fabs (denom) < TM_EPS)
    return 0.0;
  double d = 0.5 * (ym1 - yp1) / denom;
  if (d > 0.5)
    d = 0.5;
  if (d < -0.5)
    d = -0.5;
  return d;
}

tonemeas_state_t *
tonemeas_create (size_t n, double fs, int window, float beta, size_t pad,
                 size_t n_harmonics, double full_scale, size_t dc_guard)
{
  if (n < 2 || fs <= 0.0 || (window != 0 && window != 1) || full_scale <= 0.0)
    return NULL;
  if (pad < 1)
    pad = 1;

  tonemeas_state_t *s = (tonemeas_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  /* The shared PSD core owns the window, the zero-padded FFT and the (single-
   * frame mean) averager; full_scale stays 1.0 there so its linear accessors
   * return bare cg^2-normalised power — this core applies its own dBFS ref. */
  s->psd = welch_create (n, fs, window, beta, pad, 1.0, ACC_TRACE_MEAN, 0.0);
  if (!s->psd)
    {
      tonemeas_destroy (s);
      return NULL;
    }

  s->n          = n;
  s->nfft       = s->psd->nfft;
  s->fs         = fs;
  s->enbw       = s->psd->enbw;
  s->window     = window;
  s->n_harm     = n_harmonics;
  s->full_scale = full_scale;
  s->dc_guard   = dc_guard;

  s->pwr  = (float *)malloc (s->nfft * sizeof (float));
  s->excl = (unsigned char *)malloc (s->nfft * sizeof (unsigned char));
  if (!s->pwr || !s->excl)
    {
      tonemeas_destroy (s);
      return NULL;
    }

  /* Main-lobe half-width: window null-to-null half-width (un-padded bins)
   * scaled by the zero-pad interpolation factor nfft/n, plus a guard bin. */
  double p_eff = (double)s->nfft / (double)n;
  double lobe_unpadded
      = (window == 1) ? sqrt (1.0 + (beta / M_PI) * (beta / M_PI)) : 2.0;
  s->lobe_bins = (size_t)ceil (lobe_unpadded * p_eff) + 1;

  return s;
}

void
tonemeas_destroy (tonemeas_state_t *state)
{
  if (!state)
    return;
  if (state->psd)
    welch_destroy (state->psd);
  free (state->pwr);
  free (state->excl);
  free (state);
}

void
tonemeas_reset (tonemeas_state_t *state)
{
  (void)state; /* each analyze() is independent; nothing to reset */
}

/* Average a real capture over its segments, return the one-sided power into
 * s->pwr[0..nfft/2].  Returns the bin count (nfft/2 + 1), or 0 if the capture
 * holds no full frame. */
static size_t
build_real (tonemeas_state_t *s, const float *x, size_t n_in)
{
  welch_reset (s->psd);
  welch_accumulate_real (s->psd, x, n_in);
  return welch_power_onesided (s->psd, s->nfft / 2 + 1, s->pwr);
}

/* Average a complex capture over its segments, return the DC-centred two-sided
 * power into s->pwr[0..nfft).  Returns nfft, or 0 if no full frame. */
static size_t
build_complex (tonemeas_state_t *s, const float complex *x, size_t n_in)
{
  welch_reset (s->psd);
  welch_accumulate (s->psd, x, n_in);
  return welch_power_twosided (s->psd, s->nfft, s->pwr);
}

/* Fold harmonic frequency k*f0 into the analysed band.
 *   real    -> [0, fs/2] with Nyquist reflection
 *   complex -> [-fs/2, fs/2) by wrapping */
static double
fold_harmonic (double f, double fs, int is_real)
{
  double g = fmod (f, fs);
  if (g < 0.0)
    g += fs;
  if (is_real)
    return (g <= fs / 2.0) ? g : (fs - g);
  return (g >= fs / 2.0) ? (g - fs) : g; /* wrap into [-fs/2, fs/2) */
}

/* Integrate pwr over [c-L, c+L] (clamped); optionally mark the exclusion mask.
 */
static double
lobe_power (const tonemeas_state_t *s, size_t nbins, long c, int mark)
{
  long L  = (long)s->lobe_bins;
  long lo = c - L, hi = c + L;
  if (lo < 0)
    lo = 0;
  if (hi > (long)nbins - 1)
    hi = (long)nbins - 1;
  double p = 0.0;
  for (long k = lo; k <= hi; k++)
    {
      p += (double)s->pwr[k];
      if (mark)
        s->excl[k] = 1;
    }
  return p;
}

/* The shared metric kernel over the prepared power array s->pwr[0..nbins). */
static void
compute_metrics (tonemeas_state_t *s, size_t nbins, long dc_bin, double df,
                 double ref, int is_real, tone_meas_t *out)
{
  memset (out, 0, sizeof (*out));
  memset (s->excl, 0, nbins);
  long guard = (long)s->lobe_bins + (long)s->dc_guard;

  /* 1. locate the fundamental (max power outside the DC region) */
  long   k0   = -1;
  double pmax = -1.0;
  for (long k = 0; k < (long)nbins; k++)
    {
      if (labs (k - dc_bin) <= guard)
        continue;
      if ((double)s->pwr[k] > pmax)
        {
          pmax = (double)s->pwr[k];
          k0   = k;
        }
    }
  if (k0 < 0)
    return; /* degenerate (band too small) */

  double d0 = 0.0;
  if (k0 >= 1 && k0 < (long)nbins - 1)
    d0 = parabolic_delta (10.0 * log10 ((double)s->pwr[k0 - 1] + TM_EPS),
                          10.0 * log10 ((double)s->pwr[k0] + TM_EPS),
                          10.0 * log10 ((double)s->pwr[k0 + 1] + TM_EPS));
  double f0      = ((double)(k0 - dc_bin) + d0) * df;
  out->fund_freq = f0;

  /* 2. exclude the DC region, then integrate the fundamental lobe */
  for (long k = 0; k < (long)nbins; k++)
    if (labs (k - dc_bin) <= guard)
      s->excl[k] = 1;
  double p_fund = lobe_power (s, nbins, k0, 1);

  /* 3. harmonics: fold k*f0, integrate each non-overlapping lobe */
  double  p_harm = 0.0;
  size_t  n_kept = 0;
  size_t *hbin   = (size_t *)malloc ((s->n_harm + 1) * sizeof (size_t));
  for (size_t h = 2; h <= s->n_harm && hbin; h++)
    {
      double fh = fold_harmonic ((double)h * f0, s->fs, is_real);
      long   kh = (long)lround (fh / df) + dc_bin;
      if (kh < 0 || kh >= (long)nbins)
        continue;
      if (s->excl[kh]) /* overlaps DC, fundamental or a kept harmonic */
        continue;
      p_harm += lobe_power (s, nbins, kh, 1);
      hbin[n_kept++] = (size_t)kh;
    }

  /* 4. noise = everything still unexcluded */
  double p_noise = 0.0;
  size_t n_noise = 0;
  for (long k = 0; k < (long)nbins; k++)
    if (!s->excl[k])
      {
        p_noise += (double)s->pwr[k];
        n_noise++;
      }
  if (n_noise == 0)
    n_noise = 1;
  if (p_noise < TM_EPS)
    p_noise = TM_EPS;

  /* 5. worst spur = largest component other than the fundamental.  Scan bins
   * outside the DC + fundamental lobes (harmonics are eligible spurs). */
  long   ks    = -1;
  double psmax = -1.0;
  for (long k = 0; k < (long)nbins; k++)
    {
      if (labs (k - dc_bin) <= guard || labs (k - k0) <= (long)s->lobe_bins)
        continue;
      if ((double)s->pwr[k] > psmax)
        {
          psmax = (double)s->pwr[k];
          ks    = k;
        }
    }
  double p_spur = TM_EPS;
  if (ks >= 0)
    {
      double ds = 0.0;
      if (ks >= 1 && ks < (long)nbins - 1)
        ds = parabolic_delta (10.0 * log10 ((double)s->pwr[ks - 1] + TM_EPS),
                              10.0 * log10 ((double)s->pwr[ks] + TM_EPS),
                              10.0 * log10 ((double)s->pwr[ks + 1] + TM_EPS));
      out->worst_spur_freq = ((double)(ks - dc_bin) + ds) * df;
      p_spur               = lobe_power (s, nbins, ks, 0);
      for (size_t i = 0; i < n_kept; i++)
        if (labs ((long)hbin[i] - ks) <= (long)s->lobe_bins)
          out->worst_spur_is_harm = 1;
    }
  free (hbin);

  if (p_fund < TM_EPS)
    p_fund = TM_EPS;
  if (p_harm < TM_EPS)
    p_harm = TM_EPS;

  /* 6. assemble the bag.
   *
   * Absolute dBFS levels need a coherent-power calibration: a lobe-integrated
   * tone captures the full main-lobe energy, which the window's ENBW and the
   * zero-pad density inflate by cal = (nfft/n)*enbw relative to the true tone
   * power.  Dividing by cal makes a full-scale tone read 0 dBFS.  The ratio
   * metrics (SNR/SINAD/THD/SFDR_dBc) use raw lobe powers, where cal cancels.
   */
  double cal            = (double)s->nfft / (double)s->n * s->enbw;
  double ref_cal        = ref * cal;
  double fund_dbfs      = 10.0 * log10 (p_fund / ref_cal);
  double spur_dbfs      = 10.0 * log10 (p_spur / ref_cal);
  out->snr              = 10.0 * log10 (p_fund / p_noise);
  out->sinad            = 10.0 * log10 (p_fund / (p_noise + p_harm));
  out->thd              = 10.0 * log10 (p_harm / p_fund);
  out->thd_pct          = 100.0 * sqrt (p_harm / p_fund);
  out->thd_n            = -out->sinad;
  out->sfdr_dbc         = fund_dbfs - spur_dbfs;
  out->sfdr_dbfs        = -spur_dbfs;
  out->enob             = (out->sinad - 1.76) / 6.02;
  out->enob_fs          = (out->sinad - 1.76 - fund_dbfs) / 6.02;
  out->noise_floor_dbfs = 10.0 * log10 (p_noise / (double)n_noise / ref_cal);
  out->fund_dbfs        = fund_dbfs;
  out->worst_spur_dbc   = spur_dbfs - fund_dbfs;
  out->rbw_hz           = s->enbw * s->fs / (double)s->n;
  out->enbw_hz          = out->rbw_hz;
  out->bin_hz           = s->fs / (double)s->nfft;
  out->lobe_bins        = s->lobe_bins;
  out->n_noise_bins     = n_noise;
  out->proc_gain_db     = 10.0 * log10 ((double)s->nfft / 2.0);
  out->amp_uncert_db    = (s->window == 1) ? 0.01 : 0.03;
  out->floor_uncert_db  = 4.34 / sqrt ((double)n_noise);
}

tone_meas_t
tonemeas_analyze (tonemeas_state_t *state, const float *x, size_t n_in)
{
  tone_meas_t r;
  size_t      nbins = build_real (state, x, n_in);
  double      df    = state->fs / (double)state->nfft;
  double ref = state->full_scale * state->full_scale / 2.0; /* sine power */
  compute_metrics (state, nbins, 0, df, ref, 1, &r);
  return r;
}

tone_meas_t
tonemeas_analyze_complex (tonemeas_state_t *state, const float complex *x,
                          size_t n_in)
{
  tone_meas_t r;
  size_t      nbins = build_complex (state, x, n_in);
  double      df    = state->fs / (double)state->nfft;
  double ref = state->full_scale * state->full_scale; /* exponential power */
  compute_metrics (state, nbins, (long)(state->nfft / 2), df, ref, 0, &r);
  return r;
}

time_stats_t
tonemeas_time_stats (tonemeas_state_t *state, const float *x, size_t n_in)
{
  time_stats_t r;
  size_t       n   = n_in;
  double       sum = 0.0, sumsq = 0.0, peak_abs = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double v = (double)x[i];
      sum += v;
      sumsq += v * v;
      double a = fabs (v);
      if (a > peak_abs)
        peak_abs = a;
    }
  double dc      = (n > 0) ? sum / (double)n : 0.0;
  double ms      = (n > 0) ? sumsq / (double)n : 0.0;
  double rms     = sqrt (ms);
  double ms_ac   = ms - dc * dc;
  double rms_ac  = (ms_ac > 0.0) ? sqrt (ms_ac) : 0.0;
  double peak_ac = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double a = fabs ((double)x[i] - dc);
      if (a > peak_ac)
        peak_ac = a;
    }
  r.rms         = rms;
  r.peak        = peak_ac;
  r.crest_db    = (rms_ac > 0.0) ? 20.0 * log10 (peak_ac / rms_ac) : 0.0;
  r.papr_db     = r.crest_db;
  r.dc_offset   = dc;
  r.fs_util_pct = 100.0 * peak_abs / state->full_scale;
  return r;
}

size_t
tonemeas_spectrum_dbfs_max_out (tonemeas_state_t *state)
{
  return state->nfft;
}

size_t
tonemeas_spectrum_dbfs (tonemeas_state_t *state, const float *x, size_t x_len,
                        float *out)
{
  /* DC-centred two-sided dBFS view of a real capture (analyzer display):
   * the same averaged PSD the metrics use, scaled to the 0-dBFS reference. */
  welch_reset (state->psd);
  welch_accumulate_real (state->psd, x, x_len);
  size_t nfft = welch_power_twosided (state->psd, state->nfft, state->pwr);
  if (nfft == 0)
    return 0;
  double ref = state->full_scale * state->full_scale;
  for (size_t i = 0; i < nfft; i++)
    out[i] = (float)(10.0 * log10 ((double)state->pwr[i] / ref + TM_EPS));
  return nfft;
}
