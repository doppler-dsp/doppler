/**
 * @file bench_resamp_core.c
 * @brief Throughput benchmark for the polyphase resampler.
 *
 * Sweeps four block sizes at four rates; each configuration runs ITERATIONS
 * independent timed rounds so jm_bench can report full statistics.
 * ops = TOTAL_PER_ROUND / mean = input samples per second.
 */

#include "jm_bench.h"
#include "resamp/resamp_core.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL_PER_ROUND 2048000
#define ITERATIONS 20

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
  size_t          maxblock = BLOCKS[N_BLOCKS - 1];
  float _Complex *in       = calloc (maxblock, sizeof (float _Complex));
  float _Complex *out = calloc (maxblock * 2 + 16, sizeof (float _Complex));
  if (!in || !out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (size_t i = 0; i < maxblock; i++)
    in[i] = CMPLXF (1.0f, 0.0f);

  resamp_state_t *probe   = resamp_create (1.0001);
  size_t          nphases = resamp_get_num_phases (probe);
  size_t          ntaps   = resamp_get_num_taps (probe);
  resamp_destroy (probe);

  jm_bench_t bench = { 0 };

  printf ("=== resamp_core benchmark ===\n");
  printf ("filter = %zu-phase × %zu-tap Kaiser (60 dB, 0.4/0.6)\n", nphases,
          ntaps);
  printf ("TOTAL_PER_ROUND = %d samples,  %d iterations\n\n", TOTAL_PER_ROUND,
          ITERATIONS);

  for (size_t ri = 0; ri < N_RATES; ri++)
    {
      double rate = RATES[ri];
      printf ("rate = %.4f\n", rate);

      for (size_t bi = 0; bi < N_BLOCKS; bi++)
        {
          size_t block = BLOCKS[bi];
          int    iters = TOTAL_PER_ROUND / (int)block;
          if (iters < 1)
            iters = 1;
          size_t max_out
              = (rate >= 1.0) ? (size_t)(block * rate + 16) : block + 16;

          resamp_state_t *obj = resamp_create (rate);
          if (!obj)
            continue;

          double          times[ITERATIONS];
          struct timespec t0, t1;

          /* warmup */
          for (int i = 0; i < 4; i++)
            resamp_execute (obj, in, block, out, max_out);

          for (int rep = 0; rep < ITERATIONS; rep++)
            {
              clock_gettime (CLOCK_MONOTONIC, &t0);
              for (int i = 0; i < iters; i++)
                resamp_execute (obj, in, block, out, max_out);
              clock_gettime (CLOCK_MONOTONIC, &t1);
              times[rep] = elapsed_sec (t0, t1);
            }

          char name[64];
          snprintf (name, sizeof (name), "execute[rate=%.4f,block=%zu]", rate,
                    block);
          jm_bench_add (&bench, name, times, ITERATIONS, TOTAL_PER_ROUND);

          double mean = 0.0;
          for (int i = 0; i < ITERATIONS; i++)
            mean += times[i];
          mean /= ITERATIONS;
          printf ("  block=%-8zu  %8.1f MSa/s\n", block,
                  (double)TOTAL_PER_ROUND / mean / 1e6);

          resamp_destroy (obj);
        }
      printf ("\n");
    }

  jm_bench_write_json (&bench, "resamp_core");
  free (in);
  free (out);
  return 0;
}
