/*
 * fft_demo.c — demonstration of the doppler FFT API
 *
 * Shows 1-D and 2-D complex FFTs via the global-plan interface and
 * highlights the correct zero-copy usage pattern for maximum speed.
 *
 * IMPORTANT — zero-copy contract:
 *   1. Allocate your input and output buffers.
 *   2. Call dp_fft_global_setup() with those buffers already allocated.
 *      The setup call BINDS the plan to the specific pointer addresses.
 *      Use "estimate" so the planner does not overwrite your data.
 *   3. Fill the input buffer.
 *   4. Call dp_fft1d_execute() / dp_fft2d_execute() passing the
 *      SAME pointers.  The library executes in-place on those pointers
 *      with zero copy — no internal buffer or memcpy.
 *   5. Repeat steps 3-4 as many times as needed.
 *
 * For in-place transforms (dp_fft1d_execute_inplace), use the same
 * buffer for both input and output in step 1, then bind with setup,
 * fill, and execute.
 *
 * Key FFT facts used in this demo:
 *
 *   cos(2π k/N)  →  energy at bins ±1  (bin 0 = DC, no offset in cosine)
 *   sin(2π k/N)  →  energy at bins ±1, purely imaginary
 *
 *   For a length-N full-cycle cosine (convention: sign=+1 = forward DFT):
 *     Y[0]   ≈  0       (zero DC)
 *     Y[1]   ≈ +N/2     (positive frequency, real part)
 *     Y[N-1] ≈ +N/2     (negative frequency, conjugate symmetric)
 *
 *   For a length-N full-cycle sine:
 *     Y[1]   ≈ -j·N/2
 *     Y[N-1] ≈ +j·N/2
 *
 *   DC bin (Y[0]) = sum of all input values.
 *
 * Usage: fft_demo [--help]
 */

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dp/fft.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [--help]\n", prog);
  printf ("\n");
  printf (
      "Demonstrates the doppler FFT global API (zero-copy, FFTW-backed):\n");
  printf ("  dp_fft_global_setup()      — bind plan to caller's buffers\n");
  printf ("  dp_fft1d_execute()         — 1-D FFT, out-of-place\n");
  printf ("  dp_fft1d_execute_inplace() — 1-D FFT, in-place\n");
  printf ("  dp_fft2d_execute()         — 2-D FFT, out-of-place\n");
  printf ("  dp_fft2d_execute_inplace() — 2-D FFT, in-place\n");
  printf ("\n");
  printf ("The demo verifies each result against the expected DFT output\n");
  printf ("and prints the key bins so you can confirm correctness.\n");
}

static void
fill_cosine (double complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      double t = 2.0 * M_PI * (double)i / (double)n;
      x[i] = cos (t); /* one full cycle, real-only */
    }
}

static void
fill_sine (double complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    {
      double t = 2.0 * M_PI * (double)i / (double)n;
      x[i] = sin (t); /* one full cycle, real-only */
    }
}

