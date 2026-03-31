/**
 * @file bench_hbdecim.c
 * @brief Throughput benchmark for dp_hbdecim_cf32.
 *
 * Measures input MSamples/s for the halfband 2:1 decimator at several
 * FIR branch lengths (N), then compares against the general polyphase
 * resampler (dp_resamp_cf32, phases=2, rate=0.5) with the same N.
 *
 * Run with: ./bench_hbdecim_c
 */

#include <dp/hbdecim.h>
#include <dp/resamp.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BLOCK_SIZE 65536
#define ITERATIONS 400

/* ------------------------------------------------------------------ */
/* Timer                                                               */
/* ------------------------------------------------------------------ */

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (t1->tv_sec - t0->tv_sec) + (t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* ------------------------------------------------------------------ */
/* Halfband FIR branch builder                                         */
/*                                                                     */
/* Designs a halfband prototype via Kaiser window (cutoff π/2),        */
/* scales by phases=2, then returns the FIR polyphase branch.          */
/* The FIR branch is bank[0] (even-indexed taps) when M (half-length   */
/* of prototype) is even, or bank[1] (odd-indexed taps) when M is odd. */
/* Returns the FIR branch length N and which branch is FIR.            */
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

/**
 * Build a halfband FIR branch for phases=2.
 *
 * @param n_fir     Desired FIR branch length (roughly controls quality).
 *                  The actual prototype is htaps ≈ 2*n_fir - 1.
 * @param atten     Stopband attenuation in dB.
 * @param[out] N    Actual FIR branch length returned here.
 * @return          Heap-allocated float[N] — caller must free().
 */
static float *
build_hb_fir (int n_fir, double atten, size_t *N_out)
{
  /* Halfband prototype: cutoff at π/2, Kaiser window */
  int M = n_fir - 1;     /* half-length of prototype */
  int htaps = 2 * M + 1; /* prototype length (odd)   */
  double beta = (atten > 50.0)    ? 0.1102 * (atten - 8.7)
                : (atten >= 21.0) ? 0.5842 * pow (atten - 21.0, 0.4)
                                        + 0.07886 * (atten - 21.0)
                                  : 0.0;

  /* Prototype: windowed sinc at ω_c = π/2, scaled for unity DC gain. */
  double *h = calloc ((size_t)htaps, sizeof (double));
  for (int n = 0; n < htaps; n++)
    {
      double w = kaiser_win (n, htaps, beta);
      double m = n - M;
      /* Halfband sinc: h_ideal[m] = sin(π/2 * m) / (π * m), h[0]=0.5 */
      double s = (m == 0.0) ? 0.5 : sin (0.5 * M_PI * m) / (M_PI * m);
      h[n] = w * s * 2.0; /* × phases=2 scale */
    }

  /* phases=2 polyphase: bank[0][k]=h[2k], bank[1][k]=h[2k+1].
   * The delay branch has a dominant 1.0 at its centre tap; the FIR
   * branch has the actual windowed-sinc taps.
   * N = ceil(htaps / 2) = M + 1.                                       */
  size_t N = (size_t)M + 1;
  float *bank0 = malloc (N * sizeof (float));
  float *bank1 = malloc (N * sizeof (float));
  for (size_t k = 0; k < N; k++)
    {
      bank0[k] = (2 * k < (size_t)htaps) ? (float)h[2 * k] : 0.0f;
      bank1[k] = (2 * k + 1 < (size_t)htaps) ? (float)h[2 * k + 1] : 0.0f;
    }
  free (h);

  /* Select the FIR branch (dominant centre ≈ 1.0 → delay branch). */
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

/* Build a 2-phase polyphase bank for dp_resamp_cf32 (phases=2) with
 * the same Kaiser prototype as above.  Returns the full bank[2][N].  */
static float *
build_resamp_bank2 (int n_fir, double atten, size_t *N_out)
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
  float *bank = malloc (2 * N * sizeof (float));
  for (size_t k = 0; k < N; k++)
    {
      bank[0 * N + k] = (2 * k < (size_t)htaps) ? (float)h[2 * k] : 0.0f;
      bank[1 * N + k]
          = (2 * k + 1 < (size_t)htaps) ? (float)h[2 * k + 1] : 0.0f;
    }
  free (h);
  *N_out = N;
  return bank;
}

/* ------------------------------------------------------------------ */
/* Benchmark runners                                                   */
/* ------------------------------------------------------------------ */

static double
bench_hbdecim (size_t N, const float *h_fir, const dp_cf32_t *in,
               dp_cf32_t *out)
{
  dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create (N, h_fir);

  size_t max_out = BLOCK_SIZE / 2 + N + 2;
  dp_hbdecim_cf32_execute (r, in, BLOCK_SIZE, out, max_out); /* warm up */
  dp_hbdecim_cf32_reset (r);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_hbdecim_cf32_execute (r, in, BLOCK_SIZE, out, max_out);
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_hbdecim_cf32_destroy (r);
  return (double)ITERATIONS * BLOCK_SIZE / elapsed_sec (&t0, &t1) / 1e6;
}

static double
bench_resamp2 (size_t N, const float *bank, const dp_cf32_t *in,
               dp_cf32_t *out)
{
  dp_resamp_cf32_t *r = dp_resamp_cf32_create (2, N, bank, 0.5);

  size_t max_out = BLOCK_SIZE / 2 + N + 2;
  dp_resamp_cf32_execute (r, in, BLOCK_SIZE, out, max_out); /* warm up */
  dp_resamp_cf32_reset (r);

  struct timespec t0, t1;
  clock_gettime (CLOCK_MONOTONIC, &t0);
  for (int i = 0; i < ITERATIONS; i++)
    dp_resamp_cf32_execute (r, in, BLOCK_SIZE, out, max_out);
  clock_gettime (CLOCK_MONOTONIC, &t1);

  dp_resamp_cf32_destroy (r);
  return (double)ITERATIONS * BLOCK_SIZE / elapsed_sec (&t0, &t1) / 1e6;
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int
main (void)
{
  printf ("=== dp_hbdecim_cf32 throughput benchmark ===\n");
  printf ("  block=%d  iters=%d  (%.0f M input samples total)\n\n", BLOCK_SIZE,
          ITERATIONS, (double)BLOCK_SIZE * ITERATIONS / 1e6);

  /* Two-tone complex input */
  dp_cf32_t *input = malloc (BLOCK_SIZE * sizeof *input);
  size_t max_out = BLOCK_SIZE / 2 + 256;
  dp_cf32_t *output = malloc (max_out * sizeof *output);
  if (!input || !output)
    {
      fprintf (stderr, "allocation failed\n");
      return 1;
    }
  for (size_t i = 0; i < BLOCK_SIZE; i++)
    {
      double t0 = 2.0 * M_PI * 0.1 * (double)i;
      double t1 = 2.0 * M_PI * 0.37 * (double)i;
      input[i].i = (float)(cos (t0) + cos (t1));
      input[i].q = (float)(sin (t0) + sin (t1));
    }

  /* n_fir choices: 10, 19, 37, 73 (covering ~40-80 dB attenuation) */
  int sizes[] = { 10, 19, 37, 73 };
  double attens[] = { 40.0, 60.0, 80.0, 80.0 };
  size_t nsizes = sizeof sizes / sizeof sizes[0];

  printf ("  %-6s  %-8s  %-12s  %-12s  %-8s\n", "N", "atten", "hbdecim",
          "resamp×2", "speedup");
  printf ("  %-6s  %-8s  %-12s  %-12s  %-8s\n", "------", "--------",
          "------------", "------------", "--------");

  for (size_t i = 0; i < nsizes; i++)
    {
      int n_fir = sizes[i];
      double atten = attens[i];

      size_t N;
      float *h_fir = build_hb_fir (n_fir, atten, &N);

      size_t N2;
      float *bank2 = build_resamp_bank2 (n_fir, atten, &N2);

      double msa_hb = bench_hbdecim (N, h_fir, input, output);
      double msa_rs = bench_resamp2 (N2, bank2, input, output);
      double speedup = msa_hb / msa_rs;

      printf ("  %-6zu  %-8.0f  %10.1f    %10.1f    %6.2fx\n", N, atten,
              msa_hb, msa_rs, speedup);

      free (h_fir);
      free (bank2);
    }

  printf ("\n  (MSa/s = input samples per second)\n");

  free (input);
  free (output);
  return 0;
}
