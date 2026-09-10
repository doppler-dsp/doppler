/**
 * @file refine_bias.c
 * @brief Is the refine's Doppler estimate biased -- and by the seed's
 *        offset, by the dwell, or by the stream it estimates on?
 *
 * The continuous async-DSSS receiver (docs/design/async-dsss-receiver.md
 * §4, §12.9) hands its carrier loop the refine stage's Doppler: the
 * coarse seed plus the residual `CarrierAcquisition` finds on the
 * despread stream, the peak of its averaged PSD correlated with a sinc^2
 * template, parabolically refined between bins. At the floor from a
 * 50 kHz seed the estimate lands 200-460 Hz low on every seed with the
 * floor's 18-block dwell, and loop 1 (pull-in 60 Hz) then acquires it
 * slowly or not at all (#1252). This harness measures that estimate on
 * its own, with nothing downstream of it: the error of the Doppler the
 * receiver hands over, against the seed's offset, against the dwell, and
 * on each of the two despread streams the refine can be given.
 *
 * Method. A static capture from the shipped C stimulus (`dp_dsss_capture`:
 * a 1023-chip Gold code at 5 Mcps, two samples per chip, asynchronous
 * BPSK data at 2700 sym/s, a fixed carrier offset of F_TRUE, AWGN sized
 * from the C/N0), so the truth is a number the harness chose and the
 * clock is undilated -- the one variable is the estimate. The receiver
 * is the SEARCHING flavor (`async_dsss_receiver_create`), seeded
 * with the stimulus's own chip phase (the capture starts on chip 0) and
 * a Doppler of F_TRUE plus a chosen error, exactly what the searcher's
 * coarse D = 1 row hands it (its bin is 4.9 kHz wide, so ~1.1 kHz off at
 * 50 kHz). Blocks of one epoch are fed until `get_tracking()` first
 * reads 1, and `get_doppler_hz()` is read there: the value the live
 * carrier loop was seeded with, moved by at most one loop update over
 * the hand-over block's tail. The error is that reading minus F_TRUE; a
 * point is N_SEEDS noise seeds, reported as mean (the bias), standard
 * deviation (the noise), and the extremes.
 *
 * Three axes. (1) The seed's error: 0, +-500, +-1100, +-2000 Hz, at the
 * floor's dwell -- `refine_design_margin_db` 19 at 45 dB-Hz sizes the
 * same 18-block dwell that 14 does at 40, the test's own trick, so the
 * capture is short and the dwell is the floor's. A bias proportional to
 * the error is under-correction: the estimate leans toward the seed, and
 * a second pass (re-wipe at the first estimate, refine again) would
 * take it out; a bias independent of it points elsewhere. F_TRUE is
 * nonzero and taken with both signs, so a lean toward the SEED (the
 * refine's own DC) is told from a lean toward true DC. (2) The dwell, at
 * a fixed +1100 Hz error, through the design margin: each row prints
 * the `dwell_target` the margin sized. (3) The despread stream: the
 * refine collects it with a look-back Dll whose dumps per epoch come from
 * `refine_max_error_db` via `dll_lookback_segments()`. The shipped
 * default, 0.5 dB, gives 11 dumps per epoch (53.8 kHz here); 100 dB
 * gives one, the epoch rate of 4.9 kHz -- below the 2700-baud data
 * lobe's own width, so any residual aliases -- and `objects/
 * async_dsss_receiver.toml` records that value as retired for exactly
 * that reason. Every C harness and test of this receiver still passes
 * 100 dB, the measurement behind #1252 included; the harness runs both,
 * so the two are compared on the same captures. Nothing here builds a
 * chip, a bit, a sigma or an estimate by hand.
 *
 * Usage:
 *   validate_refine_bias            full table: both streams, both signs
 *                                   of F_TRUE, N_SEEDS trials per point
 *   validate_refine_bias --check    both streams at the floor's dwell,
 *                                   N_SEEDS trials at 0 and +-1100 Hz;
 *                                   the design's expectations asserted
 */
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "clib_common.h"
#include "dp_dsss_test.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define TSYM (FS / SYM_RATE) /* samples per symbol, 3703.7 */
#define CARRIER_HZ 2.5e9     /* the carrier->code aid, as in the pool  */
#define CN0 45.0             /* Es/N0 10.7 dB                           */
#define FLOOR_MARGIN_DB 19.0 /* 18-block dwell at 45 dB-Hz = 14 at 40  */
#define F_TRUE 1500.0        /* the capture's carrier offset, Hz        */
#define N_SYM 2700u          /* one second of capture                   */
#define PRE_SILENCE 3u       /* the capture helper's noise-only prefix  */
#define N_SEEDS 10           /* --check too: a 7-block point has sd ~80 Hz  */
#define REFINE_N_FFT 64

