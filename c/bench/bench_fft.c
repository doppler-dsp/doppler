// C/bench/bench_fft.c
// Benchmark doppler FFT global API in MSamples/sec.
#define _POSIX_C_SOURCE 200809L

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dp/fft.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double
now_sec (void)
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void
fill_signal (double complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      double t = 2.0 * M_PI * (double)i / (double)n;
      x[i] = cos (t) + I * sin (2.0 * t);
    }
}

static void
bench_1d (size_t n, int iters)
{
  printf ("\n=== 1D FFT Benchmark (n = %zu) ===\n", n);

  size_t shape[1] = { n };
  dp_fft_global_setup (shape, 1, +1, 1, "estimate", "");

  double complex *x = malloc (n * sizeof (double complex));
  double complex *y = malloc (n * sizeof (double complex));

  fill_signal (x, n);

  /* First call: defines bound buffers and creates plan */
  dp_fft1d_execute (x, y);

  double t0 = now_sec ();
  for (int i = 0; i < iters; i++)
    dp_fft1d_execute (x, y);
  double t1 = now_sec ();

  double dt = (t1 - t0) / iters;
  double Msps = ((double)n / dt) / 1e6;

  printf ("fft1d_execute:          dt = %.6f s, MS/s = %.2f\n", dt, Msps);

  /* In-place: separate setup, separate plan */
  dp_fft_global_setup (shape, 1, +1, 1, "estimate", "");

  fill_signal (x, n);
  dp_fft1d_execute_inplace (x); /* first call binds plan */

  t0 = now_sec ();
  for (int i = 0; i < iters; i++)
    dp_fft1d_execute_inplace (x);
  t1 = now_sec ();

  dt = (t1 - t0) / iters;
  Msps = ((double)n / dt) / 1e6;

  printf ("fft1d_execute_inplace:  dt = %.6f s, MS/s = %.2f\n", dt, Msps);

  free (x);
  free (y);
}

static void
bench_2d (size_t ny, size_t nx, int iters)
{
  printf ("\n=== 2D FFT Benchmark (%zux%zu) ===\n", ny, nx);

  size_t shape[2] = { ny, nx };
  size_t total = ny * nx;

  dp_fft_global_setup (shape, 2, +1, 1, "estimate", "");

  double complex *x = malloc (total * sizeof (double complex));
  double complex *y = malloc (total * sizeof (double complex));

  for (size_t i = 0; i < total; i++)
    x[i] = sin ((double)i) + I * cos ((double)i);

  dp_fft2d_execute (x, y); /* first call binds plan */

  double t0 = now_sec ();
  for (int i = 0; i < iters; i++)
    dp_fft2d_execute (x, y);
  double t1 = now_sec ();

  double dt = (t1 - t0) / iters;
  double Msps = ((double)total / dt) / 1e6;

  printf ("fft2d_execute:          dt = %.6f s, MS/s = %.2f\n", dt, Msps);

  /* In-place: separate setup, separate plan */
  dp_fft_global_setup (shape, 2, +1, 1, "estimate", "");

  for (size_t i = 0; i < total; i++)
    x[i] = sin ((double)i);

  dp_fft2d_execute_inplace (x); /* first call binds plan */

  t0 = now_sec ();
  for (int i = 0; i < iters; i++)
    dp_fft2d_execute_inplace (x);
  t1 = now_sec ();

  dt = (t1 - t0) / iters;
  Msps = ((double)total / dt) / 1e6;

  printf ("fft2d_execute_inplace:  dt = %.6f s, MS/s = %.2f\n", dt, Msps);

  free (x);
  free (y);
}

int
main (void)
{
  printf ("doppler FFT Benchmark\n");

  bench_1d (1024, 2000);
  bench_1d (4096, 1000);
  bench_1d (16384, 200);

  bench_2d (64, 64, 2000);
  bench_2d (128, 128, 500);
  bench_2d (256, 256, 100);

  printf ("\nDone.\n");
  return 0;
}
