/* bench_psd_core.c — Welch accumulation, and the read-out that is not free.
 *
 * A jm scaffold that recorded nothing until now (doppler#891). `psd` is
 * what a spectrum display and every noise-floor measurement runs, so its
 * accumulate path is paid at the sample rate.
 *
 * Two shapes, deliberately reported in different units:
 *
 *   accumulate         per SAMPLE -- window, FFT, magnitude, trace update.
 *                      This is the one that has to keep up with a stream.
 *   accumulate_real    the same for a real input, which is half the FFT
 *                      work in principle -- whether it is in practice is
 *                      the question.
 *   power_onesided     per CALL -- the read-out a display does once per
 *                      refresh, not once per sample. Reported per call
 *                      because quoting it per sample would flatter it by
 *                      the number of samples that went in. A round times
 *                      a batch of calls (RD_SPAN), not one: a single call
 *                      at 1024 bins is about 15 steps of the clock, so its
 *                      cell moved in 7% steps (#2143).
 *   fft / frame_power / frame_linear / frame_db / accumulate_frame
 *                      per FRAME, the Spectrogram's row kernel and the
 *                      transform inside it (#1894 A4, the design's U3), and
 *                      one frame of PSD's own accumulate (#2094's first
 *                      step). Differences between rows of one run:
 *                        frame_power - fft     the window and the power/
 *                                              shift pass together (not
 *                                              split: #2094's fusion
 *                                              rewrites both)
 *                        frame_linear - frame_power
 *                                              the normalisation, per-bin
 *                                              in double
 *                        frame_db - frame_linear
 *                                              log10 and the floor: what a
 *                                              power row skips and a fast
 *                                              dB conversion changes
 *                        accumulate_frame / fft
 *                                              #2094's measure of a frame
 *                      frame_linear runs at 256, 1024 and 65536 only (the
 *                      32-row cap). accumulate_frame - frame_power is NOT
 *                      bench_acc_trace_core.c's fold[mean]: in the pipeline
 *                      the window, FFT and power passes run first and evict
 *                      the trace, while that row folds into a trace just
 *                      seeded, so hot. They are two cache regimes, and the
 *                      fold row is the fold's own cost.
 *                      `fft` mirrors PSD's own call -- dp_fft_create (nfft,
 *                      -1, 1), then dp_fft_execute_cf32, as psd_core.c
 *                      creates and psd_transform runs it -- so a change to
 *                      that call (#2094's fusion) changes this row too.
 *                      The timer's own cost is about 1-2% of a pass at nfft
 *                      256 and less above. Measured in one interleaved
 *                      loop, the kinds rotating per round.
 *
 * Swept over nfft, because the FFT is n log n and the windowing is n, so
 * the per-sample cost should rise with the transform size -- and how fast
 * it rises is what sets an analyzer's resolution-vs-throughput trade.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "doppler/dp_complex.h"
#include "doppler/fft/fft_core.h"
#include "doppler/psd/psd_core.h"
#include "dp_bench.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK 65536
#define ITERATIONS 50
#define KROUNDS 30 /* the per-frame kernel section's rounds */
#define NKERN 5    /* its transform sizes */
#define NKIND 5    /* fft, frame_power, frame_db, accumulate, frame_linear */
/* power_onesided's round: as many calls as read RD_SPAN bins' worth of
   spectrum, at least 4, so a round is a few us at every nfft and one clock
   step a fraction of a percent of it */
#define RD_SPAN 16384

/* One timed pass of the per-frame section: `reps` frames of one kind. */
typedef struct
{
  size_t          nfft, reps;
  dp_psd_state_t *p;
  dp_fft_state_t *plan;
  float _Complex *spec;
  float          *out;
  int             lin; /* frame_linear is timed at this size */
  double          t[NKIND][KROUNDS];
} kern_t;

