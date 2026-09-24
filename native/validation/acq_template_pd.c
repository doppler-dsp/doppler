/**
 * @file acq_template_pd.c
 * @brief The Pd a TEMPLATE burst engine delivers, against the Pd it
 *        predicts (doppler#1470 phase 4 of the object lifecycle's Explore).
 *
 * acq_create_burst() searches any repeated complex preamble by its
 * samples, and sizes itself from `pd_predicted` -- a model whose inputs for a
 * template (the zone, the band-limited delay straddle) are pinned against
 * closed forms in test_acq_core.c, but whose OUTPUT had never been measured
 * over noise. acq's report §2.6 measures it for a code; this measures it for
 * four kinds of preamble, the same way:
 *
 *   one frame of D = 8 repetitions per trial, the grid pinned at D = 8 and
 *   one look (acq_configure_search_raw), the Doppler uniform over the
 *   native span +/- fs/(2n), the delay uniform and CONTINUOUS, AWGN from
 *   the shipped awgn at the design C/N0; a trial hits when the frame
 *   reports any detection.
 *
 * A template is defined only by its samples, so a fractional delay is the
 * band-limited one: a linear phase across the periodic template's FFT --
 * exact for a periodic signal, and the model the engine's numeric delay
 * straddle is built on. (A code's continuous signal is its chip train, so
 * the CONTROL row delays an oversampled chip train instead, exactly as
 * §2.6 does, and must land on §2.6's numbers: that is what says the
 * harness, not the engine, produced any gap the other rows show.)
 *
 * Each template, and the code, is measured at three design points -- the
 * C/N0 at which the engine itself first predicts Pd 0.3, 0.6 and 0.9 --
 * because one point cannot show a model whose slope is wrong. Each hit's delay
 * error is recorded too: a chirp and a Zadoff-Chu sequence move their
 * correlation peak along the ambiguity ridge under Doppler, and this is the
 * first number on how far.
 *
 * Each row is read against §2.6's bounds: measured Pd never below the
 * prediction by more than 2 sigma (never optimistic), never above it by more
 * than 0.15. Measured 2026-09-24 at 3000 trials a row: the control lands on
 * §2.6 (0.729 against its 0.740 +/- 0.025); QPSK and Zadoff-Chu are ~+0.04
 * conservative, the SHAPED QPSK +0.05..+0.10 and the chirp +0.08..+0.12 --
 * the chirp's peak slides along its ridge (~0.3 samples) instead of
 * shrinking, so the model's zero-delay rotation loss overstates it. The
 * shaped QPSK was -0.02..-0.03 OPTIMISTIC (doppler#1483) until the model
 * priced the CFAR reference its correlation energy off the peak inflates
 * (doppler#1501).
 *
 * A Doppler RATE is measured last (doppler#1482): a long Zadoff-Chu
 * preamble, sized by the engine, under a ramp -- once with the rate
 * withheld (the sizer picks D = 10 and promises a Pd the drift takes away)
 * and once with it given (the depth caps at f_epoch/sqrt(2 rate) = 4 and
 * the prediction holds). Measured 2026-09-24 at 3000 trials: 0.911
 * promised, 0.843 delivered; told, 0.418 promised, 0.455 delivered. (D was
 * 12 and 0.648 delivered until the sizer judged the burst, doppler#1498:
 * it no longer reaches so deep a block of 16.)
 *
 * Usage:
 *   validate_acq_template_pd            every template, every design point:
 *                                       reports each row's verdict, decides
 *                                       nothing (the certification does)
 *   validate_acq_template_pd --emit     the full sweep as CSV blocks, for
 *                                       acq's validate.py -- the report
 *                                       renders them and its limits()
 *                                       decide
 *   validate_acq_template_pd --check    the spot checks CTest runs, which
 *                                       ARE asserted: the control, the
 *                                       code at the 0.9 design point,
 *                                       Zadoff-Chu at the 0.6 design point,
 *                                       and the three drift rows at 500
 *                                       trials
 */
#include "acq/acq_core.h"
#include "awgn/awgn_core.h"
#include "dp_complex.h"
#include "dp_preamble_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include "fft/fft_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D 8u
#define TRIALS 3000
#define PFA 1e-3
#define NMAX 128u
/* Templates run at 1 MHz, not in normalized units: the burst constructors
   refuse cn0_dbhz < 0 (0 is "no design point"), and at fs = 1 C/N0 IS the
   per-sample SNR, which is negative at any interesting operating point.
   The physics is scale-free; only the dB-Hz numbers carry the 60 dB. */
