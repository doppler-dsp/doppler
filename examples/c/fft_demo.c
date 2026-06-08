/**
 * fft_demo.c — doppler FFT API demonstration.
 *
 * Shows 1-D forward and inverse FFTs using the instance-based API
 * (fft_create / fft_execute_cf64 / fft_destroy).
 *
 * Key facts used in this demo:
 *
 *   cos(2π k/N) → energy at bins ±1 (no DC for zero-mean cosine)
 *     Y[1]   ≈ +N/2  (positive-freq bin, real)
 *     Y[N-1] ≈ +N/2  (negative-freq bin, conjugate-symmetric)
 *
 *   sin(2π k/N) → energy at bins ±1 (purely imaginary)
 *     Y[1]   ≈ -j·N/2
 *     Y[N-1] ≈ +j·N/2
 *
 *   DC bin Y[0] = sum of all input values.
 *
 * Build:
 *   make build
 *   ./build/native/examples/fft_demo
 */

#include <fft/fft_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Print the first `show` bins (and last `show`) of a spectrum. */
static void
print_spectrum (const double _Complex *Y, size_t N, size_t show)
{
  printf ("    %-5s  %12s  %12s\n", "bin", "real", "imag");
  for (size_t k = 0; k < show && k < N; k++)
    printf ("    [%3zu]  %+12.4f  %+12.4f\n", k, creal (Y[k]), cimag (Y[k]));
  if (2 * show < N)
    {
      printf ("     ...\n");
      for (size_t k = N - show; k < N; k++)
        printf ("    [%3zu]  %+12.4f  %+12.4f\n", k, creal (Y[k]),
                cimag (Y[k]));
    }
}

int
main (void)
{
  printf ("=== doppler FFT Demo ===\n\n");

  /* ------------------------------------------------------------------ *
   * 1-D forward FFT — cosine input                                      *
   * ------------------------------------------------------------------ */
  {
    const size_t N = 16;
    printf ("--- 1-D forward FFT (N=%zu, input: cos(2πk/N)) ---\n", N);
    printf ("Expected: Y[1] ≈ +%.1f (real), Y[%zu] ≈ +%.1f (real)\n",
            (double)N / 2.0, N - 1, (double)N / 2.0);

    double _Complex *x = malloc (N * sizeof (double _Complex));
    double _Complex *y = malloc (N * sizeof (double _Complex));
    if (!x || !y)
      {
        fputs ("malloc failed\n", stderr);
        return 1;
      }

    for (size_t i = 0; i < N; i++)
      x[i] = cos (2.0 * M_PI * (double)i / (double)N);

    /* sign = +1 → forward DFT; nthreads = 1 */
    fft_state_t *fft = fft_create (N, +1, 1);
    fft_execute_cf64 (fft, x, N, y);
    print_spectrum (y, N, 3);
    fft_destroy (fft);

    free (x);
    free (y);
  }

  /* ------------------------------------------------------------------ *
   * 1-D forward FFT — sine input                                        *
   * ------------------------------------------------------------------ */
  {
    const size_t N = 16;
    printf ("\n--- 1-D forward FFT (N=%zu, input: sin(2πk/N)) ---\n", N);
    printf ("Expected: Y[1] ≈ -j%.1f, Y[%zu] ≈ +j%.1f\n", (double)N / 2.0,
            N - 1, (double)N / 2.0);

    double _Complex *x = malloc (N * sizeof (double _Complex));
    double _Complex *y = malloc (N * sizeof (double _Complex));
    if (!x || !y)
      {
        fputs ("malloc failed\n", stderr);
        return 1;
      }

    for (size_t i = 0; i < N; i++)
      x[i] = sin (2.0 * M_PI * (double)i / (double)N);

    fft_state_t *fft = fft_create (N, +1, 1);
    fft_execute_cf64 (fft, x, N, y);
    print_spectrum (y, N, 3);
    fft_destroy (fft);

    free (x);
    free (y);
  }

  /* ------------------------------------------------------------------ *
   * Round-trip: forward then inverse, verify DC                         *
   * ------------------------------------------------------------------ */
  {
    const size_t N = 16;
    printf ("\n--- Round-trip FFT→IFFT (N=%zu) ---\n", N);

    double _Complex *x   = malloc (N * sizeof (double _Complex));
    double _Complex *mid = malloc (N * sizeof (double _Complex));
    double _Complex *out = malloc (N * sizeof (double _Complex));
    if (!x || !mid || !out)
      {
        fputs ("malloc failed\n", stderr);
        return 1;
      }

    /* Mixed tone input */
    for (size_t i = 0; i < N; i++)
      {
        double re = cos (2.0 * M_PI * 2.0 * (double)i / (double)N);
        double im = sin (2.0 * M_PI * 2.0 * (double)i / (double)N);
        x[i]      = re + im * _Complex_I;
      }

    fft_state_t *fwd = fft_create (N, +1, 1);
    fft_state_t *inv = fft_create (N, -1, 1);

    fft_execute_cf64 (fwd, x, N, mid);

    /* IFFT output needs 1/N normalisation */
    fft_execute_cf64 (inv, mid, N, out);
    for (size_t i = 0; i < N; i++)
      out[i] /= (double)N;

    double max_err = 0.0;
    for (size_t i = 0; i < N; i++)
      {
        double err = cabs (out[i] - x[i]);
        if (err > max_err)
          max_err = err;
      }
    printf ("    Max reconstruction error: %e  (should be ~0)\n", max_err);

    fft_destroy (fwd);
    fft_destroy (inv);
    free (x);
    free (mid);
    free (out);
  }

  printf ("\nDemo complete.\n");
  return 0;
}
