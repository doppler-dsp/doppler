/**
 * @file bench_resamp.c
 * @brief Throughput benchmark for dp_resamp_cf32 polyphase resampler.
 *
 * Reports input MSamples/s for interpolation and decimation at
 * several rates.  The polyphase bank is built inline (same Kaiser
 * design as the test suite) so the benchmark is self-contained.
 *
 * Run with: ./bench_resamp_c
 */

#include <dp/resamp.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK_SIZE 65536 /* input samples per call              */
#define ITERATIONS 200   /* calls per measurement               */

/* ------------------------------------------------------------------ */
/* Timer helpers                                                      */
/* ------------------------------------------------------------------ */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* ------------------------------------------------------------------ */
/* Kaiser prototype (same as test_resamp.c)                           */
/* ------------------------------------------------------------------ */

static double
kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

static int
kaiser_taps_fn (double atten, double pb, double sb)
{
  return (int)(1.0 + (atten - 8.0) / 2.285 / (2.0 * M_PI * (sb - pb)));
}

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

/* Build a polyphase bank with an explicit number of phases L and
 * taps-per-phase N.  The prototype length is L*N taps.  Used for
 * fair comparison with DPMFS (same delay-line depth, variable L). */
static float *
build_bank_fixed (size_t L, size_t N, size_t *out_taps)
{
  double atten = 60.0, pb = 0.4, sb = 0.6;
  int htaps = (int)(L * N);
  if (htaps % 2 == 0)
    htaps++; /* force odd (linear phase) */
  int halflen = htaps / 2;
  double beta = kaiser_beta (atten);
  double wc = 2.0 * M_PI * (pb / L + (sb - pb) / (2.0 * L));

  double *g = calloc ((size_t)htaps, sizeof (double));
  for (int i = 0; i < htaps; i++)
    {
      double m = i - halflen;
      double w = kaiser_win (i, htaps, beta);
      double s = (m == 0.0) ? 1.0 : sin (wc * m) / (wc * m);
      g[i] = w * wc / M_PI * s * (double)L;
    }

  size_t tpp = N;
  float *bank = malloc (L * tpp * sizeof (float));
  for (size_t p = 0; p < L; p++)
    for (size_t t = 0; t < tpp; t++)
      {
        size_t idx = t * L + p;
        bank[p * tpp + t] = (idx < (size_t)htaps) ? (float)g[idx] : 0.0f;
      }

  free (g);
  *out_taps = tpp;
  return bank;
}

static float *
build_bank_n (double img, size_t *out_phases, size_t *out_taps)
{
  double atten = 60.0, pb = 0.4, sb = 0.6;
  int log2L = (int)ceil ((20.0 * log10 (pb) + img) / 6.02);
  size_t L = (size_t)1 << log2L;
  int halflen = kaiser_taps_fn (atten, pb / L, sb / L) / 2;
  int htaps = 2 * halflen + 1;
  size_t tpp = (size_t)(htaps / (int)L) + 1;
  size_t plen = L * tpp;
  double beta = kaiser_beta (atten);
  double wc = 2.0 * M_PI * (pb / L + (sb - pb) / (2.0 * L));

  double *g = calloc (plen, sizeof (double));
  for (int i = 0; i < htaps; i++)
    {
      double m = i - halflen;
      double w = kaiser_win (i, htaps, beta);
      double s = (m == 0.0) ? 1.0 : sin (wc * m) / (wc * m);
      g[i] = w * wc / M_PI * s * (double)L;
    }

  float *bank = malloc (L * tpp * sizeof (float));
  for (size_t p = 0; p < L; p++)
    for (size_t t = 0; t < tpp; t++)
      bank[p * tpp + t] = (float)g[t * L + p];

  free (g);
  *out_phases = L;
  *out_taps = tpp;
  return bank;
}

/* ------------------------------------------------------------------ */
/* Benchmark runner                                                   */
/* ------------------------------------------------------------------ */

