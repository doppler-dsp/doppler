#include "burst_demod/burst_demod_core.h"

#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BURST_DEMOD_CRC_BITS 16 /* CRC-16-CCITT trailer */
#define BURST_DEMOD_EST_ITERS 4 /* preamble estimate refinement passes */

/* PN chip / BPSK bit sign: 0 -> +1, 1 -> -1. */
static inline float
chip_sign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/* Wrap a phase (radians) to [-pi, pi] for cexpf accuracy. */
static inline double
wrap_pi (double ph)
{
  return ph - 2.0 * M_PI * round (ph / (2.0 * M_PI));
}

/* CRC-16-CCITT (poly 0x1021, init 0xFFFF) over a bit stream, MSB-first. */
static uint16_t
crc16_ccitt (const uint8_t *bits, size_t n)
{
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      crc ^= (uint16_t)((bits[i] & 1u) << 15);
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
    }
  return crc;
}

burst_demod_state_t *
burst_demod_create (const uint8_t *data_code, size_t data_code_len, size_t spc,
                    double chip_rate, double carrier_hz, double max_rate,
                    size_t payload_len, size_t est_segments)
{
  if (!data_code || data_code_len == 0 || spc == 0 || chip_rate <= 0.0
      || max_rate < 0.0 || est_segments == 0)
    return NULL;
  burst_demod_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  s->data_code = malloc (data_code_len);
  if (!s->data_code)
    {
      free (s);
      return NULL;
    }
  memcpy (s->data_code, data_code, data_code_len);
  s->data_sf      = data_code_len;
  s->spc          = spc;
  s->chip_rate    = chip_rate;
  s->carrier_hz   = carrier_hz;
  s->max_rate     = max_rate;
  s->payload_len  = payload_len;
  s->est_segments = est_segments;
  return s;
}

void
burst_demod_destroy (burst_demod_state_t *s)
{
  if (!s)
    return;
  if (s->ppe)
    ppe_destroy (s->ppe);
  free (s->data_code);
  free (s->acq_code);
  free (s->sync);
  free (s->part);
  free (s);
}

void
burst_demod_reset (burst_demod_state_t *s)
{
  s->frame_valid  = 0;
  s->frame_offset = 0;
  s->n_symbols    = 0;
  s->est_freq_hz  = 0.0;
  s->est_rate_hz  = 0.0;
  s->est_snr_db   = 0.0;
}

void
burst_demod_set_preamble (burst_demod_state_t *s, const uint8_t *acq_code,
                          size_t acq_code_len, size_t reps)
{
  if (!acq_code || acq_code_len == 0 || reps == 0)
    return;
  free (s->acq_code);
  s->acq_code = malloc (acq_code_len);
  if (!s->acq_code)
    return;
  memcpy (s->acq_code, acq_code, acq_code_len);
  s->acq_sf   = acq_code_len;
  s->acq_reps = reps;

  s->n_part = reps * s->est_segments; /* one partial per segment per period */
  free (s->part);
  s->part = malloc (s->n_part * sizeof (float complex));

  /* Size ppe's rate search in PARTIAL units: a partial spans Lseg samples, so
   * a physical rate of max_rate cyc/sample^2 maps to max_rate*Lseg^2 per
   * partial.*/
  size_t lseg_chips = s->acq_sf / s->est_segments;
  if (lseg_chips == 0)
    lseg_chips = 1;
  double lseg         = (double)(lseg_chips * s->spc);
  double ppe_max_rate = s->max_rate * lseg * lseg;
  if (s->ppe)
    ppe_destroy (s->ppe);
  s->ppe = ppe_create (s->n_part, ppe_max_rate);
}

void
burst_demod_set_sync (burst_demod_state_t *s, const uint8_t *sync,
                      size_t sync_len)
{
  if (!sync || sync_len == 0)
    return;
  free (s->sync);
  s->sync = malloc (sync_len * sizeof (int8_t));
  if (!s->sync)
    return;
  for (size_t i = 0; i < sync_len; i++)
    s->sync[i] = (sync[i] & 1u) ? -1 : 1; /* 0 -> +1, 1 -> -1 */
  s->sync_len = sync_len;
}

void
burst_demod_set_prior (burst_demod_state_t *s, double f0_coarse, size_t start)
{
  s->f0_prior = f0_coarse;
  s->start    = start;
}

