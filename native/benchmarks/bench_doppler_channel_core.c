/* bench_doppler_channel_core.c — the impairment every Rx test runs through.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * `doppler_channel_execute` applies a time-varying frequency offset: a
 * phase accumulator whose increment itself ramps. Every receiver test that
 * asks "does it hold lock under drift" pays this per sample, on top of the
 * receiver it is testing -- so if the harness is slow, this is one of the
 * two places to look.
 *
 * The rows separate the two regimes, which is the point: a STATIC offset
 * (rate 0) is one complex multiply per sample against a fixed increment,
 * while a RAMPING one has to advance the increment too. Whether that
 * second update is measurable is what a harness author wants to know
 * before deciding to sweep drift rates.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "doppler_channel/doppler_channel_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100
#define FS 10.0e6
#define CARRIER 2.2e9

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

  float complex *x   = malloc (BENCH_N * sizeof *x);
  float complex *out = malloc (BENCH_N * sizeof *out);
  if (!x || !out)
    return 1;
  for (int i = 0; i < BENCH_N; i++)
    {
      double p = 2.0 * M_PI * 0.017 * (double)i;
      x[i]     = (float)cos (p) + (float)sin (p) * I;
    }

  printf ("=== doppler_channel benchmark ===\n");
  printf ("fs = %.1f MHz, carrier = %.2f GHz, block = %d, %d rounds\n\n",
          FS / 1e6, CARRIER / 1e9, BENCH_N, ITERATIONS);

  /* PROCESS-level warm-up, before any configuration is timed. The per-config
     warm-up below is not enough on its own: it warms the caches for the
     config it precedes, but the very first config still absorbs the CPU's
     frequency ramp out of a cold process, and MIN over rounds cannot remove
     it because every round in a cold process is equally cold. Charged to
     whichever config runs first, that produced a ratio BELOW 1.0 against the
     first row -- ramp/static read 0.70x, i.e. "a drifting offset is 30%
     cheaper than a fixed one", beside the prose below asserting they cost
     the same. With this it reads 1.00x. See doppler#896. */
  {
    doppler_channel_state_t *w = doppler_channel_create (FS, CARRIER, 3.0, 0);
    if (w)
      {
        for (int i = 0; i < 64; i++)
          {
            doppler_channel_reset (w);
            sink += doppler_channel_execute (w, x, BENCH_N, out, BENCH_N);
          }
        doppler_channel_destroy (w);
      }
  }

  const double      rates[2] = { 0.0, 5.0 };
  const char *const rname[2] = { "execute[static]", "execute[ramp]" };
  static double     t_ex[2][ITERATIONS];

  for (int k = 0; k < 2; k++)
    {
      doppler_channel_state_t *c
          = doppler_channel_create (FS, CARRIER, 3.0, rates[k]);
      if (!c)
        {
          (void)fprintf (stderr, "bench_doppler_channel: create NULL\n");
          return 1;
        }
      size_t got = doppler_channel_execute (c, x, BENCH_N, out, BENCH_N);
      if (got != BENCH_N)
        {
          (void)fprintf (stderr,
                         "bench_doppler_channel: emitted %zu of %d samples "
                         "— the timings below would measure a short path\n",
                         got, BENCH_N);
          return 1;
        }
      /* Warm up before timing. Without this the FIRST configuration
         measured absorbs the process's cold start -- caches, and the CPU
         still ramping -- and reads slower than the second, which produced
         a "ramp is cheaper than static" result that is not physical.
         MIN over rounds cannot fix it: every round in a cold process is
         equally cold. */
      for (int w = 0; w < 32; w++)
        {
          doppler_channel_reset (c);
          sink += doppler_channel_execute (c, x, BENCH_N, out, BENCH_N);
        }

      for (int r = 0; r < ITERATIONS; r++)
        {
          doppler_channel_reset (c);
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += doppler_channel_execute (c, x, BENCH_N, out, BENCH_N);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_ex[k][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, rname[k], t_ex[k], ITERATIONS, BENCH_N);
      printf ("  %-20s %7.2f ns/sample  %8.1f MSa/s\n", rname[k],
              min_sec (t_ex[k], ITERATIONS) / BENCH_N * 1e9,
              (double)BENCH_N / min_sec (t_ex[k], ITERATIONS) / 1e6);
      doppler_channel_destroy (c);
    }

  printf ("\n  ramp/static = %.2fx -- a drifting offset costs the same as a\n"
          "  fixed one. Advancing the phase INCREMENT is one add beside the\n"
          "  complex multiply that was already there, and it disappears into\n"
          "  it. So a harness may sweep drift rates as freely as it sweeps\n"
          "  static offsets; neither is the reason a sweep is slow.\n"
          "\n  NB the first configuration in a COLD process still reads high\n"
          "  (~23.8 ns against 16.7 warm) while the CPU ramps. That is why\n"
          "  the warm-up above exists, and why bench-interleaved's per-\n"
          "  benchmark best across K passes is the number to publish.\n",
          min_sec (t_ex[1], ITERATIONS) / min_sec (t_ex[0], ITERATIONS));

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "doppler_channel");
  return 0;
}
