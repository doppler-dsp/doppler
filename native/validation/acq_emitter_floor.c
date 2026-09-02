/**
 * @file acq_emitter_floor.c
 * @brief What ONE emitter puts on the (tile x code phase) surface, and what
 *        the CFAR reference makes of it.
 *
 * The continuous async-DSSS design (docs/design/async-dsss-receiver.md
 * §6.3) forks on one number: how far below a strong emitter's peak the rest
 * of the surface sits. Inside that floor a second emitter is a second peak
 * and a peak list finds it; below it the weak emitter is under the strong
 * one's sidelobes and only cancellation reaches it. The maintainer's figure
 * is the Gold code's three-valued bound, 65/1023 = -23.9 dB -- but that is
 * the full-period, zero-Doppler autocorrelation, and the surface the
 * searcher actually gates on is 21 or 53 roll-FFT tiles of it, with the
 * emitter possibly half a tile off centre and possibly carrying a data
 * transition inside the epoch. This harness measures the surface the engine
 * really computes, through the engine, and reads the CFAR reference the
 * engine really forms from it.
 *
 * Method. A continuous engine at the operating point (one epoch per look,
 * forced by a sizing C/N0 high enough that n_noncoh == 1, asserted). One
 * emitter from the shipped synth (`wfm_synth`, continuous DSSS: the same
 * generator wfmgen renders the waveform with -- nothing here builds a chip
 * or a tone by hand), at tile w and code phase tau0, clean. After
 * the push that completes the dwell, the engine's own surface (`mag_buf`,
 * the (window_bins x code_bins) grid) is read back and every cell outside
 * the exclusion zone -- one tile either side in Doppler, one chip either
 * side in code phase -- is binned by tile distance from the peak. Per bin:
 * the maximum and the RMS, in dB below the peak. Beside it, `noise_est` --
 * the reference the CFAR gate divides by -- in dB below the peak, which is
 * the number that decides whether a weaker emitter is above threshold at
 * all.
 *
 * Axes: chip rate (5 and 2 Mcps, the ends of the range: 21 and 53 tiles);
 * the emitter's position within its tile (centre, and half a tile off --
 * the worst straddle); a data transition mid-epoch (none, and one every
 * epoch); and, last, the reference with noise at a design C/N0 with and
 * without the emitter present -- the rise a strong emitter imposes on
 * everything weaker.
 *
 * Usage:
 *   validate_acq_emitter_floor            full table
 *   validate_acq_emitter_floor --check    spot check: the zero-Doppler,
 *                                         centred, data-free same-tile
 *                                         maximum is the Gold bound
 */
#include "acq/acq_core.h"
#include "gold/gold_core.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define NX (SF * SPC) /* code_bins = 2046 */
#define SYMBOL_RATE 2700.0
#define DU 50000.0      /* +/-50 kHz, the design's starting uncertainty  */
#define SIZING_CN0 60.0 /* high enough that the sizer picks n_noncoh = 1 */
#define PFA 1e-3
#define PD 0.9
#define TAU0 777u /* injected code phase, samples                   */
#define TILE 5u   /* injected tile (window hypothesis)              */
#define NBIN 4    /* tile-distance bins: 0, 1, 2, >=3                */

#define GOLD_BOUND_DB (20.0 * log10 (65.0 / 1023.0)) /* -23.94 dB */

static double
db (double lin)
{
  return 20.0 * log10 (lin > 0.0 ? lin : 1e-30);
}

typedef struct
{
  size_t peak_row, peak_col; /* where the engine put the peak           */
  double peak_mag;
  double noise_est; /* the CFAR reference the engine formed    */
  double test_stat;
  double max_db[NBIN]; /* max off-zone cell per tile distance, dB */
  double rms_db[NBIN]; /* RMS off-zone cell per tile distance, dB */
  double max_all_db;   /* max over every off-zone cell            */
  double max_other_db; /* max over cells NOT at the peak's code
                          phase (|dc| > one chip): the floor a
                          second emitter at another code phase
                          actually competes with                    */
  size_t n_noncoh;
  size_t window_bins;
} floor_t;

/* Build one dwell of an emitter and push it; read the surface back. The
 * emitter is at tile TILE plus `frac` of a tile, code phase TAU0, unit
 * amplitude, with an optional sign flip at mid-epoch every epoch (the async
 * data's worst case) and optional noise at cn0_dbhz (0 = none). `present`
 * = 0 pushes the noise alone, for the reference-rise row. */