size_t
burst_demod_demod_max_out (burst_demod_state_t *s)
{
  return s->payload_len;
}

/* Dechirp the unmodulated preamble by (f0, mu) and PN-wipe it into one partial
 * correlation per segment — a short complex sequence whose residual chirp ppe
 * estimates.  Iterating with the running (f0, mu) drives the residual to zero.
 */
static void
form_partials (const burst_demod_state_t *s, const float complex *x, double f0,
               double mu, size_t lseg_chips, float complex *part)
{
  const size_t npre = s->acq_sf * s->acq_reps * s->spc;
  for (size_t m = 0; m < s->n_part; m++)
    part[m] = 0.0f;
  for (size_t n = 0; n < npre; n++)
    {
      size_t chip  = n / s->spc;
      size_t inper = chip % s->acq_sf;
      size_t rep   = chip / s->acq_sf;
      size_t seg   = inper / lseg_chips;
      if (seg >= s->est_segments)
        seg = s->est_segments - 1;
      size_t m  = rep * s->est_segments + seg;
      double ph = wrap_pi (
          -2.0 * M_PI * (f0 * (double)n + 0.5 * mu * (double)n * (double)n));
      part[m] += x[s->start + n] * cexpf ((float)ph * I)
                 * chip_sign (s->acq_code[inper]);
    }
}

/* Dechirp the data section by (f0, mu) and prompt-despread to soft BPSK
 * symbols (one per data code period). Returns the symbol count (<= cap). */
static size_t
despread_data (const burst_demod_state_t *s, const float complex *x,
               size_t x_len, size_t data0, size_t npre, double f0, double mu,
               float complex *sym, size_t cap)
{
  const size_t ndata = x_len - data0;
  const size_t tsym  = s->data_sf * s->spc;
  const double fs    = s->chip_rate * (double)s->spc;
  /* Bulk code-Doppler: the chip clock stretches by the Doppler fraction. */
  double code_rate
      = (s->carrier_hz > 0.0) ? 1.0 + (f0 * fs) / s->carrier_hz : 1.0;
  const double  inv_spc = code_rate / (double)s->spc;
  size_t        nsym    = 0;
  double        cp      = 0.0;
  float complex acc     = 0.0f;
  for (size_t i = 0; i < ndata && nsym < cap; i++)
    {
      double nrel = (double)(npre + i); /* sample index from preamble start */
      double ph = wrap_pi (-2.0 * M_PI * (f0 * nrel + 0.5 * mu * nrel * nrel));
      float complex d  = x[data0 + i] * cexpf ((float)ph * I);
      size_t        pj = (size_t)cp;
      if (pj >= s->data_sf)
        pj = s->data_sf - 1;
      acc += d * chip_sign (s->data_code[pj]);
      cp += inv_spc;
      if (cp >= (double)s->data_sf)
        {
          sym[nsym++] = acc / (float)tsym;
          acc         = 0.0f;
          cp -= (double)s->data_sf;
        }
    }
  return nsym;
}

