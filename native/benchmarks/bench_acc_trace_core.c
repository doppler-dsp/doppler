/* bench_acc_trace_core.c — spectrum-trace accumulation, per mode.
 *
 * A jm scaffold that recorded nothing until now (doppler#891). This is the
 * analyzer's per-frame reducer: every FFT frame a spectrum display shows
 * goes through `dp_acc_trace_accumulate`, so its cost is paid at the frame
 * rate for as long as the display is open.
 *
 * The four modes do different arithmetic per bin, and a caller picks one:
 *
 *   mean      running linear mean -- an add and a scale
 *   exp       exponential moving average, through the shared `ema_step`
 *   maxhold   a compare and a conditional store, per bin
 *   minhold   the same, other direction
 *
 * The interesting question is whether the two hold modes are cheaper than
 * the two averaging ones (a compare against a multiply-add) or whether the
 * branch costs more than the arithmetic it avoids. `value()` is measured
 * separately because a display reads it far less often than it accumulates.
 *
 *   fold[<mode>,nfft=N]
 *                      per FRAME, at the five frame sizes PSD's per-frame
 *                      kernel is measured at (bench_psd_core.c): the fold
 *                      PSD's accumulate runs on every frame, so
 *                      accumulate_frame ~ frame_power + fold[mean] there
 *                      (#2094's first step, the baseline the SIMD fold is
 *                      measured against). Each round resets the trace and
 *                      seeds it untimed, so every timed call is a fold and
 *                      never the seeding copy; the four modes and five
 *                      sizes are interleaved, the modes rotating per round.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "doppler/acc_trace/acc_trace_core.h"
#include "dp_bench.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* A 4096-bin trace is a typical analyzer FFT; FRAMES per round keeps the
   per-call overhead out of the per-bin number. */
#define NBINS 4096
#define FRAMES 16
#define ITERATIONS 100
#define FROUNDS 30 /* the fold section's rounds */
#define NFOLD 5    /* its frame sizes, PSD's kernel sizes */

/* One size of the fold section: a trace per mode, FRAMES + 1 distinct
   frames (the first seeds), and per-round timings per mode. */
typedef struct
{
  size_t                n;
  dp_acc_trace_state_t *a[4];
  float                *frames;
  double                t[4][FROUNDS];
} fold_t;

/* Reset, seed untimed, then time FRAMES folds: what is timed is a fold of
   every bin, never the first frame's copy. */
