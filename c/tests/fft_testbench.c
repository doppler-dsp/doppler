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
max_abs_error (const double complex *a, const double complex *b, size_t n)
{
  double maxerr = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      double err = cabs (a[i] - b[i]);
      if (err > maxerr)
        maxerr = err;
    }
  return maxerr;
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

/* ------------------------------------------------------------
 * 1D TEST
 * ------------------------------------------------------------ */
static int
test_fft_1d (size_t n)
{
  printf ("\n=== Testing 1D FFT (n = %zu) ===\n", n);

  size_t shape[1] = { n };

  double complex *x = malloc (n * sizeof (double complex));
  double complex *y = malloc (n * sizeof (double complex));
  double complex *z = malloc (n * sizeof (double complex));

  fill_signal (x, n);

  /* Forward plan */
  dp_fft_global_setup (shape, 1, +1, 1, "measure", "");
  dp_fft1d_execute (x, y);

  /* Inverse plan */
  dp_fft_global_setup (shape, 1, -1, 1, "measure", "");
  dp_fft1d_execute (y, z);

  /* Normalize inverse FFT */
  for (size_t i = 0; i < n; i++)
    z[i] /= (double)n;

  double err = max_abs_error (x, z, n);
  printf ("Max error: %.3e\n", err);

  free (x);
  free (y);
  free (z);

  if (err < 1e-9)
    {
      printf ("PASS\n");
      return 1;
    }
  else
    {
      printf ("FAIL\n");
      return 0;
    }
}

/* ------------------------------------------------------------
 * 2D TEST
 * ------------------------------------------------------------ */
static int
test_fft_2d (size_t ny, size_t nx)
{
  printf ("\n=== Testing 2D FFT (%zux%zu) ===\n", ny, nx);

  size_t shape[2] = { ny, nx };
  size_t total = ny * nx;

  double complex *x = malloc (total * sizeof (double complex));
  double complex *y = malloc (total * sizeof (double complex));
  double complex *z = malloc (total * sizeof (double complex));

  for (size_t i = 0; i < total; i++)
    x[i] = sin ((double)i) + I * cos ((double)i);

  /* Forward */
  dp_fft_global_setup (shape, 2, +1, 1, "measure", "");
  dp_fft2d_execute (x, y);

  /* Inverse */
  dp_fft_global_setup (shape, 2, -1, 1, "measure", "");
  dp_fft2d_execute (y, z);

  /* Normalize */
  for (size_t i = 0; i < total; i++)
    z[i] /= (double)total;

  double err = max_abs_error (x, z, total);
  printf ("Max error: %.3e\n", err);

  free (x);
  free (y);
  free (z);

  if (err < 1e-9)
    {
      printf ("PASS\n");
      return 1;
    }
  else
    {
      printf ("FAIL\n");
      return 0;
    }
}

/* ------------------------------------------------------------
 * MAIN
 * ------------------------------------------------------------ */
int
main (void)
{
  printf ("doppler FFT Testbench\n");

  int ok = 1;

  ok &= test_fft_1d (16);
  ok &= test_fft_1d (1024);
  ok &= test_fft_1d (4096);

  ok &= test_fft_2d (8, 8);
  ok &= test_fft_2d (32, 32);
  ok &= test_fft_2d (64, 64);

  printf ("\nOverall result: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