static void
kern_run (kern_t *k, int kind, const float _Complex *x)
{
  for (size_t r = 0; r < k->reps; r++)
    {
      const float _Complex *f = x + (r * k->nfft) % (BLOCK * 8 - k->nfft);
      if (kind == 0)
        dp_fft_execute_cf32 (k->plan, f, k->nfft, k->spec, k->nfft);
      else if (kind == 1)
        dp_psd_frame_power (k->p, f, k->out);
      else if (kind == 2)
        dp_psd_frame_db (k->p, f, k->out);
      else if (kind == 3)
        dp_psd_accumulate (k->p, f, k->nfft); /* one frame: n = nfft */
      else
        dp_psd_frame_linear (k->p, f, k->out);
    }
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

  float _Complex *x  = malloc (BLOCK * sizeof *x);
  float          *xr = malloc (BLOCK * sizeof *xr);
  if (!x || !xr)
    return 1;
  for (int i = 0; i < BLOCK; i++)
    {
      double p = 0.01 * i;
      x[i]     = (float)cos (p) + (float)sin (p * 1.7) * I;
      xr[i]    = (float)cos (p);
    }

  printf ("=== psd benchmark ===\n");
  printf ("block = %d samples, %d rounds\n\n", BLOCK, ITERATIONS);

  const size_t  nffts[3] = { 1024, 4096, 16384 };
  static double t_acc[3][ITERATIONS], t_real[3][ITERATIONS];

  for (int k = 0; k < 3; k++)
    {
      /* window 0, pad 1, full_scale 1.0, bits 0, mode 0 (linear mean),
         alpha 0.1 -- the shape test_psd_core.c constructs. */
      dp_psd_state_t *p
          = dp_psd_create (nffts[k], 1.0e6, 0, 0.0f, 1, 1.0, 0, 0, 0.1);
      if (!p)
        {
          (void)fprintf (stderr, "bench_psd: dp_psd_create(nfft=%zu) NULL\n",
                         nffts[k]);
          return 1;
        }
      char name[64];

      for (int r = 0; r < ITERATIONS; r++)
        {
          t0 = jm_bench_now_ns ();
          dp_psd_accumulate (p, x, BLOCK);
          t1          = jm_bench_now_ns ();
          t_acc[k][r] = jm_bench_elapsed_sec (t0, t1);
        }
      (void)snprintf (name, sizeof name, "accumulate[nfft=%zu]", nffts[k]);
      jm_bench_add (&_bench, name, t_acc[k], ITERATIONS, BLOCK);
      printf ("  %-26s %7.2f ns/sample  %8.1f MSa/s\n", name,
              min_sec (t_acc[k], ITERATIONS) / BLOCK * 1e9,
              (double)BLOCK / min_sec (t_acc[k], ITERATIONS) / 1e6);

      for (int r = 0; r < ITERATIONS; r++)
        {
          t0 = jm_bench_now_ns ();
          dp_psd_accumulate_real (p, xr, BLOCK);
          t1           = jm_bench_now_ns ();
          t_real[k][r] = jm_bench_elapsed_sec (t0, t1);
        }
      (void)snprintf (name, sizeof name, "accumulate_real[nfft=%zu]",
                      nffts[k]);
      jm_bench_add (&_bench, name, t_real[k], ITERATIONS, BLOCK);
      printf ("  %-26s %7.2f ns/sample  %8.1f MSa/s\n", name,
              min_sec (t_real[k], ITERATIONS) / BLOCK * 1e9,
              (double)BLOCK / min_sec (t_real[k], ITERATIONS) / 1e6);

      size_t cap = dp_psd_power_onesided_max_out (p);
      float *out = malloc (cap * sizeof *out);
      if (!out)
        return 1;
      static double t_rd[ITERATIONS];
      const int calls = RD_SPAN / nffts[k] < 4 ? 4 : (int)(RD_SPAN / nffts[k]);
      for (int r = 0; r < ITERATIONS; r++)
        {
          t0 = jm_bench_now_ns ();
          for (int c = 0; c < calls; c++)
            sink += (double)dp_psd_power_onesided (p, cap, out, cap);
          t1      = jm_bench_now_ns ();
          t_rd[r] = jm_bench_elapsed_sec (t0, t1);
        }
      (void)snprintf (name, sizeof name, "power_onesided[nfft=%zu]", nffts[k]);
      jm_bench_add (&_bench, name, t_rd, ITERATIONS, calls);
      printf ("  %-26s %7.2f us/call    (%zu bins)\n\n", name,
              min_sec (t_rd, ITERATIONS) / calls * 1e6, cap);
      free (out);
      dp_psd_destroy (p);
    }

  printf ("  nfft 1024 -> 16384 costs %.2fx per sample -- BELOW 1.0, so a\n"
          "  16x finer resolution is not a throughput loss here, it is a\n"
          "  throughput gain. The n log n transform term is swamped by the\n"
          "  per-FRAME work, and a small nfft buys 16x more frames over the\n"
          "  same block. Resolution is close to free on this path; the cost\n"
          "  that does scale is power_onesided, linear in the bin count.\n"
          "\n  accumulate_real tracks accumulate within a few percent rather\n"
          "  than halving it -- a real input does not buy half an FFT here.\n",
          (min_sec (t_acc[2], ITERATIONS) / min_sec (t_acc[0], ITERATIONS)));

  int n_lin = 0; /* frame_linear rows, counted as configured */
  /* ── the per-frame kernel, and the transform inside it (#1894 A4) ── */
  {
    static const size_t ksz[NKERN]   = { 256, 1024, 4096, 16384, 65536 };
    static const char  *kname[NKIND] = { "fft", "frame_power", "frame_db",
                                         "accumulate_frame", "frame_linear" };
    kern_t              kern[NKERN]  = { 0 };
    float _Complex     *xk           = malloc ((size_t)BLOCK * 8 * sizeof *xk);
    if (!xk)
      return 1;
    for (size_t i = 0; i < (size_t)BLOCK * 8; i++)
      {
        double q = 0.01 * (double)i;
        xk[i]    = (float)cos (q) + (float)sin (q * 1.7) * I;
      }
    for (int k = 0; k < NKERN; k++)
      {
        kern_t *c = &kern[k];
        c->nfft   = ksz[k];
        /* frame_linear at the ends of the range and the Spectrogram's 1024
           only: the three rows the 32-row cap leaves */
        c->lin = c->nfft == 256 || c->nfft == 1024 || c->nfft == 65536;
        n_lin += c->lin;
        /* as many frames as fill one BLOCK, at least 4, so a pass of the
           largest transform is still several frames long */
        c->reps = BLOCK / c->nfft < 4 ? 4 : BLOCK / c->nfft;
        /* Hann, pad 1: the Spectrogram's own construction */
        c->p    = dp_psd_create (c->nfft, 1.0, 0, 0.0f, 1, 1.0, 0, 0, 0.0);
        c->plan = dp_fft_create (c->nfft, -1, 1); /* psd_core.c's call */
        c->spec = malloc (c->nfft * sizeof *c->spec);
        c->out  = malloc (c->nfft * sizeof *c->out);
        if (!c->p || !c->plan || !c->spec || !c->out)
          return 1;
        /* seed the average untimed: every timed accumulate is then a fold,
           never the first frame's copy (the frame calls do not touch it) */
        dp_psd_accumulate (c->p, xk, c->nfft);
      }
    DP_BENCH_SETTLE (kern_run (&kern[1], 2, xk));
    /* the kinds ROTATE per round: their shares are differences, and a
       fixed order would always start the same kind warm */
    for (int r = 0; r < KROUNDS; r++)
      for (int k = 0; k < NKERN; k++)
        for (int q = 0; q < NKIND; q++)
          {
            const int kind = (q + r) % NKIND;
            if (kind == 4 && !kern[k].lin)
              continue;
            t0 = jm_bench_now_ns ();
            kern_run (&kern[k], kind, xk);
            t1                 = jm_bench_now_ns ();
            kern[k].t[kind][r] = jm_bench_elapsed_sec (t0, t1);
          }
    printf ("\n");
    for (int k = 0; k < NKERN; k++)
      {
        for (int kind = 0; kind < NKIND; kind++)
          {
            if (kind == 4 && !kern[k].lin)
              continue;
            char name[64];
            (void)snprintf (name, sizeof name, "%s[nfft=%zu]", kname[kind],
                            kern[k].nfft);
            dp_bench_record (&_bench, name, kern[k].t[kind], KROUNDS,
                             kern[k].reps, "frame");
          }
        sink += (double)kern[k].out[0];
        free (kern[k].spec);
        free (kern[k].out);
        dp_fft_destroy (kern[k].plan);
        dp_psd_destroy (kern[k].p);
      }
    free (xk);
  }

  /* Every row RECORDS, or nothing is written: jm_bench.h drops entries past
     JM_BENCH_MAX_ENTRIES without a word (just-buildit/just-makeit#2188), so
     the count is checked against the one the tables above DERIVE. A short
     set then reaches the publish gate as a missing component (#2062). */
  const int want
      = (int)(sizeof nffts / sizeof *nffts) * 3 + NKERN * (NKIND - 1) + n_lin;
  if (_bench.count != want)
    {
      (void)fprintf (stderr,
                     "bench_psd: recorded %d rows of %d; writing none\n",
                     _bench.count, want);
      return 1;
    }
  (void)sink;
  free (x);
  free (xr);
  jm_bench_write_json (&_bench, "psd");
  return 0;
}