static double
fold_round (fold_t *f, int m)
{
  dp_acc_trace_reset (f->a[m]);
  dp_acc_trace_accumulate (f->a[m], f->frames, f->n);
  const uint64_t t0 = jm_bench_now_ns ();
  for (int k = 1; k <= FRAMES; k++)
    dp_acc_trace_accumulate (f->a[m], f->frames + (size_t)k * f->n, f->n);
  return jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  uint64_t        t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile double sink   = 0.0;

  float *frame = malloc (NBINS * sizeof *frame);
  float *out   = malloc (NBINS * sizeof *out);
  if (!frame || !out)
    return 1;

  /* FRAMES DISTINCT frames, not one frame fed FRAMES times. That
     distinction is the whole measurement for the hold modes: replaying a
     single frame means every compare after the first fails, the update
     branch is never taken, and maxhold reads as a pure compare loop that
     no real display would ever see. Each frame here is an independent
     draw, so roughly 1/f of the bins update on frame f -- the shape a
     drifting spectrum actually has. */
  float *frames = malloc ((size_t)FRAMES * NBINS * sizeof *frames);
  if (!frames)
    return 1;
  uint32_t lfsr = 0x9E3Du;
  for (size_t i = 0; i < (size_t)FRAMES * NBINS; i++)
    {
      lfsr      = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      frames[i] = (float)((lfsr & 0xFFFFu) / 65535.0);
    }
  for (int i = 0; i < NBINS; i++)
    frame[i] = frames[i];

  printf ("=== acc_trace benchmark ===\n");
  printf ("%d bins x %d frames/round, %d rounds\n\n", NBINS, FRAMES,
          ITERATIONS);

  const int modes[4] = { ACC_TRACE_MEAN, ACC_TRACE_EXP, ACC_TRACE_MAXHOLD,
                         ACC_TRACE_MINHOLD };
  const char *const mname[4]
      = { "accumulate[mean]", "accumulate[exp]", "accumulate[maxhold]",
          "accumulate[minhold]" };
  static double t_acc[4][ITERATIONS];

  for (int m = 0; m < 4; m++)
    {
      dp_acc_trace_state_t *a = dp_acc_trace_create (NBINS, modes[m], 0.1);
      if (!a)
        {
          (void)fprintf (stderr, "bench_acc_trace: create(mode=%d) NULL\n",
                         modes[m]);
          return 1;
        }
      for (int r = 0; r < ITERATIONS; r++)
        {
          dp_acc_trace_reset (a);
          t0 = jm_bench_now_ns ();
          for (int f = 0; f < FRAMES; f++)
            dp_acc_trace_accumulate (a, frames + (size_t)f * NBINS, NBINS);
          t1          = jm_bench_now_ns ();
          t_acc[m][r] = jm_bench_elapsed_sec (t0, t1);
        }
      jm_bench_add (&_bench, mname[m], t_acc[m], ITERATIONS, NBINS * FRAMES);
      double s = min_sec (t_acc[m], ITERATIONS) / (FRAMES * (double)NBINS);
      printf ("  %-22s %7.3f ns/bin  %8.1f Mbin/s\n", mname[m], s * 1e9,
              1.0 / s / 1e6);
      dp_acc_trace_destroy (a);
    }

  dp_acc_trace_state_t *a = dp_acc_trace_create (NBINS, ACC_TRACE_MEAN, 0.1);
  if (!a)
    return 1;
  for (int f = 0; f < FRAMES; f++)
    dp_acc_trace_accumulate (a, frames + (size_t)f * NBINS, NBINS);
  static double t_val[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      sink += (double)dp_acc_trace_value (a, NBINS, out, NBINS);
      t1       = jm_bench_now_ns ();
      t_val[r] = jm_bench_elapsed_sec (t0, t1);
    }
  jm_bench_add (&_bench, "value", t_val, ITERATIONS, NBINS);
  printf ("  %-22s %7.3f ns/bin  (read-out, once per display refresh)\n",
          "value", min_sec (t_val, ITERATIONS) / NBINS * 1e9);
  dp_acc_trace_destroy (a);

  printf ("\n  maxhold/mean = %.2fx, minhold/mean = %.2fx -- a\n"
          "  compare-and-conditional-store against an unconditional\n"
          "  multiply-add, over %d INDEPENDENT frames so the update branch\n"
          "  is exercised at a realistic rate. Any remaining gap between\n"
          "  the two hold directions is branch behaviour on this stimulus,\n"
          "  not a difference in the arithmetic. `exp` runs the shared\n"
          "  ema_step primitive (bench_util_core.c), so a change there\n"
          "  moves this row.\n",
          min_sec (t_acc[2], ITERATIONS) / min_sec (t_acc[0], ITERATIONS),
          min_sec (t_acc[3], ITERATIONS) / min_sec (t_acc[0], ITERATIONS),
          FRAMES);

  /* ── the fold at PSD's frame sizes (#2094) ── */
  {
    static const size_t fsz[NFOLD]  = { 256, 1024, 4096, 16384, 65536 };
    static const char  *fmode[4]    = { "mean", "exp", "maxhold", "minhold" };
    fold_t              fold[NFOLD] = { 0 };
    for (int s = 0; s < NFOLD; s++)
      {
        fold_t *f = &fold[s];
        f->n      = fsz[s];
        f->frames = malloc ((size_t)(FRAMES + 1) * f->n * sizeof *f->frames);
        if (!f->frames)
          return 1;
        /* independent draws, as above, so the hold modes update at a
           realistic rate rather than never */
        for (size_t i = 0; i < (size_t)(FRAMES + 1) * f->n; i++)
          {
            lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
            f->frames[i] = (float)((lfsr & 0xFFFFu) / 65535.0);
          }
        for (int m = 0; m < 4; m++)
          if (!(f->a[m] = dp_acc_trace_create (f->n, modes[m], 0.1)))
            return 1;
      }
    DP_BENCH_SETTLE ((void)fold_round (&fold[1], 0));
    printf ("\n");
    for (int r = 0; r < FROUNDS; r++)
      for (int s = 0; s < NFOLD; s++)
        for (int q = 0; q < 4; q++)
          {
            const int m     = (q + r) % 4;
            fold[s].t[m][r] = fold_round (&fold[s], m);
          }
    for (int s = 0; s < NFOLD; s++)
      {
        for (int m = 0; m < 4; m++)
          {
            char name[64];
            (void)snprintf (name, sizeof name, "fold[%s,nfft=%zu]", fmode[m],
                            fold[s].n);
            dp_bench_record (&_bench, name, fold[s].t[m], FROUNDS, FRAMES,
                             "frame");
            dp_acc_trace_destroy (fold[s].a[m]);
          }
        free (fold[s].frames);
      }
  }

  /* Every row RECORDS, or nothing is written: jm_bench.h drops entries past
     JM_BENCH_MAX_ENTRIES without a word (just-buildit/just-makeit#2188), so
     the count is checked against the one the sections above derive (#2062):
     4 modes and value(), then 4 modes at NFOLD sizes. */
  const int want = 4 + 1 + 4 * NFOLD;
  if (_bench.count != want)
    {
      (void)fprintf (stderr,
                     "bench_acc_trace: recorded %d rows of %d; writing none\n",
                     _bench.count, want);
      return 1;
    }
  (void)sink;
  free (frame);
  free (frames);
  free (out);
  jm_bench_write_json (&_bench, "acc_trace");
  return 0;
}
