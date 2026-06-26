/**
 * @file acq_core.c
 * @brief Streaming DSSS burst-acquisition engine implementation.
 *
 * Owns the full receive-side acquisition pipeline: a raw cf32 ring, the
 * slow-time Doppler FFT, a single-row-reference 2-D code correlator (corr2d,
 * coherently integrating `dwell` frames), and a CFAR threshold gate whose
 * threshold and dwell are auto-configured from a target (Pfa, Pd) using the
 * detection-theory functions.  The CFAR helpers (_noise_estimate,
 * _ring_create) are reused header-only from det_private.h — no link to
 * detector_core.
 */

#include "acq/acq_core.h"
/* det_private.h needs det_noise_mode_t (from detector2d_core.h, included
 * above) and supplies the header-only _ring_create / _noise_estimate statics.
 */
#include "detector/det_private.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* sqrt(2/pi): maps the raw Rayleigh threshold eta into mean-estimator units,
 * since E|Rayleigh(1)| = sqrt(pi/2).  Its reciprocal recovers eta from the
 * mean-based test statistic for the per-sample SNR estimate. */
#define ACQ_SQRT_2_OVER_PI 0.7978845608028654f

/* ── Helpers ────────────────────────────────────────────────────────────── */

/* Peak, CFAR noise, and test statistic from the coherent dump in out_buf. */
static void
_compute_stat (acq_state_t *st)
{
  const size_t n = st->n;

  for (size_t k = 0; k < n; k++)
    st->mag_buf[k] = cabsf (st->out_buf[k]);

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (st->mag_buf[k] > st->mag_buf[peak])
      peak = k;

  st->peak_row = peak / st->nx;
  st->peak_col = peak % st->nx;
  st->peak_mag = st->mag_buf[peak];

  st->noise_est = _noise_estimate (st->mag_buf, st->noise_lo, st->noise_hi,
                                   st->noise_scratch, st->noise_mode);

  st->test_stat
      = (st->noise_est > 0.0f) ? (st->peak_mag / st->noise_est) : 0.0f;
}