#define FS_T 1.0e6

/* PN(mls_poly(5), seed=1), the code acq's §2.6 measures */
static const uint8_t CODE31[31]
    = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
        1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };

typedef struct
{
  const char *name;
  size_t      n;
  float _Complex t[NMAX]; /* one period, unit RMS */
} tmpl_t;

static void
unit_rms (float _Complex *t, size_t n)
{
  double e = 0.0;
  for (size_t i = 0; i < n; i++)
    e += (double)(crealf (t[i]) * crealf (t[i])
                  + cimagf (t[i]) * cimagf (t[i]));
  float g = (float)sqrt ((double)n / e);
  for (size_t i = 0; i < n; i++)
    t[i] *= g;
}

static size_t
templates (tmpl_t *out)
{
  uint32_t st = 1470u;
  size_t   k;
  out[0].name = "Zadoff-Chu 127";
  out[0].n    = 127;
  for (k = 0; k < 127; k++)
    out[0].t[k] = (float _Complex)cexp (-I * M_PI * 5.0 * (double)k
                                        * (double)(k + 1) / 127.0);
  out[1].name = "chirp 128";
  out[1].n    = 128;
  for (k = 0; k < 128; k++)
    out[1].t[k] = (float _Complex)cexp (I * M_PI * (double)(k * k) / 128.0);
  out[2].name = "QPSK 96";
  out[2].n    = 96;
  for (k = 0; k < 96; k++)
    out[2].t[k] = (float _Complex)cexp (I * M_PI / 2.0
                                        * (double)(dp_xs32 (&st) >> 30));
  /* the same QPSK, periodically low-passed: an envelope that varies */
  out[3].name = "QPSK 96 shaped";
  out[3].n    = 96;
  {
    const double w[5] = { 0.1, 0.25, 0.3, 0.25, 0.1 };
    for (k = 0; k < 96; k++)
      {
        float _Complex acc = 0;
        for (size_t j = 0; j < 5; j++)
          acc += (float)w[j] * out[2].t[(k + 96 + j - 2) % 96];
        out[3].t[k] = acc;
      }
  }
  for (size_t i = 0; i < 4; i++)
    unit_rms (out[i].t, out[i].n);
  return 4;
}

/* The grid every row is measured on: D repetitions, one look. A harness
   that cannot pin it has nothing to measure, so it stops. */
static void
pin (acq_state_t *a)
{
  if (acq_configure_search_raw (a, D, 1) != 0)
    {
      fprintf (stderr, "acq_configure_search_raw (D=%u, 1) refused\n", D);
      abort ();
    }
}

typedef struct
{
  double cn0, pred, meas, se, delay_err;
  size_t depth;
  int    ok;
} row_t;

/* The drift experiment (doppler#1482): how the engine is built and what
   the signal does. `reps` is the preamble; `pinned` pins the grid at D
   (the Pd rows) or leaves the auto-sizer's depth (the drift rows, where the
   bound acts THROUGH the sizer); `ramp` is the signal's Doppler rate in
   Hz/s, centred on the trial's frequency; `rate_arg` is the rate the
   constructor is told; `trials` per row. */
typedef struct
{
  size_t reps;
  int    pinned;
  double ramp, rate_arg;
  int    trials;
} geom_t;

static const geom_t PINNED = { D, 1, 0.0, 0.0, TRIALS };

/* Measure one design point. `code` != NULL runs the CONTROL: acq's §2.6,
   an oversampled chip train delayed in quarter-sample steps. A trial is
   one frame -- coherent_bins repetitions. */