static int
measure (const uint8_t *code, double chip_rate, double frac, int data_flip,
         double cn0_dbhz, int present, uint32_t seed, floor_t *out)
{
  acq_state_t *a = acq_create_continuous (
      code, SF, SPC, chip_rate, SYMBOL_RATE, SIZING_CN0, DU, PFA, PD, 0);
  if (!a)
    {
      fprintf (stderr, "acq_create_continuous failed\n");
      return 1;
    }
  out->n_noncoh    = a->n_noncoh;
  out->window_bins = a->window_bins;
  if (a->n_noncoh != 1 || a->coherent_bins != 1 || a->interp != 1)
    {
      fprintf (stderr,
               "engine sized n_noncoh=%zu coherent_bins=%zu interp=%zu; this "
               "harness reads the single-look surface and needs 1/1/1\n",
               a->n_noncoh, a->coherent_bins, a->interp);
      acq_destroy (a);
      return 1;
    }
  const size_t nx = a->code_bins;
  const size_t W  = a->window_bins;
  const double fs = chip_rate * (double)SPC;

  /* Tile TILE's own frequency plus a fraction of a tile: the same fold the
     engine reports rows on, so the injected row and the read row agree. */
  const long   r      = dp_fftfreq_index (TILE, W);
  const double f_norm = ((double)r + frac) / (double)nx;

  /* The emitter, rendered by the shipped synth. `snr` is over fs; a C/N0
     converts to it in one place (wfm_snr_over_fs is the composer's; the
     synth takes the over-fs value directly). Clean means no AWGN child. */
  const double snr_fs
      = cn0_dbhz > 0.0 ? cn0_dbhz - 10.0 * log10 (fs) : WFM_SYNTH_SNR_CLEAN;
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, fs, f_norm * fs, snr_fs, 1, seed,
                          (int)SPC, 7, 0, 0, 0.0);
  /* No data: one bit per code period (the pure code). A transition at
     mid-epoch, every epoch: the pattern {0,1} at two symbols per period. */
  static const uint8_t two_bits[2] = { 0, 1 };
  int rc = data_flip
               ? wfm_synth_set_dsss_cont (syn, code, SF, (double)SF / 2.0,
                                          WFM_DSSS_DATA_BITS, two_bits, 2)
               : wfm_synth_set_dsss_cont (syn, code, SF, (double)SF,
                                          WFM_DSSS_DATA_NONE, NULL, 0);
  if (!syn || rc != 0)
    {
      fprintf (stderr, "wfm_synth continuous DSSS setup failed\n");
      acq_destroy (a);
      wfm_synth_destroy (syn);
      return 1;
    }
  /* Code phase tau0: render tau0 extra samples and start the epoch there.
     `present` = 0 takes the synth's noise term alone, the same call the
     composer uses to carry a noise floor through a gap. */
  float complex *raw = malloc ((TAU0 + nx) * sizeof *raw);
  if (present)
    wfm_synth_steps (syn, raw, TAU0 + nx);
  else
    wfm_synth_noise_steps (syn, raw, TAU0 + nx);
  float complex *buf = raw + TAU0;

  acq_result_t hit[2];
  (void)acq_push (a, buf, nx, hit, 2);

  /* The engine's surface, as gated on. */
  const float *m  = a->mag_buf;
  size_t       pk = 0;
  for (size_t k = 1; k < a->n_surf; k++)
    if (m[k] > m[pk])
      pk = k;
  out->peak_row  = pk / nx;
  out->peak_col  = pk % nx;
  out->peak_mag  = m[pk];
  out->noise_est = a->noise_est;
  out->test_stat = a->test_stat;

  double mx[NBIN] = { 0 }, ss[NBIN] = { 0 };
  size_t cnt[NBIN] = { 0 };
  double mx_all    = 0.0;
  double mx_other  = 0.0;
  for (size_t k = 0; k < a->n_surf; k++)
    {
      size_t row = k / nx, col = k % nx;
      long   dr = (long)row - (long)out->peak_row;
      if (dr < 0)
        dr = -dr;
      long dc = (long)col - (long)out->peak_col;
      if (dc < 0)
        dc = -dc;
      if ((size_t)dc > nx / 2)
        dc = (long)nx - dc; /* code phase is circular */
      /* The exclusion zone: one tile either side AND one chip either side.
         A cell is excluded only if it is inside BOTH, which is the zone's
         definition; outside it every cell is a candidate second peak. */
      if (dr <= 1 && dc <= (long)SPC)
        continue;
      int b = dr >= 3 ? 3 : (int)dr;
      if (m[k] > mx[b])
        mx[b] = m[k];
      ss[b] += (double)m[k] * (double)m[k];
      cnt[b]++;
      if (m[k] > mx_all)
        mx_all = m[k];
      if (dc > (long)SPC && m[k] > mx_other)
        mx_other = m[k];
    }
  for (int b = 0; b < NBIN; b++)
    {
      out->max_db[b] = db (mx[b] / out->peak_mag);
      out->rms_db[b]
          = cnt[b] ? db (sqrt (ss[b] / (double)cnt[b]) / out->peak_mag) : 0.0;
    }
  out->max_all_db   = db (mx_all / out->peak_mag);
  out->max_other_db = db (mx_other / out->peak_mag);

  free (raw);
  wfm_synth_destroy (syn);
  acq_destroy (a);
  return 0;
}

