/**
 * @file nco_demo.c
 * @brief NCO demo — free-running tone and FM modulation.
 *
 * Demonstrates:
 *   1. Free-running NCO at f_n = 0.25 (quarter sample rate).
 *      The I/Q samples rotate 90° per sample, cycling through
 *      (1,0) → (0,1) → (-1,0) → (0,-1) → ...
 *
 *   2. FM-modulated NCO: a sine-wave modulation signal applied to
 *      the phase-increment control port.
 *
 * Build via the project Makefile:
 *   make build
 *   ./build/nco_demo
 */

#include <dp/nco.h>

#include <math.h>
#include <stdio.h>

#define N_FREE 16     /* free-running samples to print         */
#define N_FM 32       /* FM-modulated samples to generate      */
#define FM_RATE 0.25f /* modulating tone: f_n of the sine wave */
#define FM_DEV 0.05f  /* peak frequency deviation (normalised) */

int
main (void)
{
  /* --------------------------------------------------------------- *
   * 1. Free-running quarter-rate NCO                                *
   * --------------------------------------------------------------- */
  printf ("=== Free-running NCO  (f_n = 0.25) ===\n");
  printf ("%-6s  %9s  %9s\n", "sample", "I", "Q");
  printf ("------  ---------  ---------\n");

  dp_nco_t *nco = dp_nco_create (0.25f);
  dp_cf32_t out[N_FREE];
  dp_nco_execute_cf32 (nco, out, N_FREE);

  for (int i = 0; i < N_FREE; i++)
    printf ("%-6d  %+9.6f  %+9.6f\n", i, (double)out[i].i, (double)out[i].q);

  dp_nco_destroy (nco);

  /* --------------------------------------------------------------- *
   * 2. FM-modulated NCO                                             *
   *                                                                 *
   * Carrier: f_n = 0.1                                             *
   * Modulator: sine wave at FM_RATE, amplitude FM_DEV              *
   *   → instantaneous frequency sweeps 0.1 ± 0.05                 *
   * --------------------------------------------------------------- */
  printf ("\n=== FM NCO  (carrier 0.10, dev ±%.2f, mod %.2f) ===\n",
          (double)FM_DEV, (double)FM_RATE);
  printf ("%-6s  %9s  %9s  %9s\n", "sample", "I", "Q", "f_inst");
  printf ("------  ---------  ---------  ---------\n");

  float ctrl[N_FM];
  for (int i = 0; i < N_FM; i++)
    ctrl[i] = FM_DEV * sinf (2.0f * (float)M_PI * FM_RATE * (float)i);

  dp_nco_t *fm = dp_nco_create (0.1f);
  dp_cf32_t fmo[N_FM];
  dp_nco_execute_cf32_ctrl (fm, ctrl, fmo, N_FM);

  for (int i = 0; i < N_FM; i++)
    printf ("%-6d  %+9.6f  %+9.6f  %+9.6f\n", i, (double)fmo[i].i,
            (double)fmo[i].q, (double)(0.1f + ctrl[i]));

  dp_nco_destroy (fm);
  return 0;
}
