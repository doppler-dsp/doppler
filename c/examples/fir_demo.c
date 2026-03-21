/**
 * @file fir_demo.c
 * @brief Demonstrates complex FIR filtering for IQ signal streams.
 *
 * Shows:
 *   1. CF32 filtering — native float IQ (GNU Radio compatible)
 *   2. CI16 filtering — LimeSDR / USRP direct input
 *   3. CI8  filtering — RTL-SDR / HackRF direct input
 *   4. State persistence across buffer boundaries
 */

#include <dp/fir.h>
#include <dp/stream.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Simple low-pass FIR (windowed sinc, real taps)
 * ========================================================================= */

static void
make_lowpass (dp_cf32_t *taps, int N, double cutoff_norm)
{
  /* Hann-windowed sinc, cutoff_norm in (0, 0.5] */
  int half = N / 2;
  for (int k = 0; k < N; k++)
    {
      int n = k - half;
      double sinc = (n == 0) ? 1.0
                             : sin (M_PI * cutoff_norm * 2 * n)
                                   / (M_PI * cutoff_norm * 2 * n);
      double win = 0.5 * (1.0 - cos (2.0 * M_PI * k / (N - 1)));
      taps[k].i = (float)(sinc * win);
      taps[k].q = 0.0f;
    }
}

/* =========================================================================
 * main
 * ========================================================================= */

int
main (void)
{
  printf ("=== doppler FIR filter demo ===\n\n");

  /* Design a 15-tap low-pass filter, cutoff = 0.1 * Fs */
  const int N = 15;
  dp_cf32_t taps[15];
  make_lowpass (taps, N, 0.1);

  printf ("15-tap Hann-windowed sinc LP taps (real part):\n  ");
  for (int i = 0; i < N; i++)
    printf ("%7.4f", (double)taps[i].i);
  printf ("\n\n");

  dp_fir_t *fir = dp_fir_create (taps, N);
  if (!fir)
    {
      fprintf (stderr, "dp_fir_create failed\n");
      return 1;
    }

  /* -----------------------------------------------------------------
   * 1. CF32 — complex tone at 0.3*Fs mixed with high-frequency noise
   * ----------------------------------------------------------------- */
  printf ("--- CF32 input (GNU Radio / native float) ---\n");
  {
    const int S = 32;
    dp_cf32_t in[32], out[32];

    for (int i = 0; i < S; i++)
      {
        double phase_lo = 2.0 * M_PI * 0.05 * i; /* 0.05*Fs tone */
        double phase_hi = 2.0 * M_PI * 0.4 * i;  /* 0.4*Fs tone */
        in[i].i = (float)(cos (phase_lo) + 0.5 * cos (phase_hi));
        in[i].q = (float)(sin (phase_lo) + 0.5 * sin (phase_hi));
      }

    dp_fir_execute_cf32 (fir, in, out, S);

    printf ("  first 8 output samples (LF tone survives, HF attenuated):\n");
    for (int i = 0; i < 8; i++)
      printf ("  [%2d]  in=(%6.3f,%6.3f)  out=(%6.3f,%6.3f)\n", i,
              (double)in[i].i, (double)in[i].q, (double)out[i].i,
              (double)out[i].q);
    printf ("\n");
  }

  dp_fir_reset (fir);

  /* -----------------------------------------------------------------
   * 2. CI16 — LimeSDR / USRP raw samples
   * ----------------------------------------------------------------- */
  printf ("--- CI16 input (LimeSDR / USRP, 4 bytes/sample) ---\n");
  {
    const int S = 16;
    dp_ci16_t in[16];
    dp_cf32_t out[16];

    for (int i = 0; i < S; i++)
      {
        /* Scale 0.05*Fs tone to ±16000 */
        double phase = 2.0 * M_PI * 0.05 * i;
        in[i].i = (int16_t)(16000.0 * cos (phase));
        in[i].q = (int16_t)(16000.0 * sin (phase));
      }

    dp_fir_execute_ci16 (fir, in, out, S);

    printf ("  first 4 output samples:\n");
    for (int i = 0; i < 4; i++)
      printf ("  [%2d]  in=(%6d,%6d)  out=(%10.1f,%10.1f)\n", i, in[i].i,
              in[i].q, (double)out[i].i, (double)out[i].q);
    printf ("\n");
  }

  dp_fir_reset (fir);

  /* -----------------------------------------------------------------
   * 3. CI8 — RTL-SDR / HackRF raw samples (2 bytes/sample)
   * ----------------------------------------------------------------- */
  printf ("--- CI8 input (RTL-SDR / HackRF, 2 bytes/sample) ---\n");
  {
    const int S = 16;
    dp_ci8_t in[16];
    dp_cf32_t out[16];

    for (int i = 0; i < S; i++)
      {
        double phase = 2.0 * M_PI * 0.05 * i;
        in[i].i = (int8_t)(100.0 * cos (phase));
        in[i].q = (int8_t)(100.0 * sin (phase));
      }

    dp_fir_execute_ci8 (fir, in, out, S);

    printf ("  first 4 output samples:\n");
    for (int i = 0; i < 4; i++)
      printf ("  [%2d]  in=(%4d,%4d)  out=(%8.1f,%8.1f)\n", i, in[i].i,
              in[i].q, (double)out[i].i, (double)out[i].q);
    printf ("\n");
  }

  /* -----------------------------------------------------------------
   * 4. State across calls — 2 blocks of 8 samples
   * ----------------------------------------------------------------- */
  printf ("--- stateful: 2×8 CF32 blocks (delay line persists) ---\n");
  {
    dp_fir_reset (fir);
    dp_cf32_t in1[8], in2[8], out1[8], out2[8];

    for (int i = 0; i < 8; i++)
      {
        double phase = 2.0 * M_PI * 0.05 * i;
        in1[i].i = (float)cos (phase);
        in1[i].q = (float)sin (phase);
      }
    for (int i = 0; i < 8; i++)
      {
        double phase = 2.0 * M_PI * 0.05 * (i + 8);
        in2[i].i = (float)cos (phase);
        in2[i].q = (float)sin (phase);
      }

    dp_fir_execute_cf32 (fir, in1, out1, 8);
    dp_fir_execute_cf32 (fir, in2, out2, 8);

    printf ("  block 1 last sample out: (%6.3f, %6.3f)\n", (double)out1[7].i,
            (double)out1[7].q);
    printf ("  block 2 first sample out: (%6.3f, %6.3f)\n", (double)out2[0].i,
            (double)out2[0].q);
    printf ("  (continuity demonstrates delay line carries across calls)\n\n");
  }

  dp_fir_destroy (fir);
  printf ("Done.\n");
  return 0;
}