static row_t
measure (const tmpl_t *tp, const uint8_t *code, double cn0, uint32_t seed,
         const geom_t *gm)
{
  const size_t spc = 4, os = 4;
  const size_t n  = code ? 31 * spc : tp->n;
  const double fs = code ? 1.0e6 * (double)spc : FS_T;
  /* The control's preamble is the code's samples (bin_to_nrz, held spc),
     the way every burst engine is built now (doppler#1470). */
  float _Complex *cpre = code ? dp_code_preamble (code, 31, spc) : NULL;
  acq_state_t    *a
      = dp_xnn (acq_create_burst (code ? cpre : tp->t, n, gm->reps, fs, cn0,
                                  0.0, PFA, 0.9, 0, gm->rate_arg));
  free (cpre);
  if (gm->pinned)
    pin (a);
  row_t r = { 0 };
  r.cn0   = cn0;
  r.pred  = a->pd_predicted;
  r.depth = a->coherent_bins;

  const size_t    len  = a->coherent_bins * n;
  float _Complex *x    = dp_xmalloc (len * sizeof *x);
  float _Complex *per  = dp_xmalloc (n * sizeof *per);
  float _Complex *nz   = dp_xmalloc (len * sizeof *nz);
  const double    span = fs / (2.0 * (double)n); /* Hz, as the engine */
  awgn_state_t   *g    = dp_xnn (awgn_create (
      seed, awgn_amplitude_for_snr ((float)(cn0 - 10.0 * log10 (fs)), 1.0f)));
  uint32_t        st   = seed;
  int             hits = 0;
  double          derr = 0.0;
  acq_result_t    h[16];

  for (int trial = 0; trial < gm->trials; trial++)
    {
      double f   = (2.0 * dp_uni (&st) - 1.0) * span;
      double tau = dp_uni (&st) * (double)n;
      if (code)
        {
          /* the chip train at os x the sample rate, rolled by a whole
             number of fine samples, decimated: §2.6's quarter-sample
             delay */
          size_t off = (size_t)(tau * (double)os) % (n * os);
          tau        = (double)off / (double)os;
          for (size_t i = 0; i < n; i++)
            {
              size_t fine = (i * os + n * os - off) % (n * os);
              per[i]      = (CODE31[fine / (spc * os)] & 1u) ? -1.0f : 1.0f;
            }
        }
      else
        dp_preamble_shift (tp->t, n, tau, per);
      if (gm->ramp == 0.0)
        for (size_t i = 0; i < len; i++)
          x[i] = per[i % n]
                 * (float _Complex)cexp (I * 2.0 * M_PI * f / fs * (double)i);
      else
        /* f at the frame's centre, sweeping at `ramp` Hz/s through it */
        for (size_t i = 0; i < len; i++)
          {
            double t = ((double)i - 0.5 * (double)len) / fs;
            x[i] = per[i % n]
                   * (float _Complex)cexp (I * 2.0 * M_PI
                                           * (f * t + 0.5 * gm->ramp * t * t));
          }
      awgn_generate (g, len, nz, len);
      for (size_t i = 0; i < len; i++)
        x[i] += nz[i];
      acq_reset (a);
      size_t nh = acq_push (a, x, len, h, 16);
      if (nh > 0)
        {
          hits++;
          double d = fabs ((double)h[0].code_phase - tau);
          if (d > (double)n / 2.0)
            d = (double)n - d;
          derr += d;
        }
    }
  r.meas      = (double)hits / gm->trials;
  r.se        = sqrt (fmax (r.meas * (1.0 - r.meas), 1e-9) / gm->trials);
  r.delay_err = hits ? derr / hits : NAN;
  r.ok        = r.meas >= r.pred - 2.0 * r.se && r.meas - r.pred <= 0.15;
  awgn_destroy (g);
  free (nz);
  free (per);
  free (x);
  acq_destroy (a);
  return r;
}

/* The C/N0 at which the engine first predicts `target`, pinned at D,
   scanned in quarter-dB steps: for a template (`pre` its samples at FS_T)
   and for the code (`pre` its held chips at the control's rate) alike. */
static double
cn0_for (const float _Complex *pre, size_t n, double fs, double target)
{
  /* Bisected on the quarter-dB grid 30..100 dB-Hz: Pd rises with C/N0 at a
     pinned grid, so this is the step a linear scan finds, in ~9
     constructions rather than ~60. Each one sizes a whole burst engine, and
     the scan alone put the spot check over its CI budget (doppler#1498). */
  int lo = 0, hi = 280;
  {
    acq_state_t *a = dp_xnn (acq_create_burst (
        pre, n, D, fs, 30.0 + 0.25 * (double)hi, 0.0, PFA, 0.9, 0, 0.0));
    pin (a);
    const double p = a->pd_predicted;
    acq_destroy (a);
    if (!(p >= target))
      return NAN;
  }
  while (lo < hi)
    {
      const int    mid = (lo + hi) / 2;
      acq_state_t *a   = dp_xnn (acq_create_burst (
          pre, n, D, fs, 30.0 + 0.25 * (double)mid, 0.0, PFA, 0.9, 0, 0.0));
      pin (a);
      const double p = a->pd_predicted;
      acq_destroy (a);
      if (p >= target)
        hi = mid;
      else
        lo = mid + 1;
    }
  return 30.0 + 0.25 * (double)lo;
}

