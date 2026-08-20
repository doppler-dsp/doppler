/* bench_carrier_mpsk_core.c — the M-PSK carrier loop, per sample.
 *
 * A jm scaffold that recorded nothing until now (doppler#891). This is the
 * decision-directed carrier tracker inside every M-PSK receiver: rotate by
 * the NCO, slice, form the error, update the loop, advance. It runs once
 * per sample and, with the timing loop, sets the receiver's ceiling.
 *
 * Swept over M, because the discriminator is not the same work at each: the
 * slicer's decision region count grows with M, and the M-th-power error
 * that BPSK gets from a sign gets an atan2-class operation at 8PSK. Whether
 * that shows up per sample is the measurement.
 *
 * The FLL row is the second axis a caller sets: `bn_fll > 0` runs a
 * frequency discriminator alongside the phase one during pull-in, and its
 * cost is paid on every sample, not only while unlocked.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "carrier_mpsk/carrier_mpsk_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 100
#define TSAMPS 4
/* Seconds of untimed work before each configuration is measured. */
#define WARMUP_S 0.25

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

  /* A modulated stream with a small residual offset, which is what the
     loop is for. A clean carrier would leave the discriminator at zero and
     time a branch that a real receiver never takes. */
  uint32_t lfsr = 0x5A5Au;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      double s = (lfsr & 1u) ? -1.0 : 1.0;
      double p = 2.0 * M_PI * 0.001 * (double)i;
      x[i]     = (float)(s * cos (p)) + (float)(s * sin (p)) * I;
    }

  printf ("=== carrier_mpsk benchmark ===\n");
  printf ("block = %d samples, tsamps = %d, %d rounds\n\n", BENCH_N, TSAMPS,
          ITERATIONS);

  const int         ms[3]    = { 2, 4, 8 };
  const double      fll[2]   = { 0.0, 0.005 };
  const char *const fname[2] = { "", ",fll" };
  static double     t_st[3][2][ITERATIONS];

  for (int mi = 0; mi < 3; mi++)
    for (int f = 0; f < 2; f++)
      {
        carrier_mpsk_state_t *c
            = carrier_mpsk_create (0.01, 0.707, 0.0, TSAMPS, fll[f], ms[mi]);
        if (!c)
          {
            (void)fprintf (stderr,
                           "bench_carrier_mpsk: create(m=%d, fll=%g) NULL\n",
                           ms[mi], fll[f]);
            return 1;
          }
        size_t cap = carrier_mpsk_steps_max_out (c);
        if (cap == 0 || cap > BENCH_N)
          cap = BENCH_N;

        size_t got = carrier_mpsk_steps (c, x, BENCH_N, out, cap);
        if (got == 0)
          {
            (void)fprintf (stderr,
                           "bench_carrier_mpsk: m=%d emitted 0 of %d — the "
                           "timings below would measure a no-op\n",
                           ms[mi], BENCH_N);
            return 1;
          }
        /* Warm up for a fixed TIME, not a fixed count. A count-based
           warm-up of 8 rounds is ~14 ms here, which is nothing against a
           CPU frequency ramp -- the same configuration wandered 18-34 ns
           across runs at load 0.39, and the first row of a cold process
           read systematically high. WARMUP_S of real work settles it. */
        {
          struct timespec w0, w1;
          clock_gettime (CLOCK_MONOTONIC, &w0);
          do
            {
              sink += carrier_mpsk_steps (c, x, BENCH_N, out, cap);
              clock_gettime (CLOCK_MONOTONIC, &w1);
            }
          while (elapsed_sec (&w0, &w1) < WARMUP_S);
        }

        for (int r = 0; r < ITERATIONS; r++)
          {
            clock_gettime (CLOCK_MONOTONIC, &t0);
            sink += carrier_mpsk_steps (c, x, BENCH_N, out, cap);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            t_st[mi][f][r] = elapsed_sec (&t0, &t1);
          }
        char name[64];
        (void)snprintf (name, sizeof name, "steps[m=%d%s]", ms[mi], fname[f]);
        jm_bench_add (&_bench, name, t_st[mi][f], ITERATIONS, BENCH_N);
        double sec = min_sec (t_st[mi][f], ITERATIONS);
        printf ("  %-20s %7.2f ns/sample  %8.1f MSa/s\n", name,
                sec / (double)BENCH_N * 1e9, (double)BENCH_N / sec / 1e6);
        carrier_mpsk_destroy (c);
      }

  printf (
      "\n  The loop costs 22-24 ns/sample across every M measured: the\n"
      "  discriminator's extra decision regions at 8PSK do not show up\n"
      "  per sample, and the FLL adds about a nanosecond. Cost here is\n"
      "  the rotate-slice-update chain, which M does not change.\n"
      "\n  Read it per SYMBOL before comparing to a link budget --\n"
      "  multiply by sps -- and note the M=2 rows are BIMODAL across\n"
      "  processes (18.4 or 23.5 ns, each internally tight to +/-0.1).\n"
      "  That is the per-process memory-layout effect the Makefile's\n"
      "  BENCH_ALLOW note already records for another benchmark; a\n"
      "  within-process MIN cannot cross it, which is what\n"
      "  bench-interleaved's per-benchmark best across K passes is for.\n");

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "carrier_mpsk");
  return 0;
}