/* The despread stream the refine estimates on, by `refine_max_error_db`:
   the shipped default and the retired one (see the file comment). */
static const double lookback_db[2]   = { 0.5, 100.0 };
static const char  *lookback_name[2] = { "0.5 dB", "100 dB" };

static const double seed_errs[]
    = { 0.0, 500.0, -500.0, 1100.0, -1100.0, 2000.0, -2000.0 };
#define N_ERRS (sizeof seed_errs / sizeof seed_errs[0])
static const double margins[] = { 8.0, 11.0, 14.0, 17.0, 19.0, 22.0, 25.0 };
#define N_MARGINS (sizeof margins / sizeof margins[0])

typedef struct
{
  float _Complex *x;
  size_t          n;
  double         *data;
} capture_t;

/* One capture per (sign of F_TRUE, noise seed), shared by every point:
   the same noise under every seed error, dwell and stream. */
static capture_t g_cap[2][N_SEEDS];

static const capture_t *
get_capture (const uint8_t *code, int sign_idx, size_t seed)
{
  capture_t *c = &g_cap[sign_idx][seed];
  if (!c->x)
    dp_dsss_capture (code, SF, SPC, FS, TSYM, sign_idx ? -F_TRUE : F_TRUE, CN0,
                     N_SYM, PRE_SILENCE, 100u + (uint32_t)seed, &c->x, &c->n,
                     &c->data);
  return c;
}

typedef struct
{
  int    handed;   /* the refine handed over inside the capture      */
  double err_hz;   /* get_doppler_hz() at the hand-over, minus truth */
  double handed_s; /* seed -> hand-over, s                           */
  size_t dwell;    /* CarrierAcquisition's dwell_target, blocks      */
  size_t segments; /* look-back dumps per epoch                      */
} trial_t;

static int
run_trial (const uint8_t *code, int sign_idx, size_t seed, double seed_err,
           double margin_db, int lb, trial_t *out)
{
  const capture_t *cap   = get_capture (code, sign_idx, seed);
  const double     truth = sign_idx ? -F_TRUE : F_TRUE;
  memset (out, 0, sizeof *out);
  /* The searching flavour, seeded from outside: its refine chain is the
     one under test (the hand-off flavour that carried this validator was
     retired, design section 12.28; seed() is a method of both). */
  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, CN0, 1e-2, 0.9, 100.0, 4, 8, 0,
      lookback_db[lb], 4, margin_db, REFINE_N_FFT, 8, false, 100000,
      CARRIER_HZ, 0.0);
  DP_REQUIRE_MSG (rx != NULL, "the receiver opens");
  /* The capture's signal starts on chip 0 at PRE_SILENCE; fed from there,
     the seed's phase is 0 -- the test's own convention. */
  DP_REQUIRE_MSG (async_dsss_receiver_seed (rx, 0.0, truth + seed_err, CN0)
                      == DP_OK,
                  "the receiver takes the seed");
  out->dwell              = rx->ca->dwell_target;
  out->segments           = rx->refine_segments;
  size_t          max_out = async_dsss_receiver_steps_max_out (rx);
  float _Complex *syms = dp_xmalloc ((max_out ? max_out : TE) * sizeof *syms);
  const float _Complex *x = cap->x + PRE_SILENCE;
  const size_t          n = cap->n - PRE_SILENCE;
  for (size_t pos = 0; pos + TE <= n; pos += TE)
    {
      (void)async_dsss_receiver_steps (rx, x + pos, TE, syms,
                                       max_out ? max_out : TE);
      if (async_dsss_receiver_get_tracking (rx) == 1)
        {
          out->handed   = 1;
          out->err_hz   = async_dsss_receiver_get_doppler_hz (rx) - truth;
          out->handed_s = (double)(pos + TE) / FS;
          break;
        }
    }
  free (syms);
  async_dsss_receiver_destroy (rx);
  return 0;
}

