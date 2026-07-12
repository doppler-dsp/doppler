/**
 * hbdecim_demo.c — Halfband 2:1 decimator demo.
 *
 * Demonstrates:
 *   1. Creating a halfband decimator with a minimal symmetric FIR.
 *   2. Decimating a complex tone by 2, showing input and output samples.
 *   3. RMS power before and after decimation (should be near unity for
 *      a passband tone after 2:1 decimation).
 *
 * Build:
 *   make build
 *   ./build/native/examples/hbdecim_demo
 */

#include <HalfbandDecimator/HalfbandDecimator_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N_TAPS 4
#define N_IN 32

/* 4-tap symmetric halfband coefficients (minimal passband test case). */
static const float H_FIR[N_TAPS] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

static double
rms_db (const float _Complex *x, size_t n)
{
  double s = 0.0;
  for (size_t k = 0; k < n; k++)
    s += (double)crealf (x[k]) * crealf (x[k])
         + (double)cimagf (x[k]) * cimagf (x[k]);
  return 10.0 * log10 (s / (double)n + 1e-20);
}

int
main (void)
{
  printf ("=== Halfband Decimator Demo ===\n\n");

  HalfbandDecimator_state_t *dec = HalfbandDecimator_create (N_TAPS, H_FIR);
  if (!dec)
    {
      fprintf (stderr, "ERROR: HalfbandDecimator_create failed\n");
      return 1;
    }

  printf ("Decimator created: %zu taps, rate=%.1f\n\n", (size_t)N_TAPS,
          HalfbandDecimator_get_rate (dec));

  /* Complex tone at f_n = 0.125 (eighth rate — well inside passband) */
  const double tone_freq = 0.125;
  float _Complex in[N_IN];
  for (int k = 0; k < N_IN; k++)
    {
      double ph = 2.0 * M_PI * tone_freq * (double)k;
      in[k]     = CMPLXF ((float)cos (ph), (float)sin (ph));
    }

  printf ("Input:  %d samples at f_n = %.3f\n", N_IN, tone_freq);
  printf ("Input RMS:  %.2f dBFS\n\n", rms_db (in, N_IN));

  float _Complex out[32];
  size_t n_out = HalfbandDecimator_execute (dec, in, N_IN, out);

  printf ("Output: %zu samples (2:1 decimation)\n", n_out);
  printf ("Output RMS: %.2f dBFS\n\n", rms_db (out, n_out));

  printf ("First 8 output samples:\n");
  printf ("%-4s  %10s  %10s\n", "idx", "I", "Q");
  printf ("----  ----------  ----------\n");
  for (size_t k = 0; k < (n_out < 8 ? n_out : 8); k++)
    printf ("%-4zu  %+10.6f  %+10.6f\n", k, (double)crealf (out[k]),
            (double)cimagf (out[k]));

  HalfbandDecimator_destroy (dec);
  return 0;
}
