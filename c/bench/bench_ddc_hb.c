/**
 * @file bench_ddc_hb.c
 * @brief Throughput benchmark: HalfbandDecimator → DDC pipeline.
 *
 * Compares two configurations for each total decimation rate:
 *   (A) Plain DDC    — dp_ddc_create(norm_freq, BLOCK_SIZE, rate)
 *   (B) HB + DDC     — dp_hbdecim_cf32 (÷2) → dp_ddc (rate × 2)
 *
 * HB uses a 60 dB Kaiser halfband (n_fir=19, N=19 taps FIR branch),
 * matching the 60 dB stopband attenuation of the built-in DDC filter.
 * DDC uses built-in M=3 N=19 DPMFS default coefficients.
 *
 * Throughput is reported as effective input MSamples/s at the
 * original sample rate (referred to the HB input for config B).
 *
 * Run with: ./bench_ddc_hb_c
 */

#include <dp/ddc.h>
#include <dp/hbdecim.h>

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
/* Timer                                                               */
/* ------------------------------------------------------------------ */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* ------------------------------------------------------------------ */
/* Kaiser halfband FIR branch (identical to bench_hbdecim.c)          */
/* ------------------------------------------------------------------ */

static double
bessel_i0 (double x)
{
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 30; k++)
    {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum += term;
      if (term < 1e-20 * sum)
        break;
    }
  return sum;
}

static double
kaiser_win (int n, int N, double beta)
{
  double mid = (N - 1) / 2.0;
  double arg = 2.0 * (n - mid) / (N - 1);
  return bessel_i0 (beta * sqrt (1.0 - arg * arg)) / bessel_i0 (beta);
}

/* Returns the FIR branch (heap-allocated); sets *N_out to its length. */
static float *
build_hb_fir (int n_fir, double atten, size_t *N_out)
{
  int M = n_fir - 1;
  int htaps = 2 * M + 1;
  double beta = (atten > 50.0)    ? 0.1102 * (atten - 8.7)
                : (atten >= 21.0) ? 0.5842 * pow (atten - 21.0, 0.4)
                                        + 0.07886 * (atten - 21.0)
                                  : 0.0;

  double *h = calloc ((size_t)htaps, sizeof (double));
  for (int n = 0; n < htaps; n++)
    {
      double w = kaiser_win (n, htaps, beta);
      double m = n - M;
      double s = (m == 0.0) ? 0.5 : sin (0.5 * M_PI * m) / (M_PI * m);
      h[n] = w * s * 2.0;
    }

  size_t N = (size_t)M + 1;
  float *bank0 = malloc (N * sizeof (float));
  float *bank1 = malloc (N * sizeof (float));
  for (size_t k = 0; k < N; k++)
    {
      bank0[k] = (2 * k < (size_t)htaps) ? (float)h[2 * k] : 0.0f;
      bank1[k] = (2 * k + 1 < (size_t)htaps) ? (float)h[2 * k + 1] : 0.0f;
    }
  free (h);

  size_t centre = N / 2;
  float *fir_branch;
  if (fabsf (bank0[centre]) > fabsf (bank1[centre]))
    {
      fir_branch = bank1;
      free (bank0);
    }
  else
    {
      fir_branch = bank0;
      free (bank1);
    }

  *N_out = N;
  return fir_branch;
}

/* ------------------------------------------------------------------ */
/* Benchmark runners                                                   */
/* ------------------------------------------------------------------ */

/* Plain DDC: BLOCK_SIZE input at given rate. */
static double
bench_plain_ddc (float norm_freq, double rate, const dp_cf32_t *in,
                 dp_cf32_t *out)
{
  dp_ddc_t *ddc = dp_ddc_create (norm_freq, BLOCK_SIZE, rate);
  if (!ddc)
    return 0.0;

  size_t max_out = dp_ddc_max_out (ddc);
  dp_ddc_execute (ddc, in, BLOCK_SIZE, out, max_out);
  dp_ddc_reset (ddc);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_ddc_execute (ddc, in, BLOCK_SIZE, out, max_out);
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_ddc_destroy (ddc);
  return (double)ITERATIONS * BLOCK_SIZE / elapsed_sec (&t0, &t1) / 1e6;
}

/* HB + DDC pipeline: HB decimates by 2, DDC runs at rate*2 on N/2 input.
 * Reports effective input MSa/s referred to the HB input.            */