typedef struct
{
  size_t n, handed;
  double mean, sd, min, max, handed_s;
  size_t dwell, segments;
} point_t;

/* One point: N seeds of one (sign, error, margin, stream); the error's
   mean is the bias, its standard deviation the noise around it. */
static int
run_point (const uint8_t *code, int sign_idx, double seed_err,
           double margin_db, int lb, size_t n_seeds, point_t *out)
{
  point_t p = { 0 };
  p.min     = HUGE_VAL;
  p.max     = -HUGE_VAL;
  double s = 0.0, s2 = 0.0;
  for (size_t k = 0; k < n_seeds; k++)
    {
      trial_t t;
      DP_REQUIRE (run_trial (code, sign_idx, k, seed_err, margin_db, lb, &t)
                  == 0);
      p.n++;
      p.dwell    = t.dwell;
      p.segments = t.segments;
      if (!t.handed)
        continue;
      p.handed++;
      s += t.err_hz;
      s2 += t.err_hz * t.err_hz;
      p.handed_s += t.handed_s;
      if (t.err_hz < p.min)
        p.min = t.err_hz;
      if (t.err_hz > p.max)
        p.max = t.err_hz;
    }
  if (p.handed)
    {
      p.mean = s / (double)p.handed;
      p.sd   = p.handed > 1 ? sqrt (fmax (0.0, (s2 - s * s / (double)p.handed)
                                                   / (double)(p.handed - 1)))
                            : 0.0;
      p.handed_s /= (double)p.handed;
    }
  *out = p;
  return 0;
}

static void
print_point_header (void)
{
  printf ("    truth      seed err   dwell   handed    bias (mean)   "
          "sd         min        max   hand-over\n");
}

static void
print_point (const point_t *p, double truth, double seed_err)
{
  printf ("    %+7.0f Hz  %+7.0f Hz  %5zu   %zu/%zu   %+9.1f Hz  %7.1f  "
          "%+8.1f  %+8.1f   %6.1f ms\n",
          truth, seed_err, p->dwell, p->handed, p->n, p->mean, p->sd, p->min,
          p->max, p->handed_s * 1e3);
}

