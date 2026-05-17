/**
 * @file bench_hbdecim_core.c
 * @brief Throughput benchmark for hbdecim_execute.
 *
 * Sweeps four block sizes; iters = TOTAL / block so total input samples
 * is constant across rows.
 */

#include "hbdecim/hbdecim_core.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL 40960000

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
  size_t maxblock = BLOCKS[N_BLOCKS - 1];
  float _Complex *in = calloc (maxblock, sizeof (float _Complex));
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

  printf ("=== hbdecim_core benchmark ===\n");
  printf ("total = %d input samples per row  (19-tap FIR branch)\n\n", TOTAL);
  printf ("  %-10s  %-6s  %14s\n", "block", "iters", "MSa/s (in)");
  printf ("  %-10s  %-6s  %14s\n", "----------", "------", "--------------");

  for (size_t bi = 0; bi < N_BLOCKS; bi++)
    {
      size_t block = BLOCKS[bi];
      int iters = TOTAL / (int)block;

      hbdecim_state_t *r = hbdecim_create (HB19_N, HB19_FIR);
      if (!r)
        continue;

      struct timespec t0, t1;
      volatile size_t sink = 0;

      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int i = 0; i < iters; i++)
        sink += hbdecim_execute (r, in, block, out, block / 2 + 16);
      clock_gettime (CLOCK_MONOTONIC, &t1);

      double msa = (double)iters * (double)block / elapsed_sec (t0, t1) / 1e6;
      printf ("  %-10zu  %-6d  %11.1f MSa  (sink=%zu)\n", block, iters, msa,
              (size_t)sink);

      hbdecim_destroy (r);
    }

  free (in);
  free (out);
  return 0;
}
