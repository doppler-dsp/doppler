/**
 * @file bench_fir.c
 * @brief Throughput benchmark for dp_fir_execute_* hot loops.
 *
 * Reports throughput in MSamples/s for each input type.
 * Run with: ./bench_fir_c
 */

#include <dp/fir.h>
#include <dp/stream.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_TAPS 19
#define BLOCK_SIZE 409600
#define ITERATIONS 100

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static void
bench_cf32 (dp_fir_t *fir, dp_cf32_t *in, dp_cf32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int iter = 0; iter < ITERATIONS; iter++)
    dp_fir_execute_cf32 (fir, in, out, BLOCK_SIZE);

  clock_gettime (CLOCK_MONOTONIC, &t1);

  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK_SIZE / sec / 1e6;
  printf ("  CF32  %8.1f MSa/s  (%.3f s total)\n", msa, sec);
}

static void
bench_ci16 (dp_fir_t *fir, dp_ci16_t *in, dp_cf32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int iter = 0; iter < ITERATIONS; iter++)
    dp_fir_execute_ci16 (fir, in, out, BLOCK_SIZE);

  clock_gettime (CLOCK_MONOTONIC, &t1);

  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK_SIZE / sec / 1e6;
  printf ("  CI16  %8.1f MSa/s  (%.3f s total)\n", msa, sec);
}

static void
bench_ci8 (dp_fir_t *fir, dp_ci8_t *in, dp_cf32_t *out)
{
  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);

  for (int iter = 0; iter < ITERATIONS; iter++)
    dp_fir_execute_ci8 (fir, in, out, BLOCK_SIZE);

  clock_gettime (CLOCK_MONOTONIC, &t1);

  double sec = elapsed_sec (&t0, &t1);
  double msa = (double)ITERATIONS * BLOCK_SIZE / sec / 1e6;
  printf ("  CI8   %8.1f MSa/s  (%.3f s total)\n", msa, sec);
}

int
main (void)
{
  printf ("=== doppler FIR benchmark ===\n");
  printf ("  taps=%d  block=%d  iters=%d\n\n", NUM_TAPS, BLOCK_SIZE,
          ITERATIONS);

  /* Build a simple real-valued LP filter */
  dp_cf32_t taps[NUM_TAPS];
  int half = NUM_TAPS / 2;
  for (int k = 0; k < NUM_TAPS; k++)
    {
      int n = k - half;
      double sinc = (n == 0) ? 1.0 : sin (M_PI * 0.2 * n) / (M_PI * 0.2 * n);
      double win = 0.5 * (1.0 - cos (2.0 * M_PI * k / (NUM_TAPS - 1)));
      taps[k].i = (float)(sinc * win);
      taps[k].q = 0.0f;
    }

  dp_fir_t *fir = dp_fir_create (taps, NUM_TAPS);
  if (!fir)
    {
      fprintf (stderr, "dp_fir_create failed\n");
      return 1;
    }

  dp_cf32_t *cf32_in = (dp_cf32_t *)malloc (BLOCK_SIZE * sizeof (dp_cf32_t));
  dp_ci16_t *ci16_in = (dp_ci16_t *)malloc (BLOCK_SIZE * sizeof (dp_ci16_t));
  dp_ci8_t *ci8_in = (dp_ci8_t *)malloc (BLOCK_SIZE * sizeof (dp_ci8_t));
  dp_cf32_t *out = (dp_cf32_t *)malloc (BLOCK_SIZE * sizeof (dp_cf32_t));

  for (int i = 0; i < BLOCK_SIZE; i++)
    {
      double phase = 2.0 * M_PI * 0.1 * i;
      cf32_in[i].i = (float)cos (phase);
      cf32_in[i].q = (float)sin (phase);
      ci16_in[i].i = (int16_t)(30000.0 * cos (phase));
      ci16_in[i].q = (int16_t)(30000.0 * sin (phase));
      ci8_in[i].i = (int8_t)(100.0 * cos (phase));
      ci8_in[i].q = (int8_t)(100.0 * sin (phase));
    }

  bench_cf32 (fir, cf32_in, out);
  dp_fir_reset (fir);
  bench_ci16 (fir, ci16_in, out);
  dp_fir_reset (fir);
  bench_ci8 (fir, ci8_in, out);

  free (cf32_in);
  free (ci16_in);
  free (ci8_in);
  free (out);
  dp_fir_destroy (fir);
  return 0;
}
