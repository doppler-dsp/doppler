/**
 * fir_demo.c — FIR filter demo for CF32 IQ signal streams.
 *
 * Shows:
 *   1. CF32 filtering — float IQ (GNU Radio compatible)
 *   2. State persistence across buffer boundaries
 *
 * The current API accepts only float complex (CF32) input.
 * Integer SDR input (CI8, CI16) must be converted to CF32 before
 * calling fir_execute().
 *
 * Build:
 *   make build
 *   ./build/native/examples/fir_demo
 */

#include <fir/fir_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Hann-windowed sinc low-pass filter, cutoff_norm in (0, 0.5]. */
static void
make_lowpass (float *taps, int N, double cutoff_norm)
{
  int half = N / 2;
  for (int k = 0; k < N; k++)
    {
      int    n    = k - half;
      double sinc = (n == 0) ? 1.0
                             : sin (M_PI * cutoff_norm * 2 * n)
                                   / (M_PI * cutoff_norm * 2 * n);
      double win  = 0.5 * (1.0 - cos (2.0 * M_PI * k / (N - 1)));
      taps[k]     = (float)(sinc * win);
    }
}

int
main (void)
{
  printf ("=== doppler FIR filter demo ===\n\n");

  /* 15-tap Hann-windowed sinc LP, cutoff = 0.1 * Fs */
  const int N = 15;
  float     taps[15];
  make_lowpass (taps, N, 0.1);

  printf ("15-tap Hann-windowed sinc LP taps:\n  ");
  for (int i = 0; i < N; i++)
    printf ("%7.4f", (double)taps[i]);
  printf ("\n\n");

  fir_state_t *fir = fir_create_real (taps, (size_t)N);
  if (!fir)
    {
      fprintf (stderr, "fir_create_real failed\n");
      return 1;
    }

  /* ------------------------------------------------------------------ *
   * 1. CF32 — complex tone at 0.05*Fs mixed with high-frequency noise  *
   * ------------------------------------------------------------------ */
  printf ("--- CF32 input (GNU Radio / native float) ---\n");
  {
    const int S = 32;
    float _Complex in[32], out[32];

    for (int i = 0; i < S; i++)
      {
        double phase_lo = 2.0 * M_PI * 0.05 * i; /* 0.05*Fs tone */
        double phase_hi = 2.0 * M_PI * 0.4 * i;  /* 0.4*Fs tone  */
        in[i] = CMPLXF ((float)(cos (phase_lo) + 0.5 * cos (phase_hi)),
                        (float)(sin (phase_lo) + 0.5 * sin (phase_hi)));
      }

    fir_execute (fir, in, S, out);

    printf ("  first 8 output samples (LF tone survives, HF attenuated):\n");
    for (int i = 0; i < 8; i++)
      printf ("  [%2d]  in=(%6.3f,%6.3f)  out=(%6.3f,%6.3f)\n", i,
              (double)crealf (in[i]), (double)cimagf (in[i]),
              (double)crealf (out[i]), (double)cimagf (out[i]));
    printf ("\n");
  }

  fir_reset (fir);

  /* ------------------------------------------------------------------ *
   * 2. State across calls — 2 blocks of 8 samples                      *
   * ------------------------------------------------------------------ */
  printf ("--- stateful: 2×8 CF32 blocks (delay line persists) ---\n");
  {
    float _Complex in1[8], in2[8], out1[8], out2[8];

    for (int i = 0; i < 8; i++)
      {
        double phase = 2.0 * M_PI * 0.05 * i;
        in1[i]       = CMPLXF ((float)cos (phase), (float)sin (phase));
      }
    for (int i = 0; i < 8; i++)
      {
        double phase = 2.0 * M_PI * 0.05 * (i + 8);
        in2[i]       = CMPLXF ((float)cos (phase), (float)sin (phase));
      }

    fir_execute (fir, in1, 8, out1);
    fir_execute (fir, in2, 8, out2);

    printf ("  block 1 last sample out:  (%6.3f, %6.3f)\n",
            (double)crealf (out1[7]), (double)cimagf (out1[7]));
    printf ("  block 2 first sample out: (%6.3f, %6.3f)\n",
            (double)crealf (out2[0]), (double)cimagf (out2[0]));
    printf ("  (continuity confirms delay line carries across calls)\n\n");
  }

  fir_destroy (fir);
  printf ("Done.\n");
  return 0;
}