static double
bench_rate (const float *bank, size_t L, size_t N, double rate,
            const dp_cf32_t *input, dp_cf32_t *output, size_t max_out)
{
  dp_resamp_cf32_t *r = dp_resamp_cf32_create (L, N, bank, rate);

  /* Warm up (fill delay line / transposed state) */
  dp_resamp_cf32_execute (r, input, BLOCK_SIZE, output, max_out);
  dp_resamp_cf32_reset (r);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_resamp_cf32_execute (r, input, BLOCK_SIZE, output, max_out);
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_resamp_cf32_destroy (r);

  double sec = elapsed_sec (&t0, &t1);
  return (double)ITERATIONS * BLOCK_SIZE / sec / 1e6;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

static void
run_bench_fixed (size_t L, size_t N, const dp_cf32_t *input, dp_cf32_t *output,
                 size_t max_out)
{
  size_t tpp;
  float *bank = build_bank_fixed (L, N, &tpp);
  printf ("  --- L=%zu phases × N=%zu taps (%zu bytes float32) ---\n", L, tpp,
          L * tpp * 4);

  double rates[] = { 4.0, 2.0333, 1.5, 1.0, 0.75, 0.50333, 0.25 };
  size_t nrates = sizeof rates / sizeof rates[0];

  printf ("  %-12s  %-8s  %10s\n", "rate", "mode", "MSa/s (in)");
  printf ("  %-12s  %-8s  %10s\n", "------------", "--------", "----------");

  for (size_t i = 0; i < nrates; i++)
    {
      double msa = bench_rate (bank, L, tpp, rates[i], input, output, max_out);
      const char *mode = rates[i] >= 1.0 ? "interp" : "decim";
      printf ("  %-12.5f  %-8s  %10.1f\n", rates[i], mode, msa);
    }

  printf ("\n");
  free (bank);
}

static void
run_bench (double img_db, const dp_cf32_t *input, dp_cf32_t *output,
           size_t max_out)
{
  size_t L, N;
  float *bank = build_bank_n (img_db, &L, &N);
  printf ("  --- image_atten=%.0f dB: %zu phases"
          " × %zu taps (%zu KB float32) ---\n",
          img_db, L, N, L * N * 4 / 1024);

  double rates[] = { 4.0, 2.0333, 1.5, 1.0, 0.75, 0.50333, 0.25 };
  size_t nrates = sizeof rates / sizeof rates[0];

  printf ("  %-12s  %-8s  %10s\n", "rate", "mode", "MSa/s (in)");
  printf ("  %-12s  %-8s  %10s\n", "------------", "--------", "----------");

  for (size_t i = 0; i < nrates; i++)
    {
      double msa = bench_rate (bank, L, N, rates[i], input, output, max_out);
      const char *mode = rates[i] >= 1.0 ? "interp" : "decim";
      printf ("  %-12.5f  %-8s  %10.1f\n", rates[i], mode, msa);
    }

  printf ("\n");
  free (bank);
}

int
main (void)
{
  printf ("=== doppler resampler benchmark ===\n");
  printf ("  block=%d  iters=%d  "
          "(%.0f M input samples/rate)\n\n",
          BLOCK_SIZE, ITERATIONS, (double)BLOCK_SIZE * ITERATIONS / 1e6);

  /* Generate input: two-tone complex signal */
  dp_cf32_t *input = malloc (BLOCK_SIZE * sizeof *input);
  for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
      double t0 = 2.0 * M_PI * 0.1 * (double)i;
      double t1 = 2.0 * M_PI * 0.37 * (double)i;
      input[i].i = (float)(cos (t0) + cos (t1));
      input[i].q = (float)(sin (t0) + sin (t1));
    }

  /* Output buffer — sized for the highest rate */
  size_t max_out = (size_t)(BLOCK_SIZE * 5.0) + 16;
  dp_cf32_t *output = malloc (max_out * sizeof *output);

  if (!input || !output)
    {
      fprintf (stderr, "allocation failed\n");
      return 1;
    }

  /* Fixed N=19 taps/phase, varying L — fair comparison with DPMFS */
  printf ("=== Fixed N=19: varying L (same delay-line depth) ===\n\n");
  run_bench_fixed (8, 19, input, output, max_out);
  run_bench_fixed (32, 19, input, output, max_out);
  run_bench_fixed (64, 19, input, output, max_out);
  run_bench_fixed (128, 19, input, output, max_out);

  /* Auto-L from image attenuation (standard design) */
  printf ("=== Auto-L from image attenuation ===\n\n");
  run_bench (80.0, input, output, max_out);
  run_bench (65.0, input, output, max_out);

  free (input);
  free (output);
  return 0;
}
