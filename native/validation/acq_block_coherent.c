/**
 * @file acq_block_coherent.c
 * @brief The block-coherent searcher, measured: what one emitter puts on
 *        the D-row surface in an aligned block and in a straddling one,
 *        the false-alarm rate per block, and the sensitivity the depth buys.
 *
 * The continuous engine now allows a coherent depth inside every window
 * tile, to accommodate waveforms with code-only windows
 * (docs/design/async-dsss-receiver.md §2.3): D epochs per block, D
 * slow-time rows per tile, one Doppler grid of tiles*D bins. Design §12
 * step 11 asks for the numbers behind it, through the engine, on the
 * shipped synth:
 *
 *   floor        one clean emitter in an ALIGNED block -- pure code for the
 *                whole block, the window's interior -- at the operating
 *                point: the surface read back through acq_surface() (§2.4,
 *                the gate's own units) and binned by Doppler-row distance
 *                from the peak; the worst cell at another code phase is the
 *                floor a second emitter competes with. Expected the
 *                transition-free number, about -21 dB.
 *   straddle     the same emitter in three blocks that are NOT aligned: one
 *                data transition mid-block; PRBS data at the symbol rate
 *                through the whole block (a block inside the data section);
 *                and the window's EDGE mid-block -- half pure code, half
 *                data, which is what a block sees when the code-only window
 *                opens or closes at no particular chip phase (§5.4). What
 *                the maintainer asked to be watched: the emitter's energy
 *                leaves its row for the others of its column, and the
 *                `conc` probe of §2.4 is the number that separates that
 *                splatter from a second emitter.
 *   pfa          pure noise, blocks of D = 16, the realized fraction of
 *                blocks whose gate fires against the configured pfa -- the
 *                CFAR counts every row of every tile, or it does not.
 *   sensitivity  realized Pd against C/N0 for D of 1, 16 and the operating
 *                point's 154, one look each: the C/N0 at which the depth
 *                detects what a single epoch cannot.
 *
 * Usage:
 *   validate_acq_block_coherent            full tables
 *   validate_acq_block_coherent --check    the spot checks CTest runs: the
 *                                          aligned floor and the straddles'
 *                                          concentration, pfa within its
 *                                          binomial band (#1064's interp
 *                                          factor included), D = 154
 *                                          detecting at a C/N0 where D = 1
 *                                          does not
 */
#include "acq/acq_core.h"
#include "dp_test.h"
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
#define W_SYM 450u      /* the code-only window, symbols on the data clock */
#define F_SYM 4950u     /* the frame, symbols                               */
#define DU 50000.0      /* +/-50 kHz, the design's starting uncertainty      */
#define RATE 500.0      /* Hz/s, the spec's Doppler rate                     */
#define SIZING_CN0 60.0 /* high enough that the sizer picks n_noncoh = 1 */
#define PD 0.9
#define TAU0 777u /* injected code phase, samples                         */
#define TILE 5u   /* injected tile (window hypothesis)                    */
#define NBIN 4    /* row-distance bins: 0 (the zone's edge), 1, 2, >= 3   */

static double
db (double lin)
{
  return 20.0 * log10 (lin > 0.0 ? lin : 1e-30);
}

/* The whole code-only epochs a window holds at any chip phase (§2.1). */
static size_t
code_only_epochs_for (double chip_rate)
{
  double cps = chip_rate / SYMBOL_RATE;
  return (size_t)floor ((double)W_SYM * cps / (double)SF) - 1;
}

static void
gold_1023 (uint8_t *code)
{
  gold_state_t *gd = gold_create (934, 350, 567, 73, 10);
  gold_generate (gd, SF, code, SF);
  gold_destroy (gd);
}

typedef enum
{
  BLK_ALIGNED,     /* pure code for the whole block             */
  BLK_FLIP_MID,    /* one data transition, mid-block            */
  BLK_PRBS,        /* PRBS data at the symbol rate, all block   */
  BLK_WINDOW_EDGE, /* the window closes mid-block: code | data  */
  BLK_NOISE        /* no emitter at all                         */
} block_kind_t;

typedef struct
{
  size_t D, tiles, rows, peak_row, peak_col, n_noncoh;
  double peak_stat, gate, conc;
  double max_row_db[NBIN]; /* max off-zone cell per row distance, dB */
  double max_other_db;     /* max at another code phase (|dc| > chip) */
  size_t want_col;         /* where the emitter's code phase lands   */
  int    hit;
} block_t;

/* One engine at chip_rate with the depth its window buys (or a chosen
   code_only_epochs when `epochs` > 0), the surface kept. The caller
   requires ONE_LOOK of it: this harness reads one block per decision. */
