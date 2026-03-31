/**
 * @file hbdecim_demo.c
 * @brief Halfband decimator demo — 2:1 decimation of a complex tone.
 *
 * Demonstrates:
 *   1. Creating a halfband decimator with a simple symmetric filter.
 *   2. Decimating a complex tone by 2, showing input and output samples.
 *   3. DC gain near unity, confirming spectral response.
 *
 * Build via the project Makefile:
 *   make build
 *   ./build/hbdecim_demo
 */

#include <dp/hbdecim.h>

#include <math.h>
#include <stdio.h>

#define N_TAPS 4
#define N_IN 32

/* Symmetric FIR coefficients for a minimal halfband filter
 * (4-tap, symmetric, designed for basic bandpass).
 * Source: test_hbdecim.c minimal test case.
 */
static const float H_FIR[N_TAPS] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

/* Compute the RMS power of a complex signal in dB */
static double
rms_db (const dp_cf32_t *x, size_t n)
{
  double s = 0.0;
  for (size_t k = 0; k < n; k++)
    s += (double)x[k].i * x[k].i + (double)x[k].q * x[k].q;
  return 10.0 * log10 (s / (double)n + 1e-20);
}

int
main (void)
{
  printf ("=== Halfband Decimator Demo ===\n\n");

  /* Create decimator with the symmetric FIR branch */
  dp_hbdecim_cf32_t *dec = dp_hbdecim_cf32_create (N_TAPS, H_FIR);
  if (!dec)
    {
      printf ("ERROR: Failed to create decimator\n");
      return 1;
    }

  printf ("Decimator created: %zu taps, rate=%.1f\n\n",
          dp_hbdecim_cf32_num_taps (dec), dp_hbdecim_cf32_rate (dec));

  /* Generate a complex tone at 0.125 cycles/sample (eighth rate) */
  double tone_freq = 0.125;
  dp_cf32_t in[N_IN];
  for (int k = 0; k < N_IN; k++)
    {
      double ph = 2.0 * M_PI * tone_freq * (double)k;
      in[k].i = (float)cos (ph);
      in[k].q = (float)sin (ph);
    }

  printf ("Input: %d samples at f_n = %.3f\n", N_IN, tone_freq);
  printf ("Input RMS power: %.2f dBFS\n\n", rms_db (in, N_IN));

  /* Decimate: expecting ~16 output samples from 32 inputs */
  dp_cf32_t out[32];
  size_t n_out = dp_hbdecim_cf32_execute (dec, in, N_IN, out, 32);

  printf ("Output: %zu samples (decimation ratio 2:1)\n", n_out);
  printf ("Output RMS power: %.2f dBFS\n\n", rms_db (out, n_out));

  /* Show first 8 samples */
  printf ("First 8 output samples:\n");
  printf ("%-4s  %10s  %10s\n", "idx", "I", "Q");
  printf ("----  ----------  ----------\n");
  for (size_t k = 0; k < (n_out < 8 ? n_out : 8); k++)
    printf ("%-4zu  %+10.6f  %+10.6f\n", k, (double)out[k].i,
            (double)out[k].q);

  dp_hbdecim_cf32_destroy (dec);
  return 0;
}
