/*
 * simd_demo.c — demonstrates dp_c16_mul: SIMD-optimised complex multiplication
 *
 * dp_c16_mul(a, b) computes a * b for double complex operands using a
 * hardware-accelerated path (AVX2/SSE2 on x86-64, NEON on AArch64) with a
 * scalar C99 fallback on other architectures.
 *
 * Complex multiplication formula:
 *   (a_r + a_i*j) * (b_r + b_i*j)
 *     = (a_r*b_r - a_i*b_i) + (a_r*b_i + a_i*b_r)*j
 *
 * Usage: simd_demo [--help]
 */

#include "dp/simd.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void
print_usage (const char *prog)
{
  printf ("Usage: %s [--help]\n", prog);
  printf ("\n");
  printf (
      "Demonstrates dp_c16_mul: SIMD-accelerated complex multiplication.\n");
  printf ("Runs several test cases and compares against the C99 formula.\n");
  printf ("\n");
  printf ("Formula: (a_r + a_i*j) * (b_r + b_i*j)\n");
  printf ("           = (a_r*b_r - a_i*b_i) + (a_r*b_i + a_i*b_r)*j\n");
}

/* Reference scalar complex multiply for comparison. */
static double complex
ref_mul (double complex a, double complex b)
{
  return (creal (a) * creal (b) - cimag (a) * cimag (b))
         + (creal (a) * cimag (b) + cimag (a) * creal (b)) * I;
}

static void
run_case (const char *label, double complex a, double complex b)
{
  double complex result = dp_c16_mul (a, b);
  double complex expect = ref_mul (a, b);
  double err = cabs (result - expect);

  printf ("  %-30s  result: (%+.6f, %+.6f)  expect: (%+.6f, %+.6f)  err: %.2e "
          " %s\n",
          label, creal (result), cimag (result), creal (expect),
          cimag (expect), err, err < 1e-12 ? "OK" : "MISMATCH");
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

  printf ("=== doppler SIMD Demo: dp_c16_mul ===\n");
  printf ("\n");
  printf ("Each row: dp_c16_mul(a, b) vs. scalar reference.\n");
  printf ("\n");

  /* Basic algebraic cases */
  run_case ("(1+2j) * (3+4j)", 1.0 + 2.0 * I, 3.0 + 4.0 * I);

  run_case ("(1+0j) * (0+1j) = 0+1j", 1.0 + 0.0 * I, 0.0 + 1.0 * I);

  run_case ("(0+1j) * (0+1j)  [j*j = -1]", 0.0 + 1.0 * I, 0.0 + 1.0 * I);

  run_case ("(-3+4j) * (2-5j)", -3.0 + 4.0 * I, 2.0 - 5.0 * I);

  /* Unit-magnitude phasors: e^(j*pi/4) * e^(j*pi/4) = e^(j*pi/2) = j */
  double complex p45 = cos (M_PI / 4.0) + sin (M_PI / 4.0) * I;
  run_case ("e^(j*pi/4) * e^(j*pi/4) = j", p45, p45);

  /* Scale by real factor: 2*(1+1j) = 2+2j */
  run_case ("2*(1+1j)  [real scale]", 2.0 + 0.0 * I, 1.0 + 1.0 * I);

  /* Zero */
  run_case ("(1+2j) * 0", 1.0 + 2.0 * I, 0.0 + 0.0 * I);

  /* Large values (no overflow for double) */
  run_case ("(1e8+2e8j) * (3e8+4e8j)", 1e8 + 2e8 * I, 3e8 + 4e8 * I);

  printf ("\nDone.\n");
  return 0;
}
