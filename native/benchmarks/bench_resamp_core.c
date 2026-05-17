/**
 * @file bench_resamp_core.c
 * @brief Throughput benchmark for the polyphase resampler.
 *
 * Sweeps four block sizes at several rates; iters = TOTAL / block so
 * total input samples is constant across block-size rows.
 */

#include "resamp/resamp_core.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL 40960000

static const size_t BLOCKS[] = { 1024, 20480, 409600, 819200 };
static const size_t N_BLOCKS = sizeof BLOCKS / sizeof BLOCKS[0];

static const double RATES[] = { 1.0001, 0.9999, 0.5, 2.0 };
static const size_t N_RATES = sizeof RATES / sizeof RATES[0];

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
  /* rate=2.0 can produce up to 2× input samples */
  float _Complex *out = calloc (maxblock * 2 + 16, sizeof (float _Complex));
  if (!in || !out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (size_t i = 0; i < maxblock; i++)
    in[i] = CMPLXF (1.0f, 0.0f);

  resamp_state_t *probe = resamp_create (1.0001);
  size_t num_phases = resamp_get_num_phases (probe);
  size_t num_taps = resamp_get_num_taps (probe);
  resamp_destroy (probe);

  printf ("=== resamp_core benchmark ===\n");
  printf ("filter = %zu-phase × %zu-tap Kaiser (60 dB, 0.4/0.6)\n", num_phases,
          num_taps);
  printf ("total = %d input samples per row\n\n", TOTAL);

  for (size_t ri = 0; ri < N_RATES; ri++)
    {
      double rate = RATES[ri];
      printf ("rate = %.4f\n", rate);
      printf ("  %-10s  %-6s  %14s\n", "block", "iters", "MSa/s (in)");
      printf ("  %-10s  %-6s  %14s\n", "----------", "------",
              "--------------");

      for (size_t bi = 0; bi < N_BLOCKS; bi++)
        {
          size_t block = BLOCKS[bi];
          int iters = TOTAL / (int)block;
          size_t max_out
              = (rate >= 1.0) ? (size_t)(block * rate + 16) : block + 16;

          resamp_state_t *obj = resamp_create (rate);
          if (!obj)
            continue;

          struct timespec t0, t1;
          clock_gettime (CLOCK_MONOTONIC, &t0);
          for (int r = 0; r < iters; r++)
            resamp_execute (obj, in, block, out, max_out);
          clock_gettime (CLOCK_MONOTONIC, &t1);

          double msa
              = (double)iters * (double)block / elapsed_sec (t0, t1) / 1e6;
          printf ("  %-10zu  %-6d  %11.1f MSa\n", block, iters, msa);
          resamp_destroy (obj);
        }
      printf ("\n");
    }

  free (in);
  free (out);
  return 0;
}
