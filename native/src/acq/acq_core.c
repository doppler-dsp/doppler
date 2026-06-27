/**
 * @file acq_core.c
 * @brief Streaming DSSS burst-acquisition engine implementation.
 *
 * Owns the full receive-side acquisition pipeline: a raw cf32 ring, the
 * slow-time Doppler FFT, a single-row-reference 2-D code correlator (corr2d),
 * and a CFAR threshold gate whose grid (coherent depth, threshold,
 * non-coherent looks) is auto-configured from the physics — C/N0, chip rate,
 * and a target (Pfa, Pd) — using the detection-theory functions.  The CFAR
 * helpers
 * (_noise_estimate, _ring_create) are reused header-only from det_private.h —
 * no link to detector_core.
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

/* Doppler-band mask: with a doppler_uncertainty prior the engine scans only
 * searched_bins rows centred on DC, so the peak search must match (else the
 * lowered Bonferroni threshold over-counts and realized Pfa exceeds target).
 * Returns 1 if the cell at flat index k is inside the scanned band.  The
 * folded distance |k_row| from DC (rows >doppler_bins/2 are negative Doppler)
 * must not exceed half = (searched_bins-1)/2.  When searched_bins ==
 * doppler_bins this admits every row (byte-identical to the full search). */
static inline int
_in_doppler_band (const acq_state_t *st, size_t k)
{
  const size_t row = k / st->code_bins;
  const size_t fold
      = (row <= st->doppler_bins - row) ? row : st->doppler_bins - row;
  return fold <= (st->searched_bins - 1) / 2;
}

/* Peak, CFAR noise, and test statistic from the coherent dump in out_buf. */
static void
_compute_stat (acq_state_t *st)
{
  const size_t n = st->n;

  for (size_t k = 0; k < n; k++)
    st->mag_buf[k] = cabsf (st->out_buf[k]);

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (st->mag_buf[k] > st->mag_buf[peak] && _in_doppler_band (st, k))
      peak = k;

  st->peak_row = peak / st->code_bins;
  st->peak_col = peak % st->code_bins;
  st->peak_mag = st->mag_buf[peak];

  st->noise_est = _noise_estimate (st->mag_buf, st->noise_lo, st->noise_hi,
                                   st->noise_scratch, st->noise_mode);

  st->test_stat
      = (st->noise_est > 0.0f) ? (st->peak_mag / st->noise_est) : 0.0f;
}

/* Non-coherent CFAR: peak + normalized order-N_nc statistic from nc_surface
 * (the per-cell sum of |dump|^2 over n_noncoh coherent looks).
 *
 * Under H0 each look's |z|^2 = sigma^2 * chi2(2)/2, so nc_surface ~
 * (sigma^2/2)*chi2(2*n_noncoh).  Estimating the per-look power sigma^2 from
 * the mean reference-cell power (noise_pow / n_noncoh), the normalized
 * statistic R = sqrt(2 * n_noncoh * peak / noise_pow) has P(R > b) =
 * marcum_q(n_noncoh, 0, b) under H0 — exactly the order-N_nc threshold eta_nc.
 * Validated against Monte-Carlo to <1% (det_pd_noncoherent tests). */
static void
_compute_stat_nc (acq_state_t *st)
{
  const size_t n = st->n;

  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (st->nc_surface[k] > st->nc_surface[peak] && _in_doppler_band (st, k))
      peak = k;

  st->peak_row = peak / st->code_bins;
  st->peak_col = peak % st->code_bins;
  st->peak_mag = sqrtf (st->nc_surface[peak]);

  float noise_pow
      = _noise_estimate (st->nc_surface, st->noise_lo, st->noise_hi,
                         st->noise_scratch, st->noise_mode);
  st->noise_est = sqrtf (noise_pow);
  st->test_stat = (noise_pow > 0.0f)
                      ? sqrtf (2.0f * (float)st->n_noncoh
                               * st->nc_surface[peak] / noise_pow)
                      : 0.0f;
}

/* Scanned Doppler bins for a coherent depth D under a doppler_uncertainty
 * prior du (Hz, one-sided half-range).  The slow-time FFT always spans the
 * full +/- span = chip_rate/(2*sf); a tighter du restricts the search to the
 * central band, shrinking the Bonferroni cell count.  Returns an odd count
 * (2*half+1) centred on DC, capped at D; du <= 0 (or >= span) means full. */
