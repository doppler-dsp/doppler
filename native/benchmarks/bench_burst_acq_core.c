/* bench_burst_acq_core.c — burst detection, per sample of search.
 *
 * A jm scaffold that recorded nothing until now (doppler#891).
 *
 * `burst_acq_push` is a DETECTOR, not a tracker: it runs continuously over
 * the input looking for a preamble that may never arrive, so unlike the
 * loops it pays its cost whether or not there is a signal. That makes its
 * per-sample number the floor of any always-on receiver, and the one that
 * decides how many channels a box can watch at once.
 *
 * Swept over spreading factor and samples-per-chip, the two knobs that set
 * the size of the search: the correlator is SF long and the hypothesis grid
 * is spc wide, so the per-sample cost should track their product if the
 * search is done directly, and much less if it is done by transform.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "burst_acq/burst_acq_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 32768
#define ITERATIONS 50
#define WARMUP_S 0.25
#define MAX_RESULTS 16
#define CHIP_RATE 1.0e6

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
  acq_result_t  *res = malloc (MAX_RESULTS * sizeof *res);
  if (!x || !res)
    return 1;
  uint32_t lfsr = 0x77C3u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      double s = (lfsr & 1u) ? -1.0 : 1.0;
      x[i]     = (float)s + (float)(0.02 * s) * I;
    }

  printf ("=== burst_acq benchmark ===\n");
  printf ("block = %d samples, %d rounds\n\n", BENCH_N, ITERATIONS);

  const size_t   sfs[2]  = { 15, 63 };
  const size_t   spcs[2] = { 2, 4 };
  static double  t_ps[2][2][ITERATIONS];
  static uint8_t code[64];

  for (int k = 0; k < 2; k++)
    for (int p = 0; p < 2; p++)
      {
        const size_t sf = sfs[k], spc = spcs[p];
        for (size_t i = 0; i < sf; i++)
          code[i] = (uint8_t)(((i * 2246822519u) >> 31) & 1u);

        burst_acq_state_t *a = burst_acq_create (code, sf, 8, spc, CHIP_RATE,
                                                 65.0, 0.0, 1e-2, 0.9, 0);
        if (!a)
          {
            (void)fprintf (stderr,
                           "bench_burst_acq: create(sf=%zu, spc=%zu) NULL\n",
                           sf, spc);
            return 1;
          }

        struct timespec w0, w1;
        clock_gettime (CLOCK_MONOTONIC, &w0);
        do
          {
            burst_acq_reset (a);
            sink += burst_acq_push (a, x, BENCH_N, res, MAX_RESULTS);
            clock_gettime (CLOCK_MONOTONIC, &w1);
          }
        while (elapsed_sec (&w0, &w1) < WARMUP_S);

        for (int r = 0; r < ITERATIONS; r++)
          {
            burst_acq_reset (a);
            clock_gettime (CLOCK_MONOTONIC, &t0);
            sink += burst_acq_push (a, x, BENCH_N, res, MAX_RESULTS);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            t_ps[k][p][r] = elapsed_sec (&t0, &t1);
          }
        char name[64];
        (void)snprintf (name, sizeof name, "push[sf=%zu,spc=%zu]", sf, spc);
        jm_bench_add (&_bench, name, t_ps[k][p], ITERATIONS, BENCH_N);
        double sec = min_sec (t_ps[k][p], ITERATIONS);
        printf ("  %-22s %7.2f ns/sample  %8.1f MSa/s\n", name,
                sec / (double)BENCH_N * 1e9, (double)BENCH_N / sec / 1e6);
        burst_acq_destroy (a);
      }

  printf ("\n  sf 15 -> 63 costs %.2fx; spc 2 -> 4 costs %.2fx. A detector\n"
          "  pays this whether or not a burst arrives, so it is the floor of\n"
          "  an always-on receiver and what decides how many channels one\n"
          "  box can watch.\n",
          min_sec (t_ps[1][0], ITERATIONS) / min_sec (t_ps[0][0], ITERATIONS),
          min_sec (t_ps[0][1], ITERATIONS) / min_sec (t_ps[0][0], ITERATIONS));

  (void)sink;
  free (x);
  free (res);
  jm_bench_write_json (&_bench, "burst_acq");
  return 0;
}