/* --emit: CSV blocks for acq's validate.py, which renders the report and
   holds every threshold. The table and the CSV print the same rows. */
static int g_emit;

static void
print_row (const char *name, double target, const row_t *r)
{
  if (g_emit)
    printf ("%s,%.2f,%.4f,%.6f,%.6f,%.6f,%.4f\n", name, target, r->cn0,
            r->pred, r->meas, r->se, r->delay_err);
  else
    printf ("%-16s %7.2f  %6.3f  %6.3f  %5.3f  %5.2f  %s\n", name, r->cn0,
            r->pred, r->meas, r->se, r->delay_err,
            r->ok ? "inside" : "OUTSIDE");
}

/* doppler#1482: a Doppler RATE smears the carrier across slow-time rows
   during a block, a loss the Pd model does not carry. A long preamble (16
   repetitions of Zadoff-Chu 127 at 1 MS/s: f_epoch = 7874 Hz) is sized at
   a C/N0 where the sizer alone wants a deep block, then measured under a
   ramp twice: told nothing (rate 0, no bound) and told the rate (the bound
   caps the depth at floor(f_epoch / sqrt(2 rate)) = 4). A ramp-free row is
   the control: the model holds without drift. */
static int
drift_rows (const tmpl_t *zc, int check)
{
  const size_t reps = 16;
  const double rate = 1.5e6; /* Hz/s */
  /* The highest C/N0, on a quarter-dB grid down from 70 dB-Hz, at which the
     sizer alone picks a deep block -- deep whether or not it can also MEET
     pd there: sizing on the burst (doppler#1498) no longer buys a 12-deep
     block of 16 that meets it, since such a dwell straddles the preamble at
     most alignments. What this needs is only a depth well past the cap.
     Bisected: the depth grows as C/N0 falls over 70..30 dB-Hz, so this is
     the step a linear scan finds, in ~8 constructions rather than ~95. */
  int lo = 0, hi = 160; /* steps down from 70 dB-Hz */
  {
    acq_state_t *a = dp_xnn (acq_create_burst (zc->t, zc->n, reps, FS_T,
                                               70.0 - 0.25 * (double)hi, 0.0,
                                               PFA, 0.9, 0, 0.0));
    const int    deep = a->coherent_bins >= 10;
    acq_destroy (a);
    DP_REQUIRE (deep);
  }
  while (lo < hi)
    {
      const int    mid  = (lo + hi) / 2;
      acq_state_t *a    = dp_xnn (acq_create_burst (zc->t, zc->n, reps, FS_T,
                                                    70.0 - 0.25 * (double)mid,
                                                    0.0, PFA, 0.9, 0, 0.0));
      const int    deep = a->coherent_bins >= 10;
      acq_destroy (a);
      if (deep)
        hi = mid;
      else
        lo = mid + 1;
    }
  const double cn0 = 70.0 - 0.25 * (double)lo;

  /* The spot check needs far fewer trials than the table: the blind row
     misses its promise by ~0.27, 13 sigma at 500. */
  const int    nt    = check ? 500 : TRIALS;
  const geom_t still = { reps, 0, 0.0, 0.0, nt };
  const geom_t blind = { reps, 0, rate, 0.0, nt };
  const geom_t told  = { reps, 0, rate, rate, nt };
  row_t        r0    = measure (zc, NULL, cn0, 1482u, &still);
  row_t        r1    = measure (zc, NULL, cn0, 1483u, &blind);
  row_t        r2    = measure (zc, NULL, cn0, 1484u, &told);

  if (g_emit)
    printf ("# drift %s,%zu,%.6g,%d\nrow,cn0,depth,pred,meas,se\n", zc->name,
            reps, rate, nt);
  else
    {
      printf ("\nDoppler rate %.3g Hz/s, %zu repetitions of %s, sized by "
              "the engine (D is its coherent depth), %d trials per row\n",
              rate, reps, zc->name, nt);
      printf ("%-24s %7s  %3s  %6s  %6s  %5s  %s\n", "", "C/N0", "D", "pred",
              "meas", "1sig", "never optimistic?");
    }
  const struct
  {
    const char  *name;
    const row_t *r;
  } rows[3] = { { "no ramp (control)", &r0 },
                { "ramp (rate not given)", &r1 },
                { "ramp (rate given)", &r2 } };
  for (size_t i = 0; i < 3; i++)
    {
      const row_t *r = rows[i].r;
      if (g_emit)
        printf ("%s,%.4f,%zu,%.6f,%.6f,%.6f\n", rows[i].name, r->cn0, r->depth,
                r->pred, r->meas, r->se);
      else
        printf ("%-24s %7.2f  %3zu  %6.3f  %6.3f  %5.3f  %s\n", rows[i].name,
                r->cn0, r->depth, r->pred, r->meas, r->se,
                r->meas >= r->pred - 2.0 * r->se ? "yes" : "NO");
    }
  if (check)
    {
      /* The control holds, the blind engine promises a Pd the ramp takes
         away, and the told one keeps its promise at the capped depth. */
      DP_CHECK (r0.meas >= r0.pred - 2.0 * r0.se);
      DP_CHECK (r1.meas < r1.pred - 2.0 * r1.se);
      DP_CHECK (r2.depth == 4);
      DP_CHECK (r2.meas >= r2.pred - 2.0 * r2.se);
    }
  return 0;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  g_emit    = (argc > 1 && strcmp (argv[1], "--emit") == 0);
  tmpl_t tp[4];
  size_t nt = templates (tp);

  if (g_emit)
    printf ("# pd %u,%d,%g\npreamble,target,cn0,pred,meas,se,delay_err\n", D,
            TRIALS, PFA);
  else
    {
      printf ("D = %u, one look, %d trials per row, pfa %g; bounds: "
              "measured >= predicted - 2 sigma, measured - predicted <= "
              "0.15\n\n",
              D, TRIALS, PFA);
      printf ("%-16s %7s  %6s  %6s  %5s  %5s\n", "preamble", "C/N0", "pred",
              "meas", "1sig", "|dly|");
    }

  /* the control: acq's §2.6 point, through this harness */
  row_t ctl = measure (NULL, CODE31, 50.0, 1183u, &PINNED);
  print_row ("code 31 x4 (ctl)", 0.0, &ctl);
  if (check)
    DP_CHECK (ctl.ok);

  const double targets[3] = { 0.3, 0.6, 0.9 };
  /* The code at the same three design points. The control above is one
     point, 0.65, and the constructors default to pd = 0.9 -- the point a
     caller sizes at, and where a model's margin is thinnest. The spot check
     asserts that one. */
  {
    const size_t    spc  = 4;
    float _Complex *cpre = dp_code_preamble (CODE31, 31, spc);
    for (size_t j = 0; j < 3; j++)
      {
        if (check && j != 2)
          continue;
        double c = cn0_for (cpre, 31 * spc, 1.0e6 * (double)spc, targets[j]);
        DP_REQUIRE (!isnan (c));
        row_t r = measure (NULL, CODE31, c, (uint32_t)(1183u + j), &PINNED);
        print_row ("code 31 x4", targets[j], &r);
        if (check)
          DP_CHECK (r.ok);
      }
    free (cpre);
  }
  for (size_t i = 0; i < nt; i++)
    for (size_t j = 0; j < 3; j++)
      {
        if (check && (i != 0 || j != 1))
          continue;
        double c = cn0_for (tp[i].t, tp[i].n, FS_T, targets[j]);
        DP_REQUIRE (!isnan (c));
        row_t r = measure (&tp[i], NULL, c, (uint32_t)(1470u + 7u * i + j),
                           &PINNED);
        print_row (tp[i].name, targets[j], &r);
        if (check)
          DP_CHECK (r.ok);
      }
  (void)drift_rows (&tp[0], check);
  if (g_emit) /* stdout is data: DP_TEST_END's banner would corrupt it */
    DP_TEST_EMIT_END ("validate_acq_template_pd");
  DP_TEST_END ("validate_acq_template_pd");
}
