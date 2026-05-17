/**
 * @file bench_fir_core.c
 * @brief Throughput benchmark for the FIR filter core.
 *
 * Sweeps four block sizes; iters = TOTAL / block so total input samples
 * is constant across rows.  Filter: 19-tap sinc-Hann LP at fc=0.25.
 *
 * Two variants per block size:
 *   CF32/real  — fir_create_real(), CF32 input
 *   CF32/cplx  — fir_create(),      CF32 input
 *
 * Build and run:
 *   cmake -B build -S . && cmake --build build --target bench_fir_core
 *   ./build/native/src/fir/bench_fir_core
 */

#include "fir/fir_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NUM_TAPS 19
#define TOTAL 40960000

static const size_t BLOCKS[] = { 1024, 20480, 409600, 819200 };
static const size_t N_BLOCKS = sizeof BLOCKS / sizeof BLOCKS[0];

static double
elapsed_sec (struct timespec a, struct timespec b)
{
  return (double)(b.tv_sec - a.tv_sec)
         + (double)(b.tv_nsec - a.tv_nsec) * 1e-9;
}

/* prevent the compiler from discarding computed output */
static volatile float _sink_r, _sink_i;
static void
consume (const float complex *out, size_t n)
{
  _sink_r = crealf (out[n - 1]);
  _sink_i = cimagf (out[n - 1]);
}

int
main (void)
{
  /* 19-tap sinc-Hann LP at fc=0.25, normalised to unit DC gain */
  float rtaps[NUM_TAPS];
  float complex ctaps[NUM_TAPS];
  {
    float fc = 0.25f;
    float sum = 0.0f;
    for (int k = 0; k < NUM_TAPS; k++)
      {
        int n = k - NUM_TAPS / 2;
        float h = (n == 0) ? 2.0f * fc
                           : sinf (2.0f * (float)M_PI * fc * (float)n)
                                 / ((float)M_PI * (float)n);
        h *= 0.5f
             * (1.0f
                - cosf (2.0f * (float)M_PI * (float)k
                        / (float)(NUM_TAPS - 1)));
        rtaps[k] = h;
        sum += h;
      }
    for (int k = 0; k < NUM_TAPS; k++)
      {
        rtaps[k] /= sum;
        ctaps[k] = rtaps[k] + 0.0f * I;
      }
  }

  size_t maxblock = BLOCKS[N_BLOCKS - 1];
  float complex *in = malloc (maxblock * sizeof (float complex));
  float complex *out = malloc (maxblock * sizeof (float complex));
  if (!in || !out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  for (size_t i = 0; i < maxblock; i++)
    {
      float angle = 2.0f * (float)M_PI * 0.1f * (float)i;
      in[i] = cosf (angle) + sinf (angle) * I;
    }

  printf ("=== bench_fir_core ===\n");
  printf ("taps = %d,  total = %d samples/row\n\n", NUM_TAPS, TOTAL);
  printf ("  %-10s  %-6s  %14s  %14s\n", "block", "iters", "CF32/real",
          "CF32/cplx");
  printf ("  %-10s  %-6s  %14s  %14s\n", "----------", "------",
          "--------------", "--------------");

  for (size_t bi = 0; bi < N_BLOCKS; bi++)
    {
      size_t block = BLOCKS[bi];
      int iters = TOTAL / (int)block;
      struct timespec t0, t1;
      double msa[2];

      /* real taps */
      {
        fir_state_t *f = fir_create_real (rtaps, NUM_TAPS);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int r = 0; r < iters; r++)
          {
            fir_execute (f, in, block, out);
            consume (out, block);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        msa[0] = (double)iters * (double)block / elapsed_sec (t0, t1) / 1e6;
        fir_destroy (f);
      }

      /* complex taps */
      {
        fir_state_t *f = fir_create (ctaps, NUM_TAPS);
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int r = 0; r < iters; r++)
          {
            fir_execute (f, in, block, out);
            consume (out, block);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        msa[1] = (double)iters * (double)block / elapsed_sec (t0, t1) / 1e6;
        fir_destroy (f);
      }

      printf ("  %-10zu  %-6d  %11.1f MSa  %11.1f MSa\n", block, iters, msa[0],
              msa[1]);
    }

  free (in);
  free (out);
  return 0;
}