/* Print the first `show` and last `show` bins of a complex spectrum. */
static void
print_spectrum (const double complex *Y, size_t N, size_t show)
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
main (int argc, char *argv[])
{
  if (argc > 1
      && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      print_usage (argv[0]);
      return 0;
    }

  printf ("=== doppler FFT Demo ===\n");
  printf ("\n");
  printf ("Zero-copy pattern: allocate buffers → setup (bind) → fill → "
          "execute.\n");

  /* ------------------------------------------------------------------ */
  /* 1-D FFT, out-of-place: cosine input                                 */
  /* ------------------------------------------------------------------ */
  {
    const size_t N = 16;
    size_t shape[1] = { N };

    printf ("\n--- 1-D out-of-place FFT (N=%zu, input: cos(2πk/N)) ---\n", N);
    printf ("Expected: Y[1] ≈ +%.1f (real), Y[%zu] ≈ +%.1f (real), rest ≈ 0\n",
            (double)N / 2.0, N - 1, (double)N / 2.0);

    /* Step 1: allocate */
    double complex *x = malloc (N * sizeof (double complex));
    double complex *y = malloc (N * sizeof (double complex));
    if (!x || !y)
      {
        fprintf (stderr, "malloc failed\n");
        return 1;
      }

    /* Step 2: bind plan to these exact pointers ("estimate" = no data clobber)
     */
    dp_fft_global_setup (shape, 1, +1, 1, "estimate", "");

    /* Step 3: fill */
    fill_cosine (x, N);

    /* Step 4: execute (zero-copy — same pointers as bound above) */
    dp_fft1d_execute (x, y);

    print_spectrum (y, N, 3);

    free (x);
    free (y);
  }

  /* ------------------------------------------------------------------ */
  /* 1-D FFT, in-place: sine input                                       */
  /* ------------------------------------------------------------------ */
  {
    const size_t N = 16;
    size_t shape[1] = { N };

    printf ("\n--- 1-D in-place FFT (N=%zu, input: sin(2πk/N)) ---\n", N);
    printf ("Expected: Y[1] ≈ -j%.1f, Y[%zu] ≈ +j%.1f, rest ≈ 0\n",
            (double)N / 2.0, N - 1, (double)N / 2.0);

    double complex *x = malloc (N * sizeof (double complex));
    if (!x)
      {
        fprintf (stderr, "malloc failed\n");
        return 1;
      }

    dp_fft_global_setup (shape, 1, +1, 1, "estimate", "");
    fill_sine (x, N);
    dp_fft1d_execute_inplace (x);

    print_spectrum (x, N, 3);

    free (x);
  }

  /* ------------------------------------------------------------------ */
  /* 2-D FFT, out-of-place: sin(k) input                                 */
  /* ------------------------------------------------------------------ */
  {
    const size_t NY = 4, NX = 4;
    const size_t total = NY * NX;
    size_t shape[2] = { NY, NX };

    printf ("\n--- 2-D out-of-place FFT (%zu×%zu, input: sin(k)) ---\n", NY,
            NX);

    double complex *x = malloc (total * sizeof (double complex));
    double complex *y = malloc (total * sizeof (double complex));
    if (!x || !y)
      {
        fprintf (stderr, "malloc failed\n");
        return 1;
      }

    dp_fft_global_setup (shape, 2, +1, 1, "estimate", "");

    for (size_t i = 0; i < total; i++)
      x[i] = sin ((double)i);

    /* Save DC reference before execute (execute does not modify x here) */
    double complex dc_ref = 0;
    for (size_t i = 0; i < total; i++)
      dc_ref += x[i];

    printf ("Input (row-major):\n");
    for (size_t row = 0; row < NY; row++)
      {
        printf ("  row %zu:", row);
        for (size_t col = 0; col < NX; col++)
          printf ("  %+.4f", creal (x[row * NX + col]));
        printf ("\n");
      }

    dp_fft2d_execute (x, y);

    printf ("Output[0,0]: %+.4f %+.4fj  (DC = sum of inputs)\n", creal (y[0]),
            cimag (y[0]));
    printf ("Expected DC: %+.4f %+.4fj\n", creal (dc_ref), cimag (dc_ref));

    free (x);
    free (y);
  }

  /* ------------------------------------------------------------------ */
  /* 2-D FFT, in-place                                                   */
  /* ------------------------------------------------------------------ */
  {
    const size_t NY = 4, NX = 4;
    const size_t total = NY * NX;
    size_t shape[2] = { NY, NX };

    printf ("\n--- 2-D in-place FFT (%zu×%zu, same sin(k) input) ---\n", NY,
            NX);

    double complex *x = malloc (total * sizeof (double complex));
    if (!x)
      {
        fprintf (stderr, "malloc failed\n");
        return 1;
      }

    dp_fft_global_setup (shape, 2, +1, 1, "estimate", "");

    for (size_t i = 0; i < total; i++)
      x[i] = sin ((double)i);

    dp_fft2d_execute_inplace (x);

    printf ("Output[0,0]: %+.4f %+.4fj  (same DC as out-of-place)\n",
            creal (x[0]), cimag (x[0]));

    free (x);
  }

  printf ("\nDemo complete.\n");
  return 0;
}