static double
bench_hb_ddc (float norm_freq, double rate, size_t hb_N, const float *hb_fir,
              const dp_cf32_t *in, dp_cf32_t *mid, dp_cf32_t *out)
{
  dp_hbdecim_cf32_t *hb = dp_hbdecim_cf32_create (hb_N, hb_fir);
  if (!hb)
    return 0.0;

  size_t hb_max_out = BLOCK_SIZE / 2 + hb_N + 2;
  size_t hb_out;

  /* DDC operates on the HB output: half the samples, double the rate. */
  size_t ddc_num_in = BLOCK_SIZE / 2;
  dp_ddc_t *ddc = dp_ddc_create (norm_freq, ddc_num_in, rate * 2.0);
  if (!ddc)
    {
      dp_hbdecim_cf32_destroy (hb);
      return 0.0;
    }

  size_t ddc_max_out = dp_ddc_max_out (ddc);

  /* Warm-up */
  hb_out = dp_hbdecim_cf32_execute (hb, in, BLOCK_SIZE, mid, hb_max_out);
  dp_ddc_execute (ddc, mid, hb_out, out, ddc_max_out);
  dp_hbdecim_cf32_reset (hb);
  dp_ddc_reset (ddc);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    {
      hb_out = dp_hbdecim_cf32_execute (hb, in, BLOCK_SIZE, mid, hb_max_out);
      dp_ddc_execute (ddc, mid, hb_out, out, ddc_max_out);
    }
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_hbdecim_cf32_destroy (hb);
  dp_ddc_destroy (ddc);

  /* Throughput relative to original BLOCK_SIZE input samples */
  return (double)ITERATIONS * BLOCK_SIZE / elapsed_sec (&t0, &t1) / 1e6;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main (void)
{
  printf ("=== DDC pipeline benchmark: plain DDC vs HB + DDC ===\n");
  printf ("  block=%d  iters=%d  HB: n_fir=19 N=19 (60 dB)\n", BLOCK_SIZE,
          ITERATIONS);
  printf ("  DDC: built-in M=3 N=19 Kaiser-DPMFS\n");
  printf ("  (%.0f M input samples total per row)\n\n",
          (double)BLOCK_SIZE * ITERATIONS / 1e6);

  /* Build halfband FIR branch: n_fir=19 → 60 dB, N=19 taps */
  size_t hb_N;
  float *hb_fir = build_hb_fir (19, 60.0, &hb_N);

  /* Allocate buffers */
  dp_cf32_t *input = malloc (BLOCK_SIZE * sizeof *input);
  dp_cf32_t *mid = malloc ((BLOCK_SIZE / 2 + hb_N + 4) * sizeof *mid);
  size_t out_cap = (size_t)(BLOCK_SIZE * 2) + 16;
  dp_cf32_t *output = malloc (out_cap * sizeof *output);
  if (!input || !mid || !output || !hb_fir)
    {
      fprintf (stderr, "allocation failed\n");
      return 1;
    }

  /* Two-tone complex input: tones at +0.05 and +0.23 (norm. freq.) */
  for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
      double t0 = 2.0 * M_PI * 0.05 * (double)i;
      double t1 = 2.0 * M_PI * 0.23 * (double)i;
      input[i].i = (float)(cos (t0) + cos (t1));
      input[i].q = (float)(sin (t0) + sin (t1));
    }

  /* NCO tunes the +0.05 tone to DC for all cases. */
  const float norm_freq = -0.05f;

  struct
  {
    double rate;
    const char *label;
  } cases[] = {
    { 0.5, "2x decim" },  { 0.25, "4x decim" },   { 0.125, "8x decim" },
    { 0.1, "10x decim" }, { 0.01, "100x decim" },
  };
  size_t ncases = sizeof cases / sizeof cases[0];

  printf ("  %-10s  %-12s  %12s  %12s  %8s\n", "rate", "config", "plain DDC",
          "HB + DDC", "speedup");
  printf ("  %-10s  %-12s  %12s  %12s  %8s\n", "----------", "------------",
          "  (MSa/s)", "  (MSa/s)", "--------");

  for (size_t i = 0; i < ncases; i++)
    {
      double plain = bench_plain_ddc (norm_freq, cases[i].rate, input, output);
      double combo = bench_hb_ddc (norm_freq, cases[i].rate, hb_N, hb_fir,
                                   input, mid, output);
      printf ("  %-10.4f  %-12s  %12.1f  %12.1f  %7.2fx\n", cases[i].rate,
              cases[i].label, plain, combo, combo / plain);
    }

  printf ("\n");
  free (input);
  free (mid);
  free (output);
  free (hb_fir);
  return 0;
}