static acq_state_t *
engine_open (const uint8_t *code, double chip_rate, size_t epochs, double pfa)
{
  size_t       W = epochs ? epochs : code_only_epochs_for (chip_rate);
  acq_state_t *a = acq_create_continuous (code, SF, SPC, chip_rate,
                                          SYMBOL_RATE, SIZING_CN0, DU, pfa, PD,
                                          0, W, epochs ? 0.0 : RATE);
  if (a)
    a->keep_surface = 1;
  return a;
}
#define ONE_LOOK(a)                                                           \
  DP_REQUIRE_MSG ((a) && (a)->n_noncoh == 1,                                  \
                  "the engine is sized to one look (n_noncoh == 1)")

/* One block of the emitter (or of noise), at tile TILE plus `frac` of a
   row, code phase TAU0, at cn0_dbhz (0 = clean), rendered by the shipped
   synth; pushed; the surface read back and binned. */
static int
measure (const uint8_t *code, acq_state_t *a, block_kind_t kind, double frac,
         double cn0_dbhz, uint32_t seed, block_t *out)
{
  const size_t D = a->coherent_bins, tiles = a->window_bins;
  const size_t nx   = a->code_bins;
  const size_t rows = a->n_surf / nx;
  const double fs   = a->fs;
  const double cps  = a->chip_rate / SYMBOL_RATE;
  /* Tile TILE's own frequency plus a fraction of a ROW: the fold the
     engine reports rows on. */
  const long   r      = dp_fftfreq_index (TILE, tiles);
  const double f_norm = ((double)r + frac / (double)D) / (double)nx;
  const double snr_fs
      = cn0_dbhz > 0.0 ? cn0_dbhz - 10.0 * log10 (fs) : WFM_SYNTH_SNR_CLEAN;
  wfm_synth_state_t *syn
      = wfm_synth_create (WFM_SYNTH_DSSS, fs, f_norm * fs, snr_fs, 1, seed,
                          (int)SPC, 7, 0, 0, 0.0);
  static const uint8_t two_bits[2] = { 0, 1 };
  int                  rc          = -1;
  size_t               discard     = TAU0; /* samples before the block */
  switch (kind)
    {
    case BLK_ALIGNED:
    case BLK_NOISE:
      rc = wfm_synth_set_dsss_cont (syn, code, SF, (double)SF,
                                    WFM_DSSS_DATA_NONE, NULL, 0);
      break;
    case BLK_FLIP_MID:
      /* a "symbol" of half a block, bits {0,1}: one flip, mid-block */
      rc = wfm_synth_set_dsss_cont (syn, code, SF, (double)(D * SF) / 2.0,
                                    WFM_DSSS_DATA_BITS, two_bits, 2);
      break;
    case BLK_PRBS:
      rc = wfm_synth_set_dsss_cont (syn, code, SF, cps, WFM_DSSS_DATA_PRBS,
                                    NULL, 0);
      break;
    case BLK_WINDOW_EDGE:
      /* the real frame: the window closes at symbol W_SYM; start the
         block D/2 epochs before that edge */
      rc = wfm_synth_set_dsss_cont (syn, code, SF, cps, WFM_DSSS_DATA_PRBS,
                                    NULL, 0);
      if (rc == 0)
        rc = wfm_synth_set_dsss_window (syn, W_SYM, F_SYM);
      {
        double edge_chip = (double)W_SYM * cps;
        double start     = edge_chip - (double)(D / 2) * (double)SF;
        discard          = (size_t)floor (start) * SPC + TAU0;
      }
      break;
    }
  DP_REQUIRE_MSG (syn && rc == 0, "the synth takes the block's waveform");
  const size_t   blk = D * nx;
  float complex *raw = dp_xmalloc ((discard + blk) * sizeof *raw);
  if (kind == BLK_NOISE)
    wfm_synth_noise_steps (syn, raw, discard + blk);
  else
    wfm_synth_steps (syn, raw, discard + blk);
  acq_result_t hit[4];
  acq_reset (a);
  size_t nh = acq_push (a, raw + discard, blk, hit, 4);
  float *s  = dp_xmalloc (a->n_surf * sizeof *s);
  DP_REQUIRE_MSG (acq_surface (a, s, a->n_surf) == a->n_surf,
                  "the surface tap reads the decided block");
  out->D         = D;
  out->tiles     = tiles;
  out->rows      = rows;
  out->n_noncoh  = a->n_noncoh;
  out->peak_row  = a->peak_row; /* native */
  out->peak_col  = a->peak_col;
  out->peak_stat = a->test_stat;
  out->gate      = a->threshold;
  out->conc      = a->peak_conc;
  out->hit       = nh > 0;
  /* The code phase the engine reports for a block that starts `discard`
     samples into the code: the hand-off's mapping, inverted. */
  out->want_col = (nx - discard % nx) % nx;
  /* Bin every cell outside the exclusion zone by its Doppler-row distance
     from the peak -- native rows, circular on the one folded grid -- and
     keep the worst cell at another code phase. */
  const size_t prow = a->peak_row * a->interp; /* surface row of the peak */
  double       mx[NBIN] = { 0 }, mx_other = 0.0;
  for (size_t k = 0; k < a->n_surf; k++)
    {
      size_t row = k / nx, col = k % nx;
      size_t dr = row > prow ? row - prow : prow - row;
      if (dr > rows - dr)
        dr = rows - dr;
      double drn = (double)dr / (double)a->interp; /* native rows */
      long   dc  = (long)col - (long)out->peak_col;
      if (dc < 0)
        dc = -dc;
      if ((size_t)dc > nx / 2)
        dc = (long)nx - dc;
      if (drn <= 1.0 && dc <= (long)SPC)
        continue; /* the zone: the emitter's own main lobe */
      int b = drn >= 3.0 ? 3 : (int)floor (drn);
      if (s[k] > mx[b])
        mx[b] = s[k];
      if (dc > (long)SPC && s[k] > mx_other)
        mx_other = s[k];
    }
  for (int b = 0; b < NBIN; b++)
    out->max_row_db[b] = db (mx[b] / out->peak_stat);
  out->max_other_db = db (mx_other / out->peak_stat);
  free (s);
  free (raw);
  wfm_synth_destroy (syn);
  return 0;
}

