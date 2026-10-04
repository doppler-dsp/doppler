/* bench.c — what the 2-D correlation detector and its parts cost on THIS
 * machine, through doppler's installed public API only.
 *
 *     uno_q_bench [ROUNDS]            (default 60; `make bench` runs it)
 *
 * It prints one row per case: the algorithm, the case, and a rate, in the
 * same shape as the table in README.md ("On an Arduino UNO Q"), so a re-run
 * on another board or another build can be pasted straight over it.
 *
 *   detector2d   the whole detector: correlate, ring, peak search, noise
 *   corr2d       its transform alone, fast path and general path
 *   fft2d, fft   the transforms underneath it
 *   fir          a block filter, real taps against complex taps
 *
 * METHOD. The same three rules as doppler's own native/benchmarks (which
 * are not installed, so they cannot be linked from here):
 *
 *   - MIN over rounds, never the mean. Interrupts, migrations and clock
 *     steps only ever ADD time, so the minimum is the least-biased cost.
 *   - SETTLE once, untimed, before anything is measured, so the first case
 *     does not pay for the governor ramping up from idle.
 *   - Rounds on the OUTSIDE, cases on the inside, so whatever drift
 *     remains lands on every case instead of on the one timed first.
 *
 * Single thread throughout. Every case is a single-thread kernel, because
 * what a board has to spare is cores, and one core is the number that
 * scales.
 */
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "doppler/corr2d/corr2d_core.h"
#include "doppler/detector2d/detector2d_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/fft2d/fft2d_core.h"
#include "doppler/fir/fir_core.h"

#define DEFAULT_ROUNDS 60
#define SETTLE_S 0.25
#define MIN_ROUND_S 1e-3
#define FFT_FORWARD (-1)
#define MAX_RESULTS 64

/* One measured case. `run` does ONE round of work on `ctx`; `units` is how
   many samples (or bins) that round processes, so rate = units / time. */
typedef struct
{
  const char *algo;
  const char *what;
  const char *unit; /* "MSa/s" or "Mbin/s" */
  size_t      units;
  void (*run) (void *ctx);
  void  *ctx;
  size_t reps; /* calls per timed round, so a round lasts >= MIN_ROUND_S */
  double best; /* fastest round, per call, seconds */
} bench_case_t;

