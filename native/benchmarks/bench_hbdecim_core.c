/**
 * @file bench_hbdecim_core.c
 * @brief Throughput benchmark for hbdecim_execute.
 *
 * Sweeps four block sizes; each configuration runs ITERATIONS independent
 * timed rounds so jm_bench can report full statistics (mean, stddev, etc.).
 * ops = TOTAL_PER_ROUND / mean = input samples per second.
 */

#include "hbdecim/hbdecim_core.h"
#include "jm_bench.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL_PER_ROUND 2048000
#define ITERATIONS 20

static const size_t BLOCKS[] = { 1024, 20480, 409600, 819200 };
static const size_t N_BLOCKS = sizeof BLOCKS / sizeof BLOCKS[0];

/* 19-tap FIR branch — kaiser_prototype(phases=2, atten=60, pb=0.4, sb=0.6) */
#define HB19_N 19
static const float HB19_FIR[HB19_N] = {
  +1.5790532343e-03f, -4.6757734381e-03f, +1.0443178937e-02f,
  -2.0174624398e-02f, +3.5798925906e-02f, -6.0866370797e-02f,
  +1.0411340743e-01f, -1.9753780961e-01f, +6.3160091639e-01f,
  +6.3160091639e-01f, -1.9753780961e-01f, +1.0411340743e-01f,
  -6.0866370797e-02f, +3.5798925906e-02f, -2.0174624398e-02f,
  +1.0443178937e-02f, -4.6757734381e-03f, +1.5790532343e-03f,
  +0.0000000000e+00f,
};

static double
elapsed_sec (struct timespec a, struct timespec b)
{
  return (double)(b.tv_sec - a.tv_sec)
         + (double)(b.tv_nsec - a.tv_nsec) * 1e-9;
}

int
main (void)
{
  size_t          maxblock = BLOCKS[N_BLOCKS - 1];
  float _Complex *in       = calloc (maxblock, sizeof (float _Complex));
  float _Complex *out = calloc (maxblock / 2 + 16, sizeof (float _Complex));
  if (!in || !out)
    {
      free (in);
      free (out);
      return 1;
    }

  for (size_t k = 0; k < maxblock; k++)
    in[k]
        = CMPLXF ((float)cos (0.1 * (double)k), (float)sin (0.1 * (double)k));

  jm_bench_t bench = { 0 };

  printf ("=== hbdecim_core benchmark ===\n");
  printf ("19-tap FIR branch\n");
  printf ("TOTAL_PER_ROUND = %d samples,  %d iterations\n\n", TOTAL_PER_ROUND,
          ITERATIONS);

  for (size_t bi = 0; bi < N_BLOCKS; bi++)
    {
      size_t block = BLOCKS[bi];
      int    iters = TOTAL_PER_ROUND / (int)block;
      if (iters < 1)
        iters = 1;

      hbdecim_state_t *r = hbdecim_create (HB19_N, HB19_FIR);
      if (!r)
        continue;

      double          times[ITERATIONS];
      struct timespec t0, t1;
      volatile size_t sink = 0;

      /* warmup */
      for (int i = 0; i < 4; i++)
        sink += hbdecim_execute (r, in, block, out, block / 2 + 16);

      for (int rep = 0; rep < ITERATIONS; rep++)
        {
          clock_gettime (CLOCK_MONOTONIC, &t0);
          for (int i = 0; i < iters; i++)
            sink += hbdecim_execute (r, in, block, out, block / 2 + 16);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          times[rep] = elapsed_sec (t0, t1);
        }

      char name[64];
      snprintf (name, sizeof (name), "execute[block=%zu]", block);
      jm_bench_add (&bench, name, times, ITERATIONS, TOTAL_PER_ROUND);

      double mean = 0.0;
      for (int i = 0; i < ITERATIONS; i++)
        mean += times[i];
      mean /= ITERATIONS;
      printf ("  block=%-8zu  %8.1f MSa/s  (sink=%zu)\n", block,
              (double)TOTAL_PER_ROUND / mean / 1e6, (size_t)sink);

      hbdecim_destroy (r);
    }

  jm_bench_write_json (&bench, "hbdecim_core");
  free (in);
  free (out);
  return 0;
}