static const char *
kind_name (block_kind_t k)
{
  static const char *const names[] = {
    [BLK_ALIGNED]     = "aligned (pure code)",
    [BLK_FLIP_MID]    = "one transition mid-block",
    [BLK_PRBS]        = "PRBS data, whole block",
    [BLK_WINDOW_EDGE] = "window edge mid-block",
    [BLK_NOISE]       = "noise",
  };
  return names[k];
}

int
main (int argc, char **argv)
{
  int     check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  uint8_t code[SF];
  gold_1023 (code);

  /* ── the floor, and the straddles: one clean emitter per block ────── */
  const double rates[2] = { 5.0e6, 2.0e6 };
  block_t      aligned[2];
  for (int i = 0; i < (check ? 1 : 2); i++)
    {
      acq_state_t *a = engine_open (code, rates[i], 0, 1e-3);
      ONE_LOOK (a);
      printf ("\n%.0f Mcps: %zu tiles x D = %zu rows (%zu whole code-only "
              "epochs, rate %.0f Hz/s), %.1f Hz per row, %zu surface rows\n",
              rates[i] / 1e6, a->window_bins, a->coherent_bins,
              a->code_only_epochs, RATE, a->doppler_res_hz,
              a->n_surf / a->code_bins);
      printf ("  %-26s %-9s %-8s %-6s %-6s %-6s %-6s %-6s %-6s\n", "block",
              "peak/gate", "conc", "row0", "row1", "row2", "row3+", "other",
              "col");
      for (int kd = BLK_ALIGNED; kd <= BLK_WINDOW_EDGE; kd++)
        {
          block_t b;
          DP_REQUIRE (measure (code, a, (block_kind_t)kd, 0.25, 0.0, 11u, &b)
                      == 0);
          if (kd == BLK_ALIGNED)
            aligned[i] = b;
          printf ("  %-26s %5.1f/%-4.1f %-8.2f %-6.1f %-6.1f %-6.1f %-6.1f "
                  "%-6.1f %zu\n",
                  kind_name ((block_kind_t)kd), b.peak_stat, b.gate, b.conc,
                  b.max_row_db[0], b.max_row_db[1], b.max_row_db[2],
                  b.max_row_db[3], b.max_other_db, b.peak_col);
          if (check)
            {
              DP_CHECK (b.hit);
              DP_CHECK_MSG (b.peak_col == b.want_col,
                            "the peak is at the emitter's code phase");
              if (kd == BLK_ALIGNED)
                {
                  DP_CHECK_MSG (b.max_other_db < -18.0,
                                "an aligned block's floor at another code "
                                "phase is the transition-free number");
                  DP_CHECK_MSG (b.conc > 0.8, "an aligned block: one lobe");
                }
              else
                {
                  DP_CHECK_MSG (b.conc < aligned[i].conc,
                                "a straddling block reads less concentrated");
                  DP_CHECK_MSG (b.peak_stat < aligned[i].peak_stat,
                                "and weaker");
                }
            }
        }
      acq_destroy (a);
    }
  printf ("  (dB below the peak, cells outside the exclusion zone, by "
          "Doppler-row distance; `other` = at another code phase)\n");

  /* ── pfa per block: pure noise at D = 16 ───────────────────────────── */
  {
    const double pfa    = check ? 0.2 : 0.1;
    const int    blocks = check ? 100 : 300;
    acq_state_t *a      = engine_open (code, 5.0e6, 31, pfa);
    ONE_LOOK (a);
    int fired = 0;
    for (int t = 0; t < blocks; t++)
      {
        block_t b;
        DP_REQUIRE (
            measure (code, a, BLK_NOISE, 0.0, 45.0, 1000u + (uint32_t)t, &b)
            == 0);
        fired += b.hit;
      }
    /* The slow-time axis is interpolated `interp`-fold and the maximum runs
       over the interpolated surface while the threshold's N counts native
       cells -- doppler#1064, the open finding on the burst engine, which
       the continuous engine inherits with its slow-time axis. The realized
       rate is therefore 1 - (1 - pfa)^interp, and that is what is pinned:
       the CFAR counts every row of every tile, and #1064 is #1064. */
    const double expect = 1.0 - pow (1.0 - pfa, (double)a->interp);
    double       sd     = sqrt (expect * (1.0 - expect) * blocks);
    printf ("\npfa per block, D = %zu, %zu tiles, interp %zu, configured "
            "%.2f: %d of %d blocks fired (expected %.0f +- %.0f: "
            "1 - (1 - pfa)^interp, doppler#1064)\n",
            a->coherent_bins, a->window_bins, a->interp, pfa, fired, blocks,
            expect * blocks, sd);
    if (check)
      DP_CHECK_MSG (fabs ((double)fired - expect * blocks) <= 3.0 * sd,
                    "the realized pfa is the configured one over the "
                    "interpolated cells (#1064): the CFAR counts every row "
                    "of every tile");
    acq_destroy (a);
  }

  /* ── sensitivity: realized Pd against C/N0, per depth ──────────────── */
  {
    const size_t epochs[3] = { 1, 31, 0 }; /* D = 1, 16, the window's 154 */
    const double cn0s[4]   = { 30.0, 34.0, 38.0, 42.0 };
    const int    trials    = check ? 3 : 20;
    printf ("\nsensitivity at 5 Mcps, one look, realized Pd over %d trials"
            " (mean peak/gate):\n  %-6s",
            trials, "D");
    for (int c = 0; c < 4; c++)
      printf ("   %5.0f dB-Hz", cn0s[c]);
    printf ("\n");
    double       pd_d1_chk = -1.0, pd_d154_chk = -1.0;
    const double chk_cn0 = 38.0; /* measured: D = 154 at 2.7x the gate,
                                    D = 1 at 0.8x -- robust both ways */
    for (int e = 0; e < 3; e++)
      {
        acq_state_t *a = engine_open (code, 5.0e6, epochs[e], 1e-3);
        ONE_LOOK (a);
        printf ("  %-6zu", a->coherent_bins);
        for (int c = 0; c < (check ? 2 : 4); c++)
          {
            const double cn0 = check ? chk_cn0 : cn0s[c];
            if (check && c == 1)
              break;
            int    hits = 0;
            double marg = 0.0;
            for (int t = 0; t < trials; t++)
              {
                block_t b;
                DP_REQUIRE (measure (code, a, BLK_ALIGNED, 0.25, cn0,
                                     5000u + (uint32_t)(100 * c + t), &b)
                            == 0);
                hits += b.hit;
                marg += b.peak_stat / b.gate;
              }
            double pd = (double)hits / trials;
            printf ("   %4.2f (%4.1f)", pd, marg / trials);
            if (a->coherent_bins == 1 && cn0 == chk_cn0)
              pd_d1_chk = pd;
            if (epochs[e] == 0 && cn0 == chk_cn0)
              pd_d154_chk = pd;
          }
        printf ("\n");
        acq_destroy (a);
      }
    if (check)
      {
        DP_CHECK_MSG (pd_d154_chk == 1.0,
                      "at 38 dB-Hz the window's depth detects every block");
        DP_CHECK_MSG (pd_d1_chk == 0.0, "and a single epoch detects none");
      }
  }
  if (check)
    DP_TEST_END ("validate_acq_block_coherent");
  return 0;
}
