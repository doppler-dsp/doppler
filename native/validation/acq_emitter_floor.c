/**
 * @file acq_emitter_floor.c
 * @brief What ONE emitter puts on the (tile x code phase) surface, and what
 *        the CFAR reference makes of it.
 *
 * The continuous async-DSSS design (docs/design/acq-surface.md §8) forks
 * on one number: how far below a strong emitter's peak the rest
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
#include "doppler/correlator/correlator_core.h"
#include "doppler/dp_complex.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_dsss_test.h"
#include "dp_test.h"
#include "dp_tx_test.h"
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
#define LB 2048u  /* lag_bins: NX is not 7-smooth, so the next pow2  */
#define TAU0 777u /* injected code phase, samples                   */
#define TILE 5u   /* injected tile (window hypothesis)              */
#define NBIN 4    /* tile-distance bins: 0, 1, 2, >=3                */

#define GOLD_BOUND_DB (20.0 * log10 (65.0 / 1023.0)) /* -23.94 dB */
/* The same-tile floor this harness measured on the padded lag grid
   (2026-11-01, 2048 columns of the 2046-sample period): the Gold bound on
   whole- and half-chip lags, read 0.75 dB higher between samples by the
   zero-padded interpolation. A REGRESSION PIN, not a derived bound. */
#define FLOOR_PIN_DB (-23.19)

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
  /* Rule 2's own claim, cell by cell (clean runs only): every off-zone
     cell of the peak's tile and the two beside it against
     dp_correlator_sidelobe_ratio() at that cell, the least margin and where.
   */
  double min_margin_db;          /* model minus cell, dB; the least     */
  double margin_cell_db;         /* that cell, dB below the peak        */
  double margin_model_db;        /* its model value                     */
  size_t margin_row, margin_col; /* where                               */
  size_t margin_cells;           /* cells compared                      */
  size_t n_noncoh;
  size_t window_bins;
} floor_t;

/* Build one dwell of an emitter and push it; read the surface back. The
 * emitter is at tile TILE plus `frac` of a tile, code phase TAU0, unit
 * amplitude, with an optional sign flip at mid-epoch every epoch (the async
 * data's worst case) and optional noise at cn0_dbhz (0 = none). `present`
 * = 0 pushes the noise alone, for the reference-rise row. */
/* The per-cell model pass: ~6,100 dp_correlator_sidelobe_ratio() calls a run,
   against one hit, so its cache makes them three row sweeps (one a tile)
   and lookups: milliseconds. The --check runs it on the two clean emitters
   it gates; the full sweep runs it on the data-transition one, which is
   printed, not gated (doppler#2734). */
static int with_margins;

