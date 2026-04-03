/**
 * @file bench_ddc.c
 * @brief Throughput benchmark for dp_ddc_t.
 *
 * Reports input MSamples/s for the complete DDC signal chain
 * (NCO mix + DPMFS decimation) at several decimation rates.
 * Uses the built-in M=3 N=19 Kaiser-DPMFS default coefficients.
 *
 * rate=1.0 bypasses the resampler — isolates NCO mix cost.
 *
 * Run with: ./bench_ddc_c
 */

#include <dp/ddc.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK_SIZE 65536
#define ITERATIONS 200

/* ------------------------------------------------------------------ */
/* Timer helpers                                                       */
/* ------------------------------------------------------------------ */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* ------------------------------------------------------------------ */
/* Benchmark runner                                                    */
/* ------------------------------------------------------------------ */

static double
bench_ddc (float norm_freq, double rate, const dp_cf32_t *input,
           dp_cf32_t *output)
{
  dp_ddc_t *ddc = dp_ddc_create (norm_freq, BLOCK_SIZE, rate);
  if (!ddc)
    {
      fprintf (stderr, "dp_ddc_create failed\n");
      return 0.0;
    }

  size_t max_out = dp_ddc_max_out (ddc);

  /* Warm-up + steady-state (exercise resampler history) */
  dp_ddc_execute (ddc, input, BLOCK_SIZE, output, max_out);
  dp_ddc_reset (ddc);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_ddc_execute (ddc, input, BLOCK_SIZE, output, max_out);
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_ddc_destroy (ddc);

  double sec = elapsed_sec (&t0, &t1);
  return (double)ITERATIONS * BLOCK_SIZE / sec / 1e6;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main (void)
{
  printf ("=== doppler DDC benchmark (NCO mix + DPMFS decimation) ===\n");
  printf ("  block=%d  iters=%d  default M=3 N=19 Kaiser-DPMFS\n", BLOCK_SIZE,
          ITERATIONS);
  printf ("  (%.0f M input samples total per row)\n\n",
          (double)BLOCK_SIZE * ITERATIONS / 1e6);

  dp_cf32_t *input = malloc (BLOCK_SIZE * sizeof *input);
  /* Output large enough for the slowest decimation (rate=1.0 → same as
   * input size; for interpolation headroom add a generous margin).    */
  size_t max_alloc = (size_t)(BLOCK_SIZE * 2) + 16;
  dp_cf32_t *output = malloc (max_alloc * sizeof *output);
  if (!input || !output)
    {
      fprintf (stderr, "allocation failed\n");
      return 1;
    }

  /* Synthetic two-tone IQ signal */
  for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
      double t0 = 2.0 * M_PI * 0.05 * (double)i;
      double t1 = 2.0 * M_PI * 0.23 * (double)i;
      input[i].i = (float)(cos (t0) + cos (t1));
      input[i].q = (float)(sin (t0) + sin (t1));
    }

  /* Tune NCO to mix the 0.05 tone to DC for all cases. */
  const float norm_freq = -0.05f;

  struct
  {
    double rate;
    const char *label;
  } cases[] = {
    { 1.0, "bypass (NCO only)" }, { 0.5, "2x decim" },  { 0.25, "4x decim" },
    { 0.125, "8x decim" },        { 0.1, "10x decim" }, { 0.01, "100x decim" },
  };
  size_t ncases = sizeof cases / sizeof cases[0];

  printf ("  %-8s  %-18s  %12s\n", "rate", "config", "MSa/s (in)");
  printf ("  %-8s  %-18s  %12s\n", "--------", "------------------",
          "------------");

  for (size_t i = 0; i < ncases; i++)
    {
      double msa = bench_ddc (norm_freq, cases[i].rate, input, output);
      printf ("  %-8.4f  %-18s  %12.1f\n", cases[i].rate, cases[i].label, msa);
    }

  printf ("\n");
  free (input);
  free (output);
  return 0;
}