size_t
burst_demod_demod (burst_demod_state_t *s, const float complex *x,
                   size_t x_len, uint8_t *out, size_t max_out)
{
  burst_demod_reset (s);
  if (!s->ppe || !s->sync || s->payload_len == 0)
    return 0;

  const size_t npre  = s->acq_sf * s->acq_reps * s->spc;
  const size_t data0 = s->start + npre;
  if (data0 >= x_len)
    return 0;
  const double fs         = s->chip_rate * (double)s->spc;
  size_t       lseg_chips = s->acq_sf / s->est_segments;
  if (lseg_chips == 0)
    lseg_chips = 1;
  const double lseg = (double)(lseg_chips * s->spc);

  /* ── 1) Feedforward estimate from the unmodulated preamble, iterated. ─────
   * Data-aided (the preamble is known), so no squaring ambiguity: re-dechirp
   * by the running (f0, mu) and re-estimate; the residual collapses toward DC,
   * removing the freq/rate coupling bias of a single pass. */
  double       f0 = s->f0_prior, mu = 0.0;
  ppe_result_t est = { 0.0, 0.0, 0.0 };
  for (int it = 0; it < BURST_DEMOD_EST_ITERS; it++)
    {
      form_partials (s, x, f0, mu, lseg_chips, s->part);
      est = ppe_estimate (s->ppe, s->part, s->n_part);
      f0 += est.freq_norm / lseg;
      mu += est.rate_norm / (lseg * lseg);
    }
  s->est_freq_hz = f0 * fs;
  s->est_rate_hz = mu * fs * fs;
  s->est_snr_db  = est.snr_db;

  /* ── 2) Dechirp + despread (coarse), then NDA-refine the RATE over the long
   * data-symbol baseline and despread again. (Frequency stays from the
   * iterated preamble — squaring folds it with a half-cycle ambiguity.) ─────
   */
  const size_t   tsym     = s->data_sf * s->spc;
  const size_t   nsym_max = (x_len - data0) / tsym + 1;
  float complex *sym      = malloc (nsym_max * sizeof (float complex));
  float complex *sym2     = malloc (nsym_max * sizeof (float complex));
  if (!sym || !sym2)
    {
      free (sym);
      free (sym2);
      return 0;
    }
  size_t nsym
      = despread_data (s, x, x_len, data0, npre, f0, mu, sym, nsym_max);
  if (s->max_rate > 0.0 && nsym >= 8)
    {
      for (size_t k = 0; k < nsym; k++)
        sym2[k] = sym[k] * sym[k];
      double       t  = (double)tsym;
      double       rm = fmin (s->max_rate * t * t * 2.0, 0.02);
      ppe_state_t *pr = ppe_create (nsym, rm);
      if (pr)
        {
          ppe_result_t e2 = ppe_estimate (pr, sym2, nsym);
          mu += (e2.rate_norm * 0.5) / (t * t); /* squared -> halve */
          ppe_destroy (pr);
          nsym = despread_data (s, x, x_len, data0, npre, f0, mu, sym,
                                nsym_max);
          s->est_rate_hz = mu * fs * fs;
        }
    }
  free (sym2);
  s->n_symbols = nsym;

  /* ── 3) Frame sync: the sync word's complex correlation peak gives the
   * offset and the residual phase (which also resolves the BPSK sign). ──────
   */
  const size_t frame = s->sync_len + s->payload_len + BURST_DEMOD_CRC_BITS;
  if (nsym < frame)
    {
      free (sym);
      return 0;
    }
  size_t        best_off = 0;
  double        best_mag = -1.0;
  float complex best_c   = 0.0f;
  for (size_t off = 0; off + frame <= nsym; off++)
    {
      float complex c = 0.0f;
      for (size_t j = 0; j < s->sync_len; j++)
        c += sym[off + j] * (float)s->sync[j];
      double mag
          = (double)crealf (c) * crealf (c) + (double)cimagf (c) * cimagf (c);
      if (mag > best_mag)
        {
          best_mag = mag;
          best_off = off;
          best_c   = c;
        }
    }
  s->frame_offset = best_off;
  double theta    = atan2 ((double)cimagf (best_c), (double)crealf (best_c));
  float complex derot = cexpf (-(float)theta * I);

  /* ── 4) Slice the payload, recompute + check the CRC-16 trailer. ─────────
   */
  const size_t pstart = best_off + s->sync_len;
  size_t       nbits  = (s->payload_len <= max_out) ? s->payload_len : max_out;
  for (size_t k = 0; k < nbits; k++)
    out[k] = (crealf (sym[pstart + k] * derot) < 0.0f) ? 1u : 0u;

  uint8_t *pbits  = malloc (s->payload_len);
  uint16_t rx_crc = 0;
  if (pbits)
    {
      for (size_t k = 0; k < s->payload_len; k++)
        pbits[k] = (crealf (sym[pstart + k] * derot) < 0.0f) ? 1u : 0u;
      const size_t cstart = pstart + s->payload_len;
      for (size_t j = 0; j < BURST_DEMOD_CRC_BITS; j++)
        {
          uint16_t b = (crealf (sym[cstart + j] * derot) < 0.0f) ? 1u : 0u;
          rx_crc |= (uint16_t)(b << (BURST_DEMOD_CRC_BITS - 1 - j));
        }
      s->frame_valid = (crc16_ccitt (pbits, s->payload_len) == rx_crc) ? 1 : 0;
      free (pbits);
    }

  free (sym);
  return nbits;
}
