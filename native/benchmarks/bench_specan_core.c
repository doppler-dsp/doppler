/* bench_specan_core.c — the spectrum analyzer's per-block path.
 *
 * A jm scaffold that recorded nothing until now (doppler#891). `specan` is
 * the front end of the analyzer application: it decimates to the requested
 * span, transforms at the requested RBW, averages, and hands back a trace.
 * It runs at the sample rate for as long as a display is open.
 *
 * The sweep is over RBW at a fixed span, because that is the control an
 * operator turns and it moves two things at once: a finer RBW needs a
 * longer transform (more work per frame) but produces fewer frames per
 * block. Whether the two cancel is the measurement.
 *
 * `retune` is measured separately: it is what a sweeping or scrolling
 * display calls between blocks, and if it rebuilds the plan it is not free.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "specan/specan_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BLOCK 65536
#define ITERATIONS 50
#define FS 10.0e6
#define SPAN 2.0e6

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
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
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile size_t sink   = 0;

  float complex *x = malloc (BLOCK * sizeof *x);
  if (!x)
    return 1;
  for (int i = 0; i < BLOCK; i++)
    {
      double p = 2.0 * M_PI * 0.031 * (double)i;
      x[i]     = (float)(0.5 * cos (p)) + (float)(0.5 * sin (p)) * I;
    }

  printf ("=== specan benchmark ===\n");
  printf ("fs = %.1f MHz, span = %.1f MHz, block = %d samples, %d rounds\n\n",
          FS / 1e6, SPAN / 1e6, BLOCK, ITERATIONS);

  const double  rbws[3] = { 100.0e3, 10.0e3, 1.0e3 };
  static double t_ex[3][ITERATIONS];

  for (int k = 0; k < 3; k++)
    {
      /* window 1, navg 1, no offset, full scale 1.0, float input (bits 0) */
      specan_state_t *s
          = specan_create (FS, SPAN, rbws[k], 0.0, 0.0, 0.0, 1.0, 0, 1, 1);
      if (!s)
        {
          (void)fprintf (stderr, "bench_specan: create(rbw=%.0f) NULL\n",
                         rbws[k]);
          return 1;
        }
      size_t cap = specan_execute_max_out (s);
      float *out = malloc ((cap ? cap : 1) * sizeof *out);
      if (!out)
        return 1;

      size_t got = specan_execute (s, x, BLOCK, out, cap);
      if (got == 0)
        {
          (void)fprintf (stderr,
                         "bench_specan: rbw=%.0f emitted 0 bins from %d "
                         "samples — the timings below would measure a "
                         "no-op\n",
                         rbws[k], BLOCK);
          return 1;
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += specan_execute (s, x, BLOCK, out, cap);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_ex[k][r] = elapsed_sec (&t0, &t1);
        }
      char name[64];
      (void)snprintf (name, sizeof name, "execute[rbw=%.0fk]", rbws[k] / 1e3);
      jm_bench_add (&_bench, name, t_ex[k], ITERATIONS, BLOCK);
      printf ("  %-24s %7.2f ns/sample  %8.1f MSa/s  (%zu bins)\n", name,
              min_sec (t_ex[k], ITERATIONS) / BLOCK * 1e9,
              (double)BLOCK / min_sec (t_ex[k], ITERATIONS) / 1e6, got);

      if (k == 1)
        {
          static double t_rt[ITERATIONS];
          for (int r = 0; r < ITERATIONS; r++)
            {
              clock_gettime (CLOCK_MONOTONIC, &t0);
              specan_retune (s, (r & 1) ? 1.0e5 : -1.0e5);
              clock_gettime (CLOCK_MONOTONIC, &t1);
              t_rt[r] = elapsed_sec (&t0, &t1);
            }
          jm_bench_add (&_bench, "retune", t_rt, ITERATIONS, 1);
          printf ("  %-24s %7.3f us/call   (between blocks, not per "
                  "sample)\n",
                  "retune", min_sec (t_rt, ITERATIONS) * 1e6);
        }

      free (out);
      specan_destroy (s);
    }

  printf ("\n  100x finer RBW costs %.2fx per sample. A finer RBW is a\n"
          "  longer transform per frame AND fewer frames per block, so the\n"
          "  two cancel almost exactly here -- which is why this is measured\n"
          "  rather than reasoning about.\n",
          min_sec (t_ex[2], ITERATIONS) / min_sec (t_ex[0], ITERATIONS));

  (void)sink;
  free (x);
  jm_bench_write_json (&_bench, "specan");
  return 0;
}