static void
gold_1023 (uint8_t *code)
{
  /* The header's own worked example: CCSDS code #365, length 1023. */
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  if (check)
    {
      floor_t f;
      if (measure (code, 5.0e6, 0.0, 0, 0.0, 1, 1u, &f))
        return 1;
      int fail = 0;
      /* The synth's chip 0 sits TAU0 samples before the epoch start, so the
         engine reports the code phase nx - TAU0 (its own convention: the
         offset of the replica's start within the pushed epoch). */
      if (f.peak_row != TILE || f.peak_col != (NX - TAU0) % NX)
        {
          printf ("FAIL peak at (%zu,%zu), injected (%u,%u)\n", f.peak_row,
                  f.peak_col, TILE, (NX - TAU0) % NX);
          fail = 1;
        }
      /* Same tile, other lags, centred, no data: the Gold three-valued bound
         exactly -- integer-chip lags ARE {-1,-65,63}/1023 and half-chip
         lags are their means. */
      if (fabs (f.max_db[0] - GOLD_BOUND_DB) > 0.3)
        {
          printf ("FAIL same-tile off-peak max %.2f dB, Gold bound %.2f\n",
                  f.max_db[0], GOLD_BOUND_DB);
          fail = 1;
        }
      if (!fail)
        printf ("acq_emitter_floor: OK (same-tile max %.2f dB vs Gold %.2f; "
                "reference %.1f dB below peak)\n",
                f.max_db[0], GOLD_BOUND_DB, db (f.noise_est / f.peak_mag));
      return fail;
    }

  printf ("one emitter on the engine's own surface, dB below its peak\n");
  printf ("Gold-1023 (CCSDS #365), spc 2, +/-%.0f kHz; exclusion zone one "
          "tile x one chip\n\n",
          DU / 1e3);
  printf ("  Mcps  tiles  off   data   peak@       "
          "max d=0   max d=1   max d=2   max d>=3   rms d=0   rms d>=3   "
          "max other-phase   ref(noise_est)  test_stat\n");
  printf ("  ----  -----  ----  -----  ----------  "
          "--------  --------  --------  ---------  --------  ---------  "
          "---------------   --------------  ---------\n");
  const double rates[] = { 5.0e6, 2.0e6 };
  const double fracs[] = { 0.0, 0.5 };
  for (size_t ri = 0; ri < 2; ri++)
    for (size_t fi = 0; fi < 2; fi++)
      for (int flip = 0; flip < 2; flip++)
        {
          floor_t f;
          if (measure (code, rates[ri], fracs[fi], flip, 0.0, 1, 1u, &f))
            return 1;
          printf ("  %4.0f  %5zu  %4.1f  %5s  (%2zu,%5zu)  "
                  "%8.2f  %8.2f  %8.2f  %9.2f  %8.2f  %9.2f  %15.2f   %14.2f  "
                  "%9.1f\n",
                  rates[ri] / 1e6, f.window_bins, fracs[fi],
                  flip ? "flip" : "none", f.peak_row, f.peak_col, f.max_db[0],
                  f.max_db[1], f.max_db[2], f.max_db[3], f.rms_db[0],
                  f.rms_db[3], f.max_other_db, db (f.noise_est / f.peak_mag),
                  f.test_stat);
        }

  /* The reference with noise, emitter present and absent: what a strong
     emitter does to the threshold every weaker one is gated against. */
  printf ("\nthe CFAR reference with noise, 5 Mcps, centred, no data flip\n");
  printf ("  C/N0 dB-Hz   noise_est alone   noise_est with emitter   rise dB"
          "   emitter peak/ref dB\n");
  const double cn0s[] = { 55.0, 45.0, 40.0 };
  for (size_t ci = 0; ci < 3; ci++)
    {
      floor_t off, on;
      if (measure (code, 5.0e6, 0.0, 0, cn0s[ci], 0, 11u, &off)
          || measure (code, 5.0e6, 0.0, 0, cn0s[ci], 1, 11u, &on))
        return 1;
      printf ("  %10.0f   %15.4g   %22.4g   %7.2f   %19.2f\n", cn0s[ci],
              off.noise_est, on.noise_est, db (on.noise_est / off.noise_est),
              db (on.peak_mag / on.noise_est));
    }
  return 0;
}
