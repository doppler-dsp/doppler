/**
 * @file psd_core.c
 * @brief PSD — averaging power-spectral-density estimator (Welch's method).
 *
 * Composition over reimplementation: each accumulated frame is windowed, run
 * through the shared ::fft_state_t plan, converted to power, fftshifted to
 * DC-centred order and folded into an ::acc_trace_state_t averager.  The
 * measurement getters read the averaged power back and reuse the spectral free
 * functions (::find_peaks_f32, ::noise_floor_db) for level statistics.
 *
 * Normalisation: dividing |X|^2 by the window coherent gain squared (cg^2)
 * makes a full-scale tone read its true power (psd_db); dividing instead by
 * fs*sum(w^2) yields PSD in dB/Hz (psd_dbhz).  The two differ only by the
 * constant 10*log10(cg^2 / (fs*s2)).  Band-*integrated* power (band_power,
 * total_band_power) is a third case: it normalises by the noise-power gain
 * nfft*s2, not cg^2, so an integrated band is window- and pad-independent —
 * see psd_band_lin.
 */
#include "psd/psd_core.h"
#include "spectral/spectral_core.h"
#include <math.h>

/* Power floor (~ -200 dB) guarding log10 of empty / zero bins. */
#define PSD_FLOOR 1e-20

/* Smallest power of two >= x. */
static size_t
next_pow2 (size_t x)
{
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */

psd_state_t *
psd_create (size_t n, double fs, int window, float beta, size_t pad,
            double full_scale, size_t bits, int mode, double alpha)
{
  /* The single definition of the dBFS reference: bits>0 selects an ADC
   * full scale (2^(bits-1)); otherwise full_scale is the analog/general ref.
   */
  if (bits > 0)
    full_scale = ldexp (1.0, (int)bits - 1);
  if (n < 2 || fs <= 0.0 || window < 0 || window > 2 || full_scale <= 0.0)
    return NULL;
  if (mode < ACC_TRACE_MEAN || mode > ACC_TRACE_MINHOLD)
    return NULL;
  if (pad < 1)
    pad = 1;

  psd_state_t *s = (psd_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  const size_t nfft = next_pow2 (n * pad);
  s->n              = n;
  s->nfft           = nfft;
  s->fs             = fs;
  s->full_scale     = full_scale;
  s->bits           = bits;
  s->w              = (float *)malloc (n * sizeof (float));
  s->frame          = (float complex *)malloc (nfft * sizeof (float complex));
  s->spec           = (float complex *)malloc (nfft * sizeof (float complex));
  s->pwr            = (float *)malloc (nfft * sizeof (float));
  s->dbbuf          = (float *)malloc (nfft * sizeof (float));
  if (!s->w || !s->frame || !s->spec || !s->pwr || !s->dbbuf)
    {
      psd_destroy (s);
      return NULL;
    }

  if (window == 1)
    kaiser_window (s->w, n, beta);
  else if (window == 2)
    blackman_harris_window (s->w, n);
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

  s->fft = fft_create (nfft, -1, 1);
  s->avg = acc_trace_create (nfft, mode, alpha);
  if (!s->fft || !s->avg)
    {
      psd_destroy (s);
      return NULL;
    }
  return s;
}

void
psd_destroy (psd_state_t *state)
{
  if (!state)
    return;
  if (state->fft)
    fft_destroy (state->fft);
  if (state->avg)
    acc_trace_destroy (state->avg);
  free (state->w);
  free (state->frame);
  free (state->spec);
  free (state->pwr);
  free (state->dbbuf);
  free (state);
}

void
psd_reset (psd_state_t *state)
{
  acc_trace_reset (state->avg);
}

/* Serializable state — delegates to the acc_trace power averager (the only
 * running state); window, FFT plan, and scratch are config (create). */
size_t
psd_state_bytes (const psd_state_t *s)
{
  return sizeof (dp_state_hdr_t) + acc_trace_state_bytes (s->avg);
}

void
psd_get_state (const psd_state_t *s, void *blob)
{
  DP_GET_OPEN (PSD_STATE_MAGIC, PSD_STATE_VERSION, psd_state_bytes (s));
  DP_W_CHILD (&_w, acc_trace, s->avg);
}

int
psd_set_state (psd_state_t *s, const void *blob)
{
  DP_SET_OPEN (PSD_STATE_MAGIC, PSD_STATE_VERSION, psd_state_bytes (s));
  DP_R_CHILD (&_r, acc_trace, s->avg);
  return DP_OK;
}

/* ── accumulation ──────────────────────────────────────────────────────── */

/* Transform the already-windowed-and-zero-padded state->frame, convert to
 * DC-centred two-sided power and fold one frame into the running average. */
static void
psd_fold_frame (psd_state_t *state)
{
  const size_t nfft = state->nfft;
  const size_t half = nfft / 2; /* fftshift roll: bin 0 -> index nfft/2 */

  fft_execute_cf32 (state->fft, state->frame, nfft, state->spec);

  for (size_t k = 0; k < nfft; k++)
    {
      const float re  = crealf (state->spec[k]);
      const float im  = cimagf (state->spec[k]);
      size_t      idx = k + half;
      if (idx >= nfft)
        idx -= nfft;
      state->pwr[idx] = re * re + im * im;
    }
  acc_trace_accumulate (state->avg, state->pwr, nfft);
}

/* Zero the zero-pad tail frame[n..nfft-1] (no-op when nfft == n). */
static void
psd_zero_pad (psd_state_t *state)
{
  for (size_t i = state->n; i < state->nfft; i++)
    state->frame[i] = 0.0f;
}

void
psd_accumulate (psd_state_t *state, const float complex *x, size_t x_len)
{
  const size_t n      = state->n;
  const size_t nframe = x_len / n;

  for (size_t f = 0; f < nframe; f++)
    {
      const float complex *xf = x + f * n;
      for (size_t i = 0; i < n; i++)
        state->frame[i] = state->w[i] * xf[i];
      psd_zero_pad (state);
      psd_fold_frame (state);
    }
}

void
psd_accumulate_real (psd_state_t *state, const float *x, size_t x_len)
{
  const size_t n      = state->n;
  const size_t nframe = x_len / n;

  for (size_t f = 0; f < nframe; f++)
    {
      const float *xf = x + f * n;
      for (size_t i = 0; i < n; i++)
        state->frame[i] = (float complex) (state->w[i] * xf[i]);
      psd_zero_pad (state);
      psd_fold_frame (state);
    }
}

/* ── internal helpers ──────────────────────────────────────────────────── */

/* Pull the averaged power trace into state->pwr; 0 if nothing accumulated. */
static size_t
psd_pull_power (psd_state_t *s)
{
  if (s->avg->count == 0)
    return 0;
  return acc_trace_value (s->avg, s->nfft, s->pwr);
}

/* dBFS reference: cg^2 (coherent gain) times the 0-dBFS amplitude squared.
 * full_scale == 1.0 (the default) reduces this to the bare cg^2 normalisation,
 * so every dB getter is byte-identical to the un-scaled estimator. */
static double
psd_db_ref (const psd_state_t *s)
{
  return s->cg * s->cg * s->full_scale * s->full_scale;
}

/* Fill out[0..nfft-1] with the averaged power spectrum in dBFS. */
static int
psd_fill_db (psd_state_t *s, float *out)
{
  if (!psd_pull_power (s))
    return 0;
  const double ref = psd_db_ref (s);
  for (size_t i = 0; i < s->nfft; i++)
    {
      double p = (double)s->pwr[i] / ref;
      out[i]   = (float)(10.0 * log10 (fmax (p, PSD_FLOOR)));
    }
  return 1;
}

/* ── linear-power accessors (raw spectral estimate for measurement) ────── */

size_t
psd_power_twosided_max_out (psd_state_t *state)
{
  return state->nfft;
}

size_t
psd_power_twosided (psd_state_t *state, size_t cap, float *out)
{
  (void)cap;
  if (!psd_pull_power (state))
    return 0;
  const double cg2 = state->cg * state->cg;
  for (size_t i = 0; i < state->nfft; i++)
    out[i] = (float)((double)state->pwr[i] / cg2);
  return state->nfft;
}

size_t
psd_power_onesided_max_out (psd_state_t *state)
{
  return state->nfft / 2 + 1;
}

size_t
psd_power_onesided (psd_state_t *state, size_t cap, float *out)
{
  (void)cap;
  if (!psd_pull_power (state))
    return 0;
  const double cg2  = state->cg * state->cg;
  const size_t nfft = state->nfft;
  const size_t half = nfft / 2; /* DC sits at index half in pwr[] */

  /* DC and Nyquist have no mirror partner; interior bins fold +k with -k. */
  out[0]    = (float)((double)state->pwr[half] / cg2);
  out[half] = (float)((double)state->pwr[0] / cg2);
  for (size_t m = 1; m < half; m++)
    out[m]
        = (float)(((double)state->pwr[half + m] + (double)state->pwr[half - m])
                  / cg2);
  return half + 1;
}

/* Map a [lo,hi] Hz band to inclusive DC-centred bin indices.  Returns 0 if the
 * band lies entirely outside the analysed span [-fs/2, fs/2). */
static int
psd_band_bins (const psd_state_t *s, double lo, double hi, size_t *ilo_out,
               size_t *ihi_out)
{
  if (hi < lo)
    {
      const double t = lo;
      lo             = hi;
      hi             = t;
    }
  const double half = (double)(s->nfft / 2);
  long         ilo  = (long)lround (lo / s->fs * (double)s->nfft + half);
  long         ihi  = (long)lround (hi / s->fs * (double)s->nfft + half);
  if (ihi < 0 || ilo > (long)s->nfft - 1)
    return 0;
  if (ilo < 0)
    ilo = 0;
  if (ihi > (long)s->nfft - 1)
    ihi = (long)s->nfft - 1;
  *ilo_out = (size_t)ilo;
  *ihi_out = (size_t)ihi;
  return 1;
}

/* Linear power integrated over a [lo,hi] Hz band (dBFS reference).
 *
 * Band-integrated power uses the *noise-power* (incoherent) window
 * normalisation nfft*s2, NOT the coherent gain cg^2 that psd_db uses to read a
 * single tone's peak bin.  The two differ by the window's equivalent noise
 * bandwidth, ENBW = nfft*s2/cg^2: a windowed tone leaks across ~ENBW bins, so
 * summing pwr/cg^2 over a band over-counts the true power by ENBW — and, with
 * zero-padding, by a further nfft/n.  Normalising by nfft*s2 (== cg^2 * nfft/n
 * * enbw, the same calibration nprmeas applies per bin) makes an integrated
 * band read its true power window- and pad-independently: by Parseval a
 * full-scale tone integrates to full_scale^2 and a noise band to its variance.
 */
static double
psd_band_lin (const psd_state_t *s, double lo, double hi)
{
  size_t ilo, ihi;
  if (!psd_band_bins (s, lo, hi, &ilo, &ihi))
    return 0.0;
  const double ref = (double)s->nfft * s->s2 * s->full_scale * s->full_scale;
  double       sum = 0.0;
  for (size_t i = ilo; i <= ihi; i++)
    sum += (double)s->pwr[i] / ref;
  return sum;
}

/* ── PSD trace getters ─────────────────────────────────────────────────── */

size_t
psd_psd_db_max_out (psd_state_t *state)
{
  return state->nfft;
}

size_t
psd_psd_db (psd_state_t *state, size_t n, float *out)
{
  (void)n;
  if (!psd_fill_db (state, out))
    return 0;
  return state->nfft;
}

size_t
psd_psd_dbhz_max_out (psd_state_t *state)
{
  return state->nfft;
}

size_t
psd_psd_dbhz (psd_state_t *state, size_t n, float *out)
{
  if (!psd_psd_db (state, n, out))
    return 0;
  /* dB/Hz differs from the dBFS spectrum by a constant. */
  const double off
      = 10.0 * log10 (state->fs * state->s2 / (state->cg * state->cg));
  for (size_t i = 0; i < state->nfft; i++)
    out[i] = (float)((double)out[i] - off);
  return state->nfft;
}

/* ── band power ────────────────────────────────────────────────────────── */

size_t
psd_band_power_max_out (psd_state_t *state)
{
  (void)state;
  return 0; /* the binding grows the buffer to the bands length */
}

size_t
psd_band_power (psd_state_t *state, const double *bands, size_t bands_len,
                float *out)
{
  if (!psd_pull_power (state))
    return 0;
  const size_t nb = bands_len / 2;
  for (size_t b = 0; b < nb; b++)
    {
      double lp = psd_band_lin (state, bands[2 * b], bands[2 * b + 1]);
      out[b]    = (float)(10.0 * log10 (fmax (lp, PSD_FLOOR)));
    }
  return nb;
}

double
psd_total_band_power (psd_state_t *state, const double *bands,
                      size_t bands_len)
{
  if (!psd_pull_power (state))
    return 10.0 * log10 (PSD_FLOOR);
  const size_t nb  = bands_len / 2;
  double       sum = 0.0;
  for (size_t b = 0; b < nb; b++)
    sum += psd_band_lin (state, bands[2 * b], bands[2 * b + 1]);
  return 10.0 * log10 (fmax (sum, PSD_FLOOR));
}

/* ── scalar measurements ───────────────────────────────────────────────── */

double
psd_occupied_bw (psd_state_t *state, double fraction)
{
  if (!psd_pull_power (state))
    return 0.0;
  const size_t n   = state->nfft;
  const double cg2 = state->cg * state->cg;

  double total = 0.0;
  for (size_t i = 0; i < n; i++)
    total += (double)state->pwr[i] / cg2;
  if (total <= 0.0)
    return 0.0;

  const double lower = (1.0 - fraction) * 0.5 * total;
  const double upper = (1.0 + fraction) * 0.5 * total;

  double cum    = 0.0;
  size_t ilo    = 0;
  size_t ihi    = n - 1;
  int    got_lo = 0;
  for (size_t i = 0; i < n; i++)
    {
      cum += (double)state->pwr[i] / cg2;
      if (!got_lo && cum >= lower)
        {
          ilo    = i;
          got_lo = 1;
        }
      if (cum >= upper)
        {
          ihi = i;
          break;
        }
    }
  return ((double)(ihi - ilo) + 1.0) * state->fs / (double)n;
}

double
psd_noise_floor (psd_state_t *state)
{
  if (!psd_fill_db (state, state->dbbuf))
    return 0.0;
  return noise_floor_db (state->dbbuf, state->nfft);
}

double
psd_snr (psd_state_t *state, double lo_hz, double hi_hz)
{
  if (!psd_fill_db (state, state->dbbuf))
    return 0.0;
  const double floor = noise_floor_db (state->dbbuf, state->nfft);

  size_t ilo, ihi;
  if (!psd_band_bins (state, lo_hz, hi_hz, &ilo, &ihi))
    return 0.0;
  double peak = -INFINITY;
  for (size_t i = ilo; i <= ihi; i++)
    if (state->dbbuf[i] > peak)
      peak = state->dbbuf[i];
  return peak - floor;
}

double
psd_sfdr (psd_state_t *state, float min_db)
{
  if (!psd_fill_db (state, state->dbbuf))
    return 0.0;
  dp_peak_t    pk[16];
  const size_t np = find_peaks_f32 (state->dbbuf, state->nfft, 16, min_db, pk);
  if (np < 2)
    return 0.0;
  /* find_peaks_f32 returns peaks sorted by descending amplitude. */
  return (double)(pk[0].amplitude_db - pk[1].amplitude_db);
}