static void
gold_1023 (uint8_t *code)
{
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

  printf ("refine bias: Gold-1023 at 5 Mcps, spc 2, 2700 sym/s async BPSK "
          "at %.0f dB-Hz (Es/N0 %.1f dB); static capture at a carrier "
          "offset of +-%.0f Hz; seeded receiver, refine_n_fft %d (bin "
          "%.1f Hz at %.0f sps), the error of get_doppler_hz() at the "
          "hand-over; block = one epoch (%.3f ms)\n\n",
          CN0, CN0 - 10.0 * log10 (SYM_RATE), F_TRUE, REFINE_N_FFT,
          4.0 * SYM_RATE / REFINE_N_FFT, 4.0 * SYM_RATE,
          (double)TE / FS * 1e3);

  const size_t n_seeds = N_SEEDS;
  const int    n_sign  = check ? 1 : 2;
  for (int lb = 0; lb < 2; lb++)
    {
      printf ("=== refine_max_error_db %s ===\n", lookback_name[lb]);
      printf ("--- bias vs seed error at the floor's dwell (margin %.0f "
              "dB) ---\n",
              FLOOR_MARGIN_DB);
      print_point_header ();
      point_t at_zero = { 0 }, at_off[2] = { { 0 }, { 0 } };
      for (int sg = 0; sg < n_sign; sg++)
        for (size_t e = 0; e < N_ERRS; e++)
          {
            if (check && fabs (seed_errs[e]) != 0.0
                && fabs (seed_errs[e]) != 1100.0)
              continue;
            point_t p;
            DP_REQUIRE (run_point (code, sg, seed_errs[e], FLOOR_MARGIN_DB, lb,
                                   n_seeds, &p)
                        == 0);
            print_point (&p, sg ? -F_TRUE : F_TRUE, seed_errs[e]);
            if (sg == 0 && seed_errs[e] == 0.0)
              at_zero = p;
            if (sg == 0 && seed_errs[e] == 1100.0)
              at_off[0] = p;
            if (sg == 0 && seed_errs[e] == -1100.0)
              at_off[1] = p;
          }
      if (!check)
        {
          printf ("--- bias vs dwell at a seed error of +1100 Hz (truth "
                  "%+.0f Hz) ---\n",
                  F_TRUE);
          printf ("    margin  ");
          print_point_header ();
          for (size_t m = 0; m < N_MARGINS; m++)
            {
              point_t p;
              DP_REQUIRE (
                  run_point (code, 0, 1100.0, margins[m], lb, n_seeds, &p)
                  == 0);
              printf ("    %4.0f dB", margins[m]);
              print_point (&p, F_TRUE, 1100.0);
            }
        }
      printf ("    (%zu look-back dumps per epoch, %.1f kHz despread)\n\n",
              at_zero.segments,
              (double)at_zero.segments * CHIP_RATE / (double)SF * 1e-3);
      if (check)
        {
          DP_CHECK_MSG (at_zero.handed == at_zero.n
                            && at_off[0].handed == at_off[0].n
                            && at_off[1].handed == at_off[1].n,
                        "the refine hands over on every trial");
          /* Not 18: the hand-over test's comment said 19 dB sizes 18
             blocks at this C/N0, and the issue and §12.9 repeated it;
             det_n_noncoh() sizes 7 (18 needs 22 dB here). */
          DP_CHECK_MSG (at_zero.dwell == 7,
                        "margin 19 dB sizes a 7-block dwell at 45 dB-Hz");
          if (lb == 0)
            {
              DP_CHECK_MSG (at_zero.segments == 11,
                            "the shipped look-back gives 11 dumps per "
                            "epoch");
              /* The design's expectation (§12.10): on the shipped stream
                 the estimate is inside loop 1's pull-in (60 Hz, its own
                 header) from a seed the searcher's coarse row hands it. */
              DP_CHECK_MSG (fabs (at_zero.mean) < 60.0
                                && fabs (at_off[0].mean) < 60.0
                                && fabs (at_off[1].mean) < 60.0,
                            "the bias at 0 and +-1100 Hz is inside loop "
                            "1's pull-in on the shipped stream");
            }
          else
            {
              /* The defect this harness was built to see: on the
                 retired stream the estimate keeps about a third of the
                 seed's error, in the seed's direction. The check pins
                 that the harness sees it. */
              DP_CHECK_MSG (at_zero.segments == 1,
                            "100 dB gives one dump per epoch");
              DP_CHECK_MSG (at_off[0].mean > 200.0 && at_off[1].mean < -200.0,
                            "on the retired stream the bias at +-1100 Hz "
                            "is over 200 Hz toward the seed");
            }
        }
    }
  for (int sg = 0; sg < 2; sg++)
    for (size_t k = 0; k < N_SEEDS; k++)
      {
        free (g_cap[sg][k].x);
        free (g_cap[sg][k].data);
      }
  if (check)
    DP_TEST_END ("validate_refine_bias");
  return 0;
}