/* Bonferroni threshold + minimum dwell from the (pfa, pd) target. */
static void
_auto_config (acq_state_t *st, double pfa, double pd, double min_snr)
{
  const double N = (double)st->n;

  st->pfa_cell  = 1.0 - pow (1.0 - pfa, 1.0 / N);
  st->eta       = (float)det_threshold (st->pfa_cell);
  st->threshold = st->eta * ACQ_SQRT_2_OVER_PI;

  /* Smallest dwell (frames) whose dwell*n coherent samples meet Pd at min_snr.
   * det_pd's dwell argument is the model integration length in samples. */
  size_t d = st->max_dwell;
  for (size_t cand = 1; cand <= st->max_dwell; cand++)
    {
      if (det_pd (min_snr, (int)(cand * st->n), st->eta) >= pd)
        {
          d = cand;
          break;
        }
    }
  st->dwell        = d;
  st->pd_predicted = det_pd (min_snr, (int)(st->dwell * st->n), st->eta);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

acq_state_t *
acq_create (const uint8_t *code, size_t code_len, size_t sf, size_t spc,
            size_t ny, double pfa, double pd, double min_snr, int noise_mode,
            size_t max_dwell)
{
  /* Validate: bad arguments yield NULL (the binding maps this to a clear
   * MemoryError) rather than undefined behaviour downstream. */
  if (!code || code_len != sf || sf < 1 || spc < 1 || ny < 1 || max_dwell < 1
      || !(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0)
      || !(min_snr > 0.0))
    return NULL;

  acq_state_t *st = (acq_state_t *)calloc (1, sizeof (*st));
  if (!st)
    return NULL;

  st->sf         = sf;
  st->spc        = spc;
  st->ny         = ny;
  st->nx         = sf * spc;
  st->n          = ny * st->nx;
  st->max_dwell  = max_dwell;
  st->noise_mode = (det_noise_mode_t)noise_mode;
  st->noise_lo   = 0;
  st->noise_hi   = st->n - 1;

  _auto_config (st, pfa, pd, min_snr);

  const size_t n = st->n;

  /* Single-row oversampled BPSK reference: row 0 carries the replica
   * (chip 0 -> +1, chip 1 -> -1, each held for spc samples), rest zero. */
  st->ref = (float complex *)calloc (n, sizeof (float complex));
  if (!st->ref)
    goto fail;
  for (size_t c = 0; c < sf; c++)
    {
      float sign = (code[c] & 1u) ? -1.0f : 1.0f;
      for (size_t s = 0; s < spc; s++)
        st->ref[c * spc + s] = sign;
    }

  st->corr = corr2d_create (st->ref, ny, st->nx, st->dwell, 1);
  if (!st->corr)
    goto fail;

  st->slow_fft = fft_create (ny, -1, 1);
  if (!st->slow_fft)
    goto fail;

  st->ring = _ring_create (n > 512 ? n : 512);
  if (!st->ring)
    goto fail;
  st->ring_cap = st->ring->capacity;

  st->yframe        = (float complex *)malloc (n * sizeof (float complex));
  st->out_buf       = (float complex *)malloc (n * sizeof (float complex));
  st->colbuf        = (float complex *)malloc (ny * sizeof (float complex));
  st->colout        = (float complex *)malloc (ny * sizeof (float complex));
  st->mag_buf       = (float *)malloc (n * sizeof (float));
  st->noise_scratch = (float *)malloc (n * sizeof (float));
  if (!st->yframe || !st->out_buf || !st->colbuf || !st->colout || !st->mag_buf
      || !st->noise_scratch)
    goto fail;

  return st;

fail:
  acq_destroy (st);
  return NULL;
}

void
acq_destroy (acq_state_t *st)
{
  if (!st)
    return;
  if (st->corr)
    corr2d_destroy (st->corr);
  if (st->slow_fft)
    fft_destroy (st->slow_fft);
  if (st->ring)
    dp_f32_destroy (st->ring);
  free (st->ref);
  free (st->yframe);
  free (st->out_buf);
  free (st->colbuf);
  free (st->colout);
  free (st->mag_buf);
  free (st->noise_scratch);
  free (st);
}

void
acq_reset (acq_state_t *st)
{
  DP_STORE_REL (&st->ring->head, 0);
  DP_STORE_REL (&st->ring->tail, 0);
  corr2d_reset (st->corr);
  st->peak_row = st->peak_col = 0;
  st->peak_mag = st->noise_est = st->test_stat = 0.0f;
}

/* ── Stream push ────────────────────────────────────────────────────────── */

size_t
acq_push (acq_state_t *st, const float complex *in, size_t n_in,
          acq_result_t *result, size_t max_results)
{
  size_t       ndet = 0;
  size_t       off  = 0;
  const size_t n    = st->n;
  const size_t ny   = st->ny;
  const size_t nx   = st->nx;

  while (off < n_in && ndet < max_results)
    {
      size_t head     = DP_LOAD_RLX (&st->ring->head);
      size_t tail     = DP_LOAD_ACQ (&st->ring->tail);
      size_t space    = st->ring->capacity - (head - tail);
      size_t to_write = n_in - off;
      if (to_write > space)
        to_write = space;

      if (to_write > 0)
        {
          dp_f32_write (st->ring, (const float *)(in + off), to_write);
          off += to_write;
        }

      while (ndet < max_results)
        {
          size_t h = DP_LOAD_ACQ (&st->ring->head);
          size_t t = DP_LOAD_RLX (&st->ring->tail);
          if (h - t < n)
            break;

          const float complex *frame
              = (const float complex *)(st->ring->data
                                        + (t & st->ring->mask) * 2);

          /* Slow-time Doppler FFT: FFT along the ny segment axis, per column.
           * Unnormalised (matches numpy fft); corr2d supplies the only 1/n. */
          for (size_t j = 0; j < nx; j++)
            {
              for (size_t i = 0; i < ny; i++)
                st->colbuf[i] = frame[i * nx + j];
              fft_execute_cf32 (st->slow_fft, st->colbuf, ny, st->colout);
              for (size_t i = 0; i < ny; i++)
                st->yframe[i * nx + j] = st->colout[i];
            }
          dp_f32_consume (st->ring, n);

          size_t n_out = corr2d_execute (st->corr, st->yframe, n, st->out_buf);
          if (n_out == 0)
            continue; /* mid-dwell — accumulating, no dump yet */

          _compute_stat (st);

          if (st->test_stat > st->threshold)
            {
              float snr_est = st->test_stat / ACQ_SQRT_2_OVER_PI
                              / sqrtf (2.0f * (float)(st->dwell * n));
              result[ndet++]
                  = (acq_result_t){ st->peak_row,  st->peak_col,  st->peak_mag,
                                    st->noise_est, st->test_stat, snr_est };
            }
        }

      if (to_write == 0)
        break;
    }

  return ndet;
}