static double
now_s (void)
{
  struct timespec ts;
  (void)clock_gettime (CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* A smooth chirp: a real correlation peak, and nothing the hardware can
   shortcut (a constant or zero input invites denormals). */
static void
chirp (float _Complex *x, size_t n, double rate)
{
  for (size_t i = 0; i < n; i++)
    {
      const double p = rate * (double)i * (double)i;
      x[i]           = (float _Complex) (cos (p) + sin (p) * I);
    }
}

static void *
must (void *p)
{
  if (!p)
    {
      (void)fprintf (stderr, "uno_q_bench: allocation or create() failed\n");
      exit (1);
    }
  return p;
}

/* ── detector2d: 4 frames of 16384 bins per round ────────────────────── */

#define DET_BINS 16384
#define DET_FRAMES 4

typedef struct
{
  dp_detector2d_state_t *det;
  const float _Complex  *in;
  det_result2d_t         res[MAX_RESULTS];
} det_ctx_t;

static void
run_detector (void *c)
{
  det_ctx_t *d = c;
  dp_detector2d_reset (d->det);
  for (int f = 0; f < DET_FRAMES; f++)
    (void)dp_detector2d_push (d->det, d->in + (size_t)f * DET_BINS, DET_BINS,
                              d->res, MAX_RESULTS);
}

/* ── corr2d: one dump of a 16 x 2046 grid ────────────────────────────── */

#define C2_NY 16
#define C2_NX 2046

typedef struct
{
  dp_corr2d_state_t *obj;
  float _Complex    *in, *out;
} corr_ctx_t;

static void
run_corr2d (void *c)
{
  corr_ctx_t  *k = c;
  const size_t n = (size_t)C2_NY * C2_NX;
  dp_corr2d_execute (k->obj, k->in, n, k->out, n);
}

/* ── fft / fft2d, forward, complex float ─────────────────────────────── */

typedef struct
{
  dp_fft_state_t   *p1;
  dp_fft2d_state_t *p2;
  size_t            n;
  float _Complex   *in, *out;
} fft_ctx_t;

static void
run_fft (void *c)
{
  fft_ctx_t *f = c;
  dp_fft_execute_cf32 (f->p1, f->in, f->n, f->out, f->n);
}

static void
run_fft2d (void *c)
{
  fft_ctx_t *f = c;
  dp_fft2d_execute_cf32 (f->p2, f->in, f->n, f->out, f->n);
}

/* ── fir: one block of 65536 samples ─────────────────────────────────── */

#define FIR_BLOCK 65536

typedef struct
{
  dp_fir_state_t       *fir;
  const float _Complex *in;
  float _Complex       *out;
} fir_ctx_t;

static void
run_fir (void *c)
{
  fir_ctx_t *f = c;
  dp_fir_execute (f->fir, f->in, FIR_BLOCK, f->out);
}

/* A windowed sinc: real coefficients with a realistic mix of magnitudes and
   signs. The complex filter gets the SAME coefficients with zero imaginary
   parts, which is what a caller gets by handing real taps to the complex
   constructor — the mistake this row prices. */
static void
sinc_taps (float *t, size_t n)
{
  for (size_t k = 0; k < n; k++)
    {
      const double m = (double)k - (double)(n - 1) / 2.0;
      const double s = (m == 0.0) ? 1.0 : sin (M_PI * 0.25 * m) / (M_PI * m);
      t[k]           = (float)(s
                               * (0.54
                                  - 0.46
                                        * cos (2.0 * M_PI * (double)k
                                               / (double)(n - 1))));
    }
}

#define MAX_CASES 24

int
main (int argc, char **argv)
{
  const long   arg    = argc > 1 ? strtol (argv[1], NULL, 10) : 0;
  const int    rounds = arg > 0 ? (int)arg : DEFAULT_ROUNDS;
  bench_case_t cases[MAX_CASES];
  size_t       nc = 0;

#define ADD(A, W, U, N, F, C)                                                 \
  cases[nc++] = (bench_case_t){ A, W, U, N, F, C, 1, 1e30 }

  /* detector2d — the 16 x 1024 and 128 x 128 shapes of 16384 bins. */
  float _Complex *dref = must (malloc (DET_BINS * sizeof *dref));
  float _Complex *din
      = must (malloc ((size_t)DET_FRAMES * DET_BINS * sizeof *din));
  chirp (dref, DET_BINS, 1e-5);
  for (int f = 0; f < DET_FRAMES; f++)
    chirp (din + (size_t)f * DET_BINS, DET_BINS, 1e-5);
  static det_ctx_t    det[2];
  static const size_t dny[2] = { 16, 128 }, dnx[2] = { 1024, 128 };
  static char         dname[2][32];
  for (int i = 0; i < 2; i++)
    {
      det[i].in  = din;
      det[i].det = must (dp_detector2d_create (
          dref, dny[i], dnx[i], 1, 1, DET_BINS - 1, DET_NOISE_MEAN, 0.0f, 1));
      (void)snprintf (dname[i], sizeof dname[i], "%zu x %zu bins", dny[i],
                      dnx[i]);
      ADD ("detector2d", dname[i], "MSa/s", (size_t)DET_FRAMES * DET_BINS,
           run_detector, &det[i]);
    }

  /* corr2d — a single-row reference takes the fast path; a full-grid one
     takes the general 2-D path. */
  const size_t       cn = (size_t)C2_NY * C2_NX;
  static corr_ctx_t  cor[2];
  static const char *cname[2]
      = { "16 x 2046, single-row ref", "16 x 2046, multi-row ref" };
  for (int i = 0; i < 2; i++)
    {
      float _Complex *ref = must (calloc (cn, sizeof *ref));
      chirp (ref, i == 0 ? C2_NX : cn, 1e-5);
      cor[i].in  = must (malloc (cn * sizeof *cor[i].in));
      cor[i].out = must (malloc (cn * sizeof *cor[i].out));
      chirp (cor[i].in, cn, 3e-6);
      cor[i].obj = must (dp_corr2d_create (ref, C2_NY, C2_NX, 1, 1, 0, 0, -1));
      free (ref);
      ADD ("corr2d", cname[i], "MSa/s", cn, run_corr2d, &cor[i]);
    }

  /* fft2d and fft — complex float, forward. */
  static fft_ctx_t    f2[2], f1[3];
  static const size_t f2y[2] = { 256, 16 }, f2x[2] = { 256, 4096 };
  static const size_t f1n[3] = { 256, 4096, 65536 };
  static char         f2name[2][32], f1name[3][32];
  for (int i = 0; i < 2; i++)
    {
      f2[i].n   = f2y[i] * f2x[i];
      f2[i].in  = must (malloc (f2[i].n * sizeof *f2[i].in));
      f2[i].out = must (malloc (f2[i].n * sizeof *f2[i].out));
      chirp (f2[i].in, f2[i].n, 1e-6);
      f2[i].p2 = must (dp_fft2d_create (f2y[i], f2x[i], FFT_FORWARD, 1));
      (void)snprintf (f2name[i], sizeof f2name[i], "%zu x %zu", f2y[i],
                      f2x[i]);
      ADD ("fft2d (cf32, forward)", f2name[i], "Mbin/s", f2[i].n, run_fft2d,
           &f2[i]);
    }
  for (int i = 0; i < 3; i++)
    {
      f1[i].n   = f1n[i];
      f1[i].in  = must (malloc (f1n[i] * sizeof *f1[i].in));
      f1[i].out = must (malloc (f1n[i] * sizeof *f1[i].out));
      chirp (f1[i].in, f1n[i], 1e-6);
      f1[i].p1 = must (dp_fft_create (f1n[i], FFT_FORWARD, 1));
      (void)snprintf (f1name[i], sizeof f1name[i], "n = %zu", f1n[i]);
      ADD ("fft (cf32, forward)", f1name[i], "Mbin/s", f1n[i], run_fft,
           &f1[i]);
    }

  /* fir — real taps and complex taps at three lengths. */
  static const size_t taps[3] = { 15, 63, 255 };
  static fir_ctx_t    fr[6];
  static char         fname[6][32];
  float              *rt   = must (malloc (taps[2] * sizeof *rt));
  float _Complex     *ct   = must (malloc (taps[2] * sizeof *ct));
  float _Complex     *fin  = must (malloc (FIR_BLOCK * sizeof *fin));
  float _Complex     *fout = must (malloc (FIR_BLOCK * sizeof *fout));
  chirp (fin, FIR_BLOCK, 1e-6);
  for (int l = 0; l < 3; l++)
    {
      sinc_taps (rt, taps[l]);
      for (size_t k = 0; k < taps[l]; k++)
        ct[k] = (float _Complex)rt[k];
      for (int kind = 0; kind < 2; kind++)
        {
          fir_ctx_t *f = &fr[l * 2 + kind];
          f->in        = fin;
          f->out       = fout;
          f->fir       = must (kind == 0 ? dp_fir_create_real (rt, taps[l])
                                         : dp_fir_create (ct, taps[l]));
          (void)snprintf (fname[l * 2 + kind], sizeof fname[0], "%zu taps",
                          taps[l]);
        }
    }
  /* Listed by tap kind, so the real rows and the complex rows read as two
     blocks and the ratio between them is a look down the column. */
  for (int kind = 0; kind < 2; kind++)
    for (int l = 0; l < 3; l++)
      ADD (kind == 0 ? "fir, real taps" : "fir, complex taps",
           fname[l * 2 + kind], "MSa/s", FIR_BLOCK, run_fir,
           &fr[l * 2 + kind]);

  /* Settle: untimed work until the clock has stopped ramping. */
  for (double w0 = now_s (); now_s () - w0 < SETTLE_S;)
    cases[0].run (cases[0].ctx);

  /* A call of a few microseconds is mostly clock and call overhead, so a
     short case repeats inside one timed round until the round lasts at
     least MIN_ROUND_S; the reported time is per call. */
  for (size_t i = 0; i < nc; i++)
    {
      const double t0 = now_s ();
      cases[i].run (cases[i].ctx);
      const double dt = now_s () - t0;
      if (dt < MIN_ROUND_S)
        cases[i].reps = (size_t)(MIN_ROUND_S / (dt > 1e-7 ? dt : 1e-7)) + 1;
    }

  for (int r = 0; r < rounds; r++)
    for (size_t i = 0; i < nc; i++)
      {
        const double t0 = now_s ();
        for (size_t k = 0; k < cases[i].reps; k++)
          cases[i].run (cases[i].ctx);
        const double dt = (now_s () - t0) / (double)cases[i].reps;
        if (dt < cases[i].best)
          cases[i].best = dt;
      }

  (void)printf ("uno_q_bench: single thread, min over %d rounds\n\n", rounds);
  (void)printf ("| %-24s | %-26s | %12s |\n", "algorithm", "case", "rate");
  (void)printf ("| %-24s | %-26s | %12s |\n", "------------------------",
                "--------------------------", "------------");
  const char *last = "";
  for (size_t i = 0; i < nc; i++)
    {
      char rate[32];
      (void)snprintf (rate, sizeof rate, "%.1f %s",
                      (double)cases[i].units / cases[i].best / 1e6,
                      cases[i].unit);
      const int same = strcmp (cases[i].algo, last) == 0;
      (void)printf ("| %-24s | %-26s | %12s |\n", same ? "" : cases[i].algo,
                    cases[i].what, rate);
      last = cases[i].algo;
    }
  return 0;
}
