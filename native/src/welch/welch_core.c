/**
 * @file welch_core.c
 * @brief Welch averaging PSD estimator and spectral measurement suite.
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
 * constant 10*log10(cg^2 / (fs*s2)).
 */
#include "welch/welch_core.h"
#include "spectral/spectral_core.h"
#include <math.h>

/* Power floor (~ -200 dB) guarding log10 of empty / zero bins. */
#define WELCH_FLOOR 1e-20

/* ── lifecycle ─────────────────────────────────────────────────────────── */

welch_state_t *
welch_create (size_t n, double fs, int window, float beta, int mode,
              double alpha)
{
  if (n < 2 || fs <= 0.0 || window < 0 || window > 1)
    return NULL;
  if (mode < ACC_TRACE_MEAN || mode > ACC_TRACE_MINHOLD)
    return NULL;

  welch_state_t *s = (welch_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->n     = n;
  s->fs    = fs;
  s->w     = (float *)malloc (n * sizeof (float));
  s->frame = (float complex *)malloc (n * sizeof (float complex));
  s->spec  = (float complex *)malloc (n * sizeof (float complex));
  s->pwr   = (float *)malloc (n * sizeof (float));
  s->dbbuf = (float *)malloc (n * sizeof (float));
  if (!s->w || !s->frame || !s->spec || !s->pwr || !s->dbbuf)
    {
      welch_destroy (s);
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

  s->fft = fft_create (n, -1, 1);
  s->avg = acc_trace_create (n, mode, alpha);
  if (!s->fft || !s->avg)
    {
      welch_destroy (s);
      return NULL;
    }
  return s;
}

void
welch_destroy (welch_state_t *state)
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
welch_reset (welch_state_t *state)
{
  acc_trace_reset (state->avg);
}

/* ── accumulation ──────────────────────────────────────────────────────── */

void
welch_accumulate (welch_state_t *state, const float complex *x, size_t x_len)
{
  const size_t n      = state->n;
  const size_t nframe = x_len / n;
  const size_t half   = n / 2; /* fftshift roll: bin 0 -> index n/2 */

  for (size_t f = 0; f < nframe; f++)
    {
      const float complex *xf = x + f * n;
      for (size_t i = 0; i < n; i++)
        state->frame[i] = state->w[i] * xf[i];

      fft_execute_cf32 (state->fft, state->frame, n, state->spec);

      for (size_t k = 0; k < n; k++)
        {
          const float re  = crealf (state->spec[k]);
          const float im  = cimagf (state->spec[k]);
          size_t      idx = k + half;
          if (idx >= n)
            idx -= n;
          state->pwr[idx] = re * re + im * im;
        }
      acc_trace_accumulate (state->avg, state->pwr, n);
    }
}

/* ── internal helpers ──────────────────────────────────────────────────── */

/* Pull the averaged power trace into state->pwr; 0 if nothing accumulated. */
static size_t
welch_pull_power (welch_state_t *s)
{
  if (s->avg->count == 0)
    return 0;
  return acc_trace_value (s->avg, s->n, s->pwr);
}

/* Fill out[0..n-1] with the averaged power spectrum in dB (cg^2 normalised).
 */
static int
welch_fill_db (welch_state_t *s, float *out)
{
  if (!welch_pull_power (s))
    return 0;
  const double cg2 = s->cg * s->cg;
  for (size_t i = 0; i < s->n; i++)
    {
      double p = (double)s->pwr[i] / cg2;
      out[i]   = (float)(10.0 * log10 (fmax (p, WELCH_FLOOR)));
    }
  return 1;
}

/* Map a [lo,hi] Hz band to inclusive DC-centred bin indices.  Returns 0 if the
 * band lies entirely outside the analysed span [-fs/2, fs/2). */
static int
welch_band_bins (const welch_state_t *s, double lo, double hi, size_t *ilo_out,
                 size_t *ihi_out)
{
  if (hi < lo)
    {
      const double t = lo;
      lo             = hi;
      hi             = t;
    }
  const double half = (double)(s->n / 2);
  long         ilo  = (long)lround (lo / s->fs * (double)s->n + half);
  long         ihi  = (long)lround (hi / s->fs * (double)s->n + half);
  if (ihi < 0 || ilo > (long)s->n - 1)
    return 0;
  if (ilo < 0)
    ilo = 0;
  if (ihi > (long)s->n - 1)
    ihi = (long)s->n - 1;
  *ilo_out = (size_t)ilo;
  *ihi_out = (size_t)ihi;
  return 1;
}

/* Linear power integrated over a [lo,hi] Hz band (cg^2 normalised). */
static double
welch_band_lin (const welch_state_t *s, double lo, double hi)
{
  size_t ilo, ihi;
  if (!welch_band_bins (s, lo, hi, &ilo, &ihi))
    return 0.0;
  const double cg2 = s->cg * s->cg;
  double       sum = 0.0;
  for (size_t i = ilo; i <= ihi; i++)
    sum += (double)s->pwr[i] / cg2;
  return sum;
}

/* ── PSD trace getters ─────────────────────────────────────────────────── */

size_t
welch_psd_db_max_out (welch_state_t *state)
{
  return state->n;
}

size_t
welch_psd_db (welch_state_t *state, size_t n, float *out)
{
  (void)n;
  if (!welch_fill_db (state, out))
    return 0;
  return state->n;
}

size_t
welch_psd_dbhz_max_out (welch_state_t *state)
{
  return state->n;
}

size_t
welch_psd_dbhz (welch_state_t *state, size_t n, float *out)
{
  if (!welch_psd_db (state, n, out))
    return 0;
  /* dB/Hz differs from the cg^2-normalised dB spectrum by a constant. */
  const double off
      = 10.0 * log10 (state->fs * state->s2 / (state->cg * state->cg));
  for (size_t i = 0; i < state->n; i++)
    out[i] = (float)((double)out[i] - off);
  return state->n;
}

/* ── band power ────────────────────────────────────────────────────────── */

size_t
welch_band_power_max_out (welch_state_t *state)
{
  (void)state;
  return 0; /* the binding grows the buffer to the bands length */
}

size_t
welch_band_power (welch_state_t *state, const double *bands, size_t bands_len,
                  float *out)
{
  if (!welch_pull_power (state))
    return 0;
  const size_t nb = bands_len / 2;
  for (size_t b = 0; b < nb; b++)
    {
      double lp = welch_band_lin (state, bands[2 * b], bands[2 * b + 1]);
      out[b]    = (float)(10.0 * log10 (fmax (lp, WELCH_FLOOR)));
    }
  return nb;
}

double
welch_total_band_power (welch_state_t *state, const double *bands,
                        size_t bands_len)
{
  if (!welch_pull_power (state))
    return 10.0 * log10 (WELCH_FLOOR);
  const size_t nb  = bands_len / 2;
  double       sum = 0.0;
  for (size_t b = 0; b < nb; b++)
    sum += welch_band_lin (state, bands[2 * b], bands[2 * b + 1]);
  return 10.0 * log10 (fmax (sum, WELCH_FLOOR));
}

/* ── scalar measurements ───────────────────────────────────────────────── */

double
welch_occupied_bw (welch_state_t *state, double fraction)
{
  if (!welch_pull_power (state))
    return 0.0;
  const size_t n   = state->n;
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
welch_noise_floor (welch_state_t *state)
{
  if (!welch_fill_db (state, state->dbbuf))
    return 0.0;
  return noise_floor_db (state->dbbuf, state->n);
}

double
welch_snr (welch_state_t *state, double lo_hz, double hi_hz)
{
  if (!welch_fill_db (state, state->dbbuf))
    return 0.0;
  const double floor = noise_floor_db (state->dbbuf, state->n);

  size_t ilo, ihi;
  if (!welch_band_bins (state, lo_hz, hi_hz, &ilo, &ihi))
    return 0.0;
  double peak = -INFINITY;
  for (size_t i = ilo; i <= ihi; i++)
    if (state->dbbuf[i] > peak)
      peak = state->dbbuf[i];
  return peak - floor;
}

double
welch_sfdr (welch_state_t *state, float min_db)
{
  if (!welch_fill_db (state, state->dbbuf))
    return 0.0;
  dp_peak_t    pk[16];
  const size_t np = find_peaks_f32 (state->dbbuf, state->n, 16, min_db, pk);
  if (np < 2)
    return 0.0;
  /* find_peaks_f32 returns peaks sorted by descending amplitude. */
  return (double)(pk[0].amplitude_db - pk[1].amplitude_db);
}