static size_t
_searched_bins (size_t D, double du, double span)
{
  if (du <= 0.0 || du >= span)
    return D;
  double frac = du / span; /* fraction of half-range */
  size_t half = (size_t)ceil (0.5 * frac * (double)D);
  size_t sb   = 2 * half + 1;
  return (sb > D) ? D : sb;
}

/* Size the search grid from the physics.  Picks the smallest coherent depth
 * doppler_bins in [1, reps] whose doppler_bins*code_bins coherent samples meet
 * Pd at the (doppler_uncertainty-shrunk) Bonferroni threshold; if the full
 * coherent ceiling still falls short, adds non-coherent looks (up to
 * max_noncoh).  Commits doppler_bins / n / searched_bins / threshold ladder.
 */
static void
_auto_config (acq_state_t *st, double pfa, double pd, double snr, double du)
{
  const size_t cb   = st->code_bins;
  const double span = st->doppler_span_hz;

  /* Smallest coherent depth D meeting Pd (minimum latency for strong signals);
   * Bonferroni uses only the cells actually scanned at that depth. */
  size_t best_d    = st->reps;
  int    coh_meets = 0;
  for (size_t D = 1; D <= st->reps; D++)
    {
      size_t sb  = _searched_bins (D, du, span);
      double pc  = 1.0 - pow (1.0 - pfa, 1.0 / (double)(sb * cb));
      double eta = det_threshold (pc);
      if (det_pd (snr, (int)(D * cb), eta) >= pd)
        {
          best_d    = D;
          coh_meets = 1;
          break;
        }
    }

  st->doppler_bins   = best_d;
  st->n              = best_d * cb;
  st->searched_bins  = _searched_bins (best_d, du, span);
  st->doppler_res_hz = st->chip_rate / ((double)st->sf * (double)best_d);
  st->pfa_cell = 1.0 - pow (1.0 - pfa, 1.0 / (double)(st->searched_bins * cb));
  st->eta      = (float)det_threshold (st->pfa_cell);

  /* Non-coherent looks only if the coherent ceiling fell short and the caller
   * raised the cap above 1 (best effort if even max_noncoh is infeasible). */
  size_t nc = 1;
  if (!coh_meets && st->max_noncoh > 1)
    {
      int k = det_n_noncoh (snr, (int)st->n, pd, st->pfa_cell,
                            (int)st->max_noncoh);
      nc    = (k > 0) ? (size_t)k : st->max_noncoh;
    }
  st->n_noncoh = nc;

  if (nc > 1)
    {
      double e      = det_threshold_noncoherent (st->pfa_cell, (int)nc);
      st->eta_nc    = (float)e;
      st->threshold = 0.0f; /* coherent gate unused on the non-coherent path */
      st->pd_predicted = det_pd_noncoherent (snr, (int)st->n, (int)nc, e);
    }
  else
    {
      st->eta_nc       = 0.0f;
      st->threshold    = st->eta * ACQ_SQRT_2_OVER_PI;
      st->pd_predicted = det_pd (snr, (int)st->n, st->eta);
    }

  st->underpowered = (uint8_t)(st->pd_predicted < pd);
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

acq_state_t *
acq_create (const uint8_t *code, size_t code_len, size_t reps, size_t spc,
            double chip_rate, double cn0_dbhz, double doppler_uncertainty,
            double pfa, double pd, int noise_mode, size_t max_noncoh)
{
  /* Validate: bad arguments yield NULL (the binding maps this to a clear
   * MemoryError) rather than undefined behaviour downstream.  chip_rate /
   * cn0_dbhz > 0 act as the required sentinels (their toml placeholder
   * defaults are valid, but an explicit 0 is rejected). */
  if (max_noncoh < 1)
    max_noncoh = 1;
  const size_t sf   = code_len; /* sf is inferred from the code length */
  const double span = (sf > 0) ? chip_rate / (2.0 * (double)sf) : 0.0;
  if (!code || code_len < 1 || spc < 1 || reps < 1 || !(chip_rate > 0.0)
      || !(cn0_dbhz > 0.0) || !isfinite (cn0_dbhz) || !(pfa > 0.0 && pfa < 1.0)
      || !(pd > 0.0 && pd < 1.0) || doppler_uncertainty < 0.0
      || doppler_uncertainty > span)
    return NULL;

  acq_state_t *st = (acq_state_t *)calloc (1, sizeof (*st));
  if (!st)
    return NULL;

  st->sf              = sf;
  st->spc             = spc;
  st->reps            = reps;
  st->code_bins       = sf * spc;
  st->chip_rate       = chip_rate;
  st->fs              = chip_rate * (double)spc;
  st->cn0_dbhz        = cn0_dbhz;
  st->doppler_span_hz = span;
  st->pd              = pd;
  st->max_noncoh      = max_noncoh;
  st->noise_mode      = (det_noise_mode_t)noise_mode;

  /* C/N0 (dB-Hz) -> per-sample amplitude SNR: power SNR = (C/N0)/fs, and the
   * detection model's non-centrality is a = sqrt(2M)*snr (amplitude). */
  const double snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / st->fs);
  _auto_config (st, pfa, pd, snr, doppler_uncertainty);

  const size_t n  = st->n; /* doppler_bins * code_bins (post-config) */
  const size_t db = st->doppler_bins;
  st->noise_lo    = 0;
  st->noise_hi    = n - 1;

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

  st->corr = corr2d_create (st->ref, db, st->code_bins, 1, 1, 0, 0);
  if (!st->corr)
    goto fail;

  st->slow_fft = fft_create (db, -1, 1);
  if (!st->slow_fft)
    goto fail;

  st->ring = _ring_create (n > 512 ? n : 512);
  if (!st->ring)
    goto fail;
  st->ring_cap = st->ring->capacity;

  st->yframe        = (float complex *)malloc (n * sizeof (float complex));
  st->out_buf       = (float complex *)malloc (n * sizeof (float complex));
  st->colbuf        = (float complex *)malloc (db * sizeof (float complex));
  st->colout        = (float complex *)malloc (db * sizeof (float complex));
  st->mag_buf       = (float *)malloc (n * sizeof (float));
  st->noise_scratch = (float *)malloc (n * sizeof (float));
  if (!st->yframe || !st->out_buf || !st->colbuf || !st->colout || !st->mag_buf
      || !st->noise_scratch)
    goto fail;

  /* Non-coherent power accumulator — only on the N_nc > 1 path. */
  if (st->n_noncoh > 1)
    {
      st->nc_surface = (float *)calloc (n, sizeof (float));
      if (!st->nc_surface)
        goto fail;
    }

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
  free (st->nc_surface);
  free (st);
}

