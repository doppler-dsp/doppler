/**
 * @file bench_acc_cf64_core.c
 * @brief Throughput benchmark for AccCf64.
 *
 * Sweeps four block sizes; iters = TOTAL / block so total input samples
 * is constant across rows.
 *
 * Build and run:
 *   cmake -B build -S . && cmake --build build --target bench_acc_cf64_core
 *   ./build/native/src/acc_cf64/bench_acc_cf64_core
 */

#include "acc_cf64/acc_cf64_core.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL 40960000

static const size_t BLOCKS[] = { 1024, 20480, 409600, 819200 };
static const size_t N_BLOCKS = sizeof BLOCKS / sizeof BLOCKS[0];

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
  double complex *in = malloc (maxblock * sizeof (double complex));
  if (!in)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }
  for (size_t i = 0; i < maxblock; i++)
    in[i] = (double)i + 0.0 * I;

  printf ("=== bench_acc_cf64_core ===\n");
  printf ("total = %d samples/row\n\n", TOTAL);
  printf ("  %-10s  %-6s  %14s\n", "block", "iters", "steps()");
  printf ("  %-10s  %-6s  %14s\n", "----------", "------", "--------------");

  for (size_t bi = 0; bi < N_BLOCKS; bi++)
    {
      size_t block = BLOCKS[bi];
      int iters = TOTAL / (int)block;

      acc_cf64_state_t *obj = acc_cf64_create (0.0 + 0.0 * I);
      /* warmup */
      for (int w = 0; w < 4; w++)
        acc_cf64_steps (obj, in, block);

      struct timespec t0, t1;
      clock_gettime (CLOCK_MONOTONIC, &t0);
      for (int r = 0; r < iters; r++)
        acc_cf64_steps (obj, in, block);
      clock_gettime (CLOCK_MONOTONIC, &t1);

      double msa = (double)iters * (double)block / elapsed_sec (t0, t1) / 1e6;
      printf ("  %-10zu  %-6d  %11.1f MSa\n", block, iters, msa);
      acc_cf64_destroy (obj);
    }

  free (in);
  return 0;
}
