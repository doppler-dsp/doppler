/**
 * nco_demo.c — LO phasor generator and NCO raw phase accumulator demo.
 *
 * Demonstrates:
 *   1. Free-running LO at f_n = 0.25 (quarter sample rate).
 *      I/Q samples rotate 90° per sample, cycling through
 *      (1,0) → (0,1) → (-1,0) → (0,-1) → ...
 *
 *   2. FM-modulated LO: a sine-wave modulation signal applied to
 *      the per-sample frequency control port via lo_steps_ctrl().
 *
 *   3. Raw NCO: uint32 phase accumulator and overflow-carry output
 *      via nco_steps_u32_ovf().
 *
 * Build:
 *   make build
 *   ./build/native/examples/nco_demo
 */

#include <lo/lo_core.h>
#include <nco/nco_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N_FREE 16     /* free-running samples to print        */
#define N_FM 32       /* FM-modulated samples to generate     */
#define FM_RATE 0.25f /* modulating tone: f_n of the sine wave */
#define FM_DEV 0.05f  /* peak frequency deviation (normalised) */
#define N_NCO 16      /* raw NCO samples to print             */

int
main (void)
{
  /* ------------------------------------------------------------------ *
   * 1. Free-running LO at f_n = 0.25                                   *
   * ------------------------------------------------------------------ */
  printf ("=== Free-running LO  (f_n = 0.25) ===\n");
  printf ("%-6s  %9s  %9s\n", "sample", "I", "Q");
  printf ("------  ---------  ---------\n");

  lo_state_t *lo = lo_create (0.25);
  float _Complex out[N_FREE];
  lo_steps (lo, N_FREE, out);

  for (int i = 0; i < N_FREE; i++)
    printf ("%-6d  %+9.6f  %+9.6f\n", i, (double)crealf (out[i]),
            (double)cimagf (out[i]));

  lo_destroy (lo);

  /* ------------------------------------------------------------------ *
   * 2. FM-modulated LO                                                  *
   *    Carrier: f_n = 0.1                                              *
   *    Modulator: sine at FM_RATE, amplitude FM_DEV                    *
   *    → instantaneous frequency sweeps 0.1 ± 0.05                    *
   * ------------------------------------------------------------------ */
  printf ("\n=== FM LO  (carrier 0.10, dev ±%.2f, mod %.2f) ===\n",
          (double)FM_DEV, (double)FM_RATE);
  printf ("%-6s  %9s  %9s  %9s\n", "sample", "I", "Q", "f_inst");
  printf ("------  ---------  ---------  ---------\n");

  float ctrl[N_FM];
  for (int i = 0; i < N_FM; i++)
    ctrl[i] = FM_DEV * sinf (2.0f * (float)M_PI * FM_RATE * (float)i);

  lo_state_t *fm = lo_create (0.1);
  float _Complex fmo[N_FM];
  lo_steps_ctrl (fm, ctrl, N_FM, fmo);

  for (int i = 0; i < N_FM; i++)
    printf ("%-6d  %+9.6f  %+9.6f  %+9.6f\n", i, (double)crealf (fmo[i]),
            (double)cimagf (fmo[i]), (double)(0.1f + ctrl[i]));

  lo_destroy (fm);

  /* ------------------------------------------------------------------ *
   * 3. Raw NCO — uint32 phase + per-sample carry flag                  *
   *    nmax = 0 → pure 32-bit wrap; nmax > 0 → scaled accumulator      *
   * ------------------------------------------------------------------ */
  printf ("\n=== Raw NCO  (f_n = 0.25, uint32 phase + carry) ===\n");
  printf ("%-6s  %12s  %5s\n", "sample", "phase", "carry");
  printf ("------  ------------  -----\n");

  nco_state_t *nco = nco_create (0.25, 0);
  uint32_t     phase[N_NCO];
  uint8_t      carry[N_NCO];
  nco_steps_u32_ovf (nco, N_NCO, phase, carry);

  for (int i = 0; i < N_NCO; i++)
    printf ("%-6d  %12u  %5u\n", i, phase[i], carry[i]);

  nco_destroy (nco);
  return 0;
}