void
acq_reset (acq_state_t *st)
{
  DP_STORE_REL (&st->ring->head, 0);
  DP_STORE_REL (&st->ring->tail, 0);
  corr2d_reset (st->corr);
  if (st->nc_surface)
    memset (st->nc_surface, 0, st->n * sizeof (float));
  st->nc_count         = 0;
  st->samples_consumed = 0;
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
  const size_t ny   = st->doppler_bins;
  const size_t nx   = st->code_bins;

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
          st->samples_consumed += n;

          size_t n_out = corr2d_execute (st->corr, st->yframe, n, st->out_buf);
          if (n_out == 0)
            continue; /* mid-dwell — coherent accumulation, no dump yet */

          if (st->n_noncoh <= 1)
            {
              /* Coherent path (unchanged): amplitude mean-CFAR per dump. */
              _compute_stat (st);
              if (st->test_stat > st->threshold)
                {
                  float snr_est = st->test_stat / ACQ_SQRT_2_OVER_PI
                                  / sqrtf (2.0f * (float)n);
                  result[ndet++]
                      = (acq_result_t){ st->peak_row,  st->peak_col,
                                        st->peak_mag,  st->noise_est,
                                        st->test_stat, snr_est };
                }
              continue;
            }

          /* Non-coherent path: magnitude-square accumulate each coherent look;
           * gate the order-N_nc statistic once n_noncoh looks are in. */
          for (size_t k = 0; k < n; k++)
            {
              float m = cabsf (st->out_buf[k]);
              st->nc_surface[k] += m * m;
            }
          if (++st->nc_count < st->n_noncoh)
            continue; /* still accumulating looks */

          _compute_stat_nc (st);
          if (st->test_stat > st->eta_nc)
            {
              float snr_est = st->test_stat
                              / sqrtf (2.0f * (float)n * (float)st->n_noncoh);
              result[ndet++]
                  = (acq_result_t){ st->peak_row,  st->peak_col,  st->peak_mag,
                                    st->noise_est, st->test_stat, snr_est };
            }
          memset (st->nc_surface, 0, n * sizeof (float));
          st->nc_count = 0;
        }

      if (to_write == 0)
        break;
    }

  return ndet;
}