static int
measure (const uint8_t *code, double chip_rate, double frac, int data_flip,
         double cn0_dbhz, int present, uint32_t seed, floor_t *out)
{
  dp_correlator_state_t *a = dp_correlator_create_continuous (
      code, SF, chip_rate * (double)SPC, chip_rate, SYMBOL_RATE, SIZING_CN0,
      DU, PFA, PD, 0, 1, 0.0);
  if (!a)
    {
      DP_TEST_ERR ("dp_correlator_create_continuous failed\n");
      return 1;
    }
  out->n_noncoh    = a->search->n_noncoh;
  out->window_bins = a->search->window_bins;
  if (a->search->n_noncoh != 1 || a->search->coherent_bins != 1
      || a->search->interp != 1)
    {
      DP_TEST_ERR (
          "engine sized n_noncoh=%zu coherent_bins=%zu interp=%zu; this "
          "harness reads the single-look surface and needs 1/1/1\n",
          a->search->n_noncoh, a->search->coherent_bins, a->search->interp);
      dp_correlator_destroy (a);
      return 1;
    }
  const size_t nx = a->search->code_bins;
  const size_t W  = a->search->window_bins;
  /* The surface is read on lag_bins columns (2048 for this 2046-sample
     period: the padded lag grid), a column code_bins/lag_bins of a sample. */
  const size_t lb = a->search->lag_bins;
  /* One chip, SPC samples, in columns: a column 2 away is 1.998 samples
     off (inside the chip), 3 away is 2.997 (outside). */
  const long zc = (long)(SPC * lb / nx);
  if (lb != LB)
    {
      DP_TEST_ERR ("engine read %zu lag columns; the checks expect %u\n", lb,
                   LB);
      dp_correlator_destroy (a);
      return 1;
    }
  const double fs = chip_rate * (double)SPC;

  /* Tile TILE's own frequency plus a fraction of a tile: the same fold the
     engine reports rows on, so the injected row and the read row agree. */
  const long   r      = dp_fftfreq_index (TILE, W);
  const double f_norm = ((double)r + frac) / (double)nx;

  /* The emitter, rendered by the shipped synth. `snr` is over fs; a C/N0
     converts to it in one place (dp_wfm_snr_over_fs is the composer's; the
     synth takes the over-fs value directly). Clean means no AWGN child. */
  const double snr_fs
      = cn0_dbhz > 0.0 ? cn0_dbhz - 10.0 * log10 (fs) : WFM_SYNTH_SNR_CLEAN;
  dp_wfm_synth_state_t *syn
      = dp_wfm_synth_create (WFM_SYNTH_DSSS, fs, f_norm * fs, snr_fs, 1, seed,
                             (int)SPC, 7, 0, 0, 0.0);
  /* No data: one bit per code period (the pure code). A transition at
     mid-epoch, every epoch: the pattern {0,1} at two symbols per period. */
  static const uint8_t two_bits[2] = { 0, 1 };
  /* A pattern is sent once (doppler#1718): tiled to every symbol the
     TAU0 + nx samples below touch (a symbol is SF/2 chips, SF*SPC/2
     samples), and checked after the render. */
  const size_t sym_samples = (size_t)SF * SPC / 2u;
  const size_t n_flips     = (TAU0 + nx + sym_samples - 1u) / sym_samples + 1u;
  uint8_t     *flips
      = data_flip ? dp_tx_bits_for_run (two_bits, 2, n_flips) : NULL;
  int rc = data_flip
               ? (flips ? dp_wfm_synth_set_dsss_cont (
                              syn, code, SF, (double)SF / 2.0,
                              WFM_DSSS_DATA_BITS, flips, n_flips)
                        : -1)
               : dp_wfm_synth_set_dsss_cont (syn, code, SF, (double)SF,
                                             WFM_DSSS_DATA_NONE, NULL, 0);
  free (flips);
  if (!syn || rc != 0)
    {
      DP_TEST_ERR ("wfm_synth continuous DSSS setup failed\n");
      dp_correlator_destroy (a);
      dp_wfm_synth_destroy (syn);
      return 1;
    }
  /* Code phase tau0: render tau0 extra samples and start the epoch there.
     `present` = 0 takes the synth's noise term alone, the same call the
     composer uses to carry a noise floor through a gap. */
  float complex *raw = malloc ((TAU0 + nx) * sizeof *raw);
  if (present)
    dp_wfm_synth_steps (syn, raw, TAU0 + nx);
  else
    dp_wfm_synth_noise_steps (syn, raw, TAU0 + nx);
  float complex *buf = raw + TAU0;

  dp_correlator_hit_t hit[2];
  const size_t        nh = dp_correlator_push (a, buf, nx, hit, 2);

  /* The engine's surface, as gated on. */
  const float *m  = a->mag_buf;
  size_t       pk = 0;
  for (size_t k = 1; k < a->search->n_surf; k++)
    if (m[k] > m[pk])
      pk = k;
  out->peak_row  = pk / lb;
  out->peak_col  = pk % lb;
  out->peak_mag  = m[pk];
  out->noise_est = a->noise_est;
  out->test_stat = a->test_stat;

  double mx[NBIN] = { 0 }, ss[NBIN] = { 0 };
  size_t cnt[NBIN] = { 0 };
  double mx_all    = 0.0;
  double mx_other  = 0.0;
  for (size_t k = 0; k < a->search->n_surf; k++)
    {
      size_t row = k / lb, col = k % lb;
      long   dr = (long)row - (long)out->peak_row;
      if (dr < 0)
        dr = -dr;
      long dc = (long)col - (long)out->peak_col;
      if (dc < 0)
        dc = -dc;
      if ((size_t)dc > lb / 2)
        dc = (long)lb - dc; /* code phase is circular */
      /* The exclusion zone: one tile either side AND one chip either side.
         A cell is excluded only if it is inside BOTH, which is the zone's
         definition; outside it every cell is a candidate second peak. */
      if (dr <= 1 && dc <= zc)
        continue;
      int b = dr >= 3 ? 3 : (int)dr;
      if (m[k] > mx[b])
        mx[b] = m[k];
      ss[b] += (double)m[k] * (double)m[k];
      cnt[b]++;
      if (m[k] > mx_all)
        mx_all = m[k];
      if (dc > zc && m[k] > mx_other)
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

  /* Rule 2's claim is per cell: what this emitter's sidelobe reads at a
     cell must sit under the model's value FOR THAT CELL, not only at the
     worst one. Every off-zone cell of the peak's tile and its two
     neighbours, against dp_correlator_sidelobe_ratio() there. Clean runs only:
     with noise a cell is the sidelobe plus noise, which rule 2's gate,
     not the model, covers. */
  out->min_margin_db = INFINITY;
  out->margin_cells  = 0;
  if (with_margins && present && !(cn0_dbhz > 0.0) && nh > 0)
    {
      /* Rule 2 scales the model by the emitter's statistic ON ITS OWN LAG
         (dp_correlator_hit_t::test_stat_peak: the straddle between two lag
         columns put back), not by the peak as read, so a cell is compared on
         that scale too. */
      const double on_lag = out->peak_mag * (double)hit[0].test_stat_peak
                            / (double)hit[0].test_stat;
      const size_t rows   = a->search->n_surf / lb;
      const dp_correlator_hit_t from
          = { .row = out->peak_row, .col = out->peak_col };
      for (long d = -1; d <= 1; d++)
        {
          const size_t row
              = (size_t)((long)out->peak_row + (long)rows + d) % rows;
          for (size_t col = 0; col < lb; col++)
            {
              long dc = (long)col - (long)out->peak_col;
              if (dc < 0)
                dc = -dc;
              if ((size_t)dc > lb / 2)
                dc = (long)lb - dc;
              if (dc <= zc) /* the zone: every row here is within one */
                continue;
              const dp_correlator_hit_t to   = { .row = row, .col = col };
              const double              cell = db (m[row * lb + col] / on_lag);
              const double              model
                  = db (dp_correlator_sidelobe_ratio (a, &from, &to));
              out->margin_cells++;
              if (model - cell < out->min_margin_db)
                {
                  out->min_margin_db   = model - cell;
                  out->margin_cell_db  = cell;
                  out->margin_model_db = model;
                  out->margin_row      = row;
                  out->margin_col      = col;
                }
            }
        }
    }

  free (raw);
  DP_CHECK_MSG (!dp_wfm_synth_data_ended (syn),
                "the flip pattern covered the render");
  dp_wfm_synth_destroy (syn);
  dp_correlator_destroy (a);
  return 0;
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);

  if (check)
    {
      floor_t f;
      with_margins = 1;
      DP_REQUIRE (measure (code, 5.0e6, 0.0, 0, 0.0, 1, 1u, &f) == 0);
      /* The synth's chip 0 sits TAU0 samples before the epoch start, so the
         engine reports the code phase nx - TAU0 samples (its own convention:
         the offset of the replica's start within the pushed epoch), on the
         lag column nearest it: lag_bins = 2048 columns of NX/2048 samples. */
      DP_CHECK (f.peak_row == TILE);
      DP_CHECK (f.peak_col
                == (size_t)lround ((double)((NX - TAU0) % NX) * LB / NX) % LB);
      /* Same tile, other lags, centred, no data. Until the padded lag grid
         (the commit that read an awkward period on it, #2731) every column
         was a whole or half-chip lag and the floor was the Gold three-valued
         bound exactly ({-1,-65,63}/1023 and their means). Now the surface
         is read on lag_bins = 2048 columns of a 2046-sample period, and
         between samples the zero-padded interpolation reads above it:
         -23.19 dB against Gold's -23.94.

         1. RULE 2, CELL BY CELL: every off-zone cell of the emitter's tile
            and the two beside it sits under dp_correlator_sidelobe_ratio() at
         that cell, on rule 2's own scale (the emitter's statistic on its own
            lag). Measured least margin +0.65 dB centred, +0.61 dB half a
            tile off, over 6,129 cells each: the model is a sampled maximum,
            not a proven bound, and this is its measured margin. With a data
            transition inside the epoch it does NOT hold: a cell reads 20.4
            dB above its model, which assumes one clean period. That is the
            D = 1 data gap (doppler#2734); the full sweep prints it, not
            gated.
         2. A REGRESSION PIN on the floor itself, +-0.3 dB, so a surface
            that degrades by more than that is red even where the model has
            room. */
      floor_t half;
      DP_REQUIRE (measure (code, 5.0e6, 0.5, 0, 0.0, 1, 1u, &half) == 0);
      const floor_t *row[2] = { &f, &half };
      const char *name[2] = { "centred, no data", "half a tile off, no data" };
      for (size_t i = 0; i < 2; i++)
        {
          printf ("  %-26s %zu cells, least margin %+.2f dB at (%zu,%zu): "
                  "cell %.2f, model %.2f\n",
                  name[i], row[i]->margin_cells, row[i]->min_margin_db,
                  row[i]->margin_row, row[i]->margin_col,
                  row[i]->margin_cell_db, row[i]->margin_model_db);
          DP_CHECK (row[i]->margin_cells > 6000);
          DP_CHECK_MSG (row[i]->min_margin_db > 0.0,
                        "every clean cell sits under rule 2's model");
        }
      DP_CHECK_NEAR (f.max_db[0], FLOOR_PIN_DB, 0.3);
      printf ("  same-tile max %.2f dB, Gold %.2f; reference %.1f dB below "
              "peak\n",
              f.max_db[0], GOLD_BOUND_DB, db (f.noise_est / f.peak_mag));
      DP_TEST_END ("validate_acq_emitter_floor");
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

  /* Rule 2's model against a data transition inside the epoch: one clean
     period does not bound it (doppler#2734). Printed, not gated. */
  {
    floor_t d;
    with_margins = 1;
    if (measure (code, 5.0e6, 0.0, 1, 0.0, 1, 1u, &d))
      return 1;
    with_margins = 0;
    printf ("\nrule 2's model, centred, a transition in the epoch: least "
            "margin %+.2f dB over %zu cells at (%zu,%zu): cell %.2f, model "
            "%.2f (doppler#2734)\n",
            d.min_margin_db, d.margin_cells, d.margin_row, d.margin_col,
            d.margin_cell_db, d.margin_model_db);
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
