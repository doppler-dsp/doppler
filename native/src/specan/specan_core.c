/**
 * @file specan_core.c
 * @brief Specan implementation — DDC tuner/decimator + averaging-PSD display.
 *
 * See specan_core.h for the design.  This file owns only the natural-parameter
 * arithmetic (RBW → window length + Kaiser beta, span → decimation rate, the
 * display crop) and the per-call plumbing between the Ddc and the Welch core;
 * the signal processing itself lives in those composed objects.
 */
#include "specan/specan_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "spectral/spectral_core.h" /* kaiser_window, kaiser_enbw */

/* fs_out = span * 1.28 places the display window (±span/2) inside the DDC
 * passband (±0.4·fs_out = ±0.512·span); the transition/stop bands are cropped
 * off by keeping the central 2·round(nfft/2.56)+1 bins. */
#define SPECAN_OVERSAMPLE 1.28
#define SPECAN_PAD 2u
#define SPECAN_EPS 1e-20f

/* Smallest power of two >= x (x >= 1). */
static size_t
next_pow2 (size_t x)
{
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

/* Kaiser beta whose equivalent noise bandwidth (in FFT bins) equals
 * target_enbw, found by bisection on the actual window — exact for this n.
 * Mirrors the inverse search doppler.specan's engine used to do in Python. */
static double
kaiser_beta_for_enbw (double target_enbw, size_t n)
{
  if (target_enbw <= 1.0)
    return 0.0;
  float *w = malloc (n * sizeof *w);
  if (!w)
    return 0.0;
  double lo = 0.0, hi = 60.0; /* beta=60 gives ENBW well past 2.1 bins */
  for (int i = 0; i < 60; i++)
    {
      double mid = 0.5 * (lo + hi);
      kaiser_window (w, n, (float)mid);
      if ((double)kaiser_enbw (w, n) < target_enbw)
        lo = mid;
      else
        hi = mid;
    }
  free (w);
  return 0.5 * (lo + hi);
}

specan_state_t *
specan_create (double fs, double span, double rbw, double src_center,
               double center, double ref_db, int window, size_t navg)
{
  if (fs <= 0.0 || span <= 0.0 || rbw <= 0.0 || navg < 1)
    return NULL;

  /* span → decimated rate (clamped so we never up-sample above the input). */
  double fs_out = span * SPECAN_OVERSAMPLE;
  if (fs_out > fs)
    fs_out = fs;

  /* RBW → window length (coarse) + Kaiser beta (fine). */
  size_t n = next_pow2 ((size_t)ceil (fs_out / rbw));
  if (n < 2)
    n = 2;
  double target_enbw = rbw / (fs_out / (double)n);
  if (target_enbw < 1.0)
    target_enbw = 1.0;
  double beta = (window == 1) ? kaiser_beta_for_enbw (target_enbw, n) : 0.0;

  /* Zero-padded transform length (must match welch_create's nfft) and the
   * central display crop covering ±span/2. */
  size_t nfft = next_pow2 (n * SPECAN_PAD);
  size_t half = (size_t)lround ((double)nfft / 2.56);
  if (half > nfft / 2)
    half = nfft / 2;

  specan_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;

  double rate = fs_out / fs;
  /* Mix the carrier at `center` (relative to src_center) DOWN to DC: the Ddc
   * shifts content up by +norm_freq, so a positive offset needs a negative LO
   * (matching ddc_core's "norm_freq = -f_carrier shifts f_carrier to DC"). */
  double norm_freq = -(center - src_center) / fs;
  s->ddc           = ddc_create (norm_freq, rate);
  s->psd
      = welch_create (n, fs_out, window, (float)beta, SPECAN_PAD, 1.0, 0, 0.1);
  s->pwr = malloc (nfft * sizeof *s->pwr);
  if (!s->ddc || !s->psd || !s->pwr)
    {
      specan_destroy (s);
      return NULL;
    }

  s->fs_in      = fs;
  s->src_center = src_center;
  s->center     = center;
  s->span       = span;
  s->rbw        = rbw;
  s->ref_db     = ref_db;
  s->fs_out     = fs_out;
  s->beta       = beta;
  s->n          = n;
  s->nfft       = nfft;
  s->navg       = navg;
  s->disp_n     = 2 * half + 1;
  s->disp_lo    = nfft / 2 - half;
  return s;
}

void
specan_destroy (specan_state_t *state)
{
  if (!state)
    return;
  if (state->ddc)
    ddc_destroy (state->ddc);
  if (state->psd)
    welch_destroy (state->psd);
  free (state->scratch);
  free (state->pend);
  free (state->pwr);
  free (state);
}

void
specan_reset (specan_state_t *state)
{
  ddc_reset (state->ddc);
  welch_reset (state->psd);
  state->pend_len = 0;
}

size_t
specan_execute_max_out (specan_state_t *state)
{
  return state->disp_n;
}

size_t
specan_execute (specan_state_t *state, const float complex *x, size_t x_len,
                float *out, size_t max_out)
{
  /* Mix to DC and decimate; output length <= x_len since rate <= 1. */
  if (state->scratch_cap < x_len)
    {
      float complex *p = realloc (state->scratch, x_len * sizeof *p);
      if (!p)
        return 0;
      state->scratch     = p;
      state->scratch_cap = x_len;
    }
  size_t m
      = ddc_execute (state->ddc, x, x_len, state->scratch, state->scratch_cap);

  /* Buffer the decimated samples until a full averaging window is available.
   */
  size_t need = state->n * state->navg;
  if (state->pend_len + m > state->pend_cap)
    {
      size_t cap = state->pend_len + m;
      if (cap < need)
        cap = need;
      float complex *p = realloc (state->pend, cap * sizeof *p);
      if (!p)
        return 0;
      state->pend     = p;
      state->pend_cap = cap;
    }
  memcpy (state->pend + state->pend_len, state->scratch,
          m * sizeof *state->scratch);
  state->pend_len += m;

  if (state->pend_len < need)
    return 0;

  /* Fresh frame: average navg segments, then read the linear two-sided power.
   */
  welch_reset (state->psd);
  welch_accumulate (state->psd, state->pend, need);
  state->pend_len -= need;
  memmove (state->pend, state->pend + need,
           state->pend_len * sizeof *state->pend);

  welch_power_twosided (state->psd, state->nfft, state->pwr);

  /* Crop the central display band and convert to dB (+ ref offset). */
  size_t cnt = state->disp_n;
  if (cnt > max_out)
    cnt = max_out;
  for (size_t i = 0; i < cnt; i++)
    out[i] = 10.0f * log10f (state->pwr[state->disp_lo + i] + SPECAN_EPS)
             + (float)state->ref_db;
  return cnt;
}

void
specan_retune (specan_state_t *state, double center)
{
  state->center = center;
  ddc_set_norm_freq (state->ddc,
                     -(center - state->src_center) / state->fs_in);
  state->pend_len = 0; /* drop stale-tune samples; next frame is single-tune */
}