/* ── Serializable state — the pure-transducer face ─────────────────────────
 *
 * Fixed flat layout (offsets depend only on the ring capacity), so the state
 * blob is portable POD:
 *   [hdr][ float complex unconsumed[ring_cap] ][ float nc_surface[n] (if nc) ]
 * Only the first hdr.n_unconsumed of the unconsumed region holds data; that
 * may exceed n (undrained full frames from a max_results-saturated run,
 * preserved so the resume processes them).
 */

/* Offset to the ring-samples region: standard envelope + acq's extra header.
 */
#define ACQ_BODY_OFF (sizeof (dp_state_hdr_t) + sizeof (acq_extra_t))

static float complex *
_state_samples (void *blob)
{
  return (float complex *)((char *)blob + ACQ_BODY_OFF);
}

static float *
_state_nc (void *blob, size_t ring_cap)
{
  return (float *)((char *)blob + ACQ_BODY_OFF
                   + ring_cap * sizeof (float complex));
}

size_t
acq_state_bytes (const acq_state_t *st)
{
  size_t b = ACQ_BODY_OFF + st->ring_cap * sizeof (float complex);
  if (st->n_noncoh > 1)
    b += st->n * sizeof (float);
  return b;
}

void
acq_get_state (const acq_state_t *st, void *blob)
{
  const size_t n   = st->n;
  size_t       h   = DP_LOAD_ACQ (&st->ring->head);
  size_t       t   = DP_LOAD_RLX (&st->ring->tail);
  size_t       nun = h - t;

  dp_writer_t w = dp_writer_init (blob, acq_state_bytes (st));
  dp_w_hdr (&w, ACQ_STATE_MAGIC, ACQ_STATE_VERSION, acq_state_bytes (st));
  acq_extra_t ex = { .has_nc           = (uint16_t)(st->n_noncoh > 1),
                     ._pad             = 0,
                     .n_noncoh         = (uint32_t)st->n_noncoh,
                     .n                = (uint64_t)n,
                     .samples_consumed = st->samples_consumed,
                     .nc_count         = (uint32_t)st->nc_count,
                     .n_unconsumed     = (uint32_t)nun };
  dp_w_bytes (&w, &ex, sizeof ex);

  float complex *dst = _state_samples (blob);
  for (size_t i = 0; i < nun; i++)
    {
      size_t idx = (t + i) & st->ring->mask;
      dst[i]     = st->ring->data[idx * 2] + I * st->ring->data[idx * 2 + 1];
    }
  if (ex.has_nc)
    memcpy (_state_nc (blob, st->ring_cap), st->nc_surface,
            n * sizeof (float));
}

int
acq_set_state (acq_state_t *st, const void *blob)
{
  int rc = dp_state_validate (blob, acq_state_bytes (st), ACQ_STATE_MAGIC,
                              ACQ_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  acq_extra_t ex;
  memcpy (&ex, (const char *)blob + sizeof (dp_state_hdr_t), sizeof ex);
  if (ex.n != (uint64_t)st->n || ex.n_noncoh != (uint32_t)st->n_noncoh
      || ex.n_unconsumed > (uint32_t)st->ring_cap)
    return DP_ERR_INVALID;

  /* Reset the live state, then replay the blob's buffered samples + nc. */
  DP_STORE_REL (&st->ring->head, 0);
  DP_STORE_REL (&st->ring->tail, 0);
  corr2d_reset (st->corr);
  st->samples_consumed = ex.samples_consumed;
  st->nc_count         = ex.nc_count;

  const float complex *src = _state_samples ((void *)blob);
  if (ex.n_unconsumed > 0)
    dp_f32_write (st->ring, (const float *)src, ex.n_unconsumed);

  if (st->nc_surface)
    {
      if (ex.has_nc)
        memcpy (st->nc_surface, _state_nc ((void *)blob, st->ring_cap),
                st->n * sizeof (float));
      else
        memset (st->nc_surface, 0, st->n * sizeof (float));
    }
  return DP_OK;
}

size_t
acq_run (acq_state_t *st, const void *state_in, void *state_out,
         const float complex *in, size_t n_in, acq_result_t *result,
         size_t max_results)
{
  if (state_in)
    {
      if (acq_set_state (st, state_in) != 0)
        return 0;
    }
  else
    acq_reset (st);

  size_t ndet = acq_push (st, in, n_in, result, max_results);

  if (state_out)
    acq_get_state (st, state_out);
  return ndet;
}
