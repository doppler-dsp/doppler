/**
 * agc_demo.c — AGC step-response convergence demo.
 *
 * Feeds the AGC a constant-envelope tone whose power steps by 20 dB
 * partway through.  The linear-in-dB loop drives the output power back
 * to the 0 dB reference after the step — and, because the loop is linear
 * in the dB domain, it settles in the same number of samples regardless
 * of how large the level change was.
 *
 * Prints a downsampled convergence table and writes the full per-sample
 * trace to agc_step_response.csv.  Plot the CSV with
 * examples/python/agc_demo.py (which also runs the demo standalone).
 *
 * Build:
 *   make build
 *   ./build/examples/c/agc_demo
 */

#include <agc/agc_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N_TOTAL 6000    /* total samples processed                   */
#define N_STEP  3000    /* sample index where the input level jumps  */
#define F_TONE  0.02    /* normalised tone frequency (cycles/sample) */
#define REF_DB  0.0     /* AGC target output power                   */
#define LOOP_BW 0.00125 /* loop noise bandwidth, cycles/sample       */
#define ALPHA   0.02    /* power-detector EMA coefficient            */
#define LO_DB   (-10.0) /* input power before the step               */
#define HI_DB   (10.0)  /* input power after the step                */

/* Instantaneous power of a complex sample, in dB. */
static double
db_power (float _Complex z)
{
  double p = (double)crealf (z) * crealf (z) + (double)cimagf (z) * cimagf (z);
  return 10.0 * log10 (p);
}

int
main (void)
{
  agc_state_t *agc = agc_create (REF_DB, LOOP_BW, ALPHA);

  /* Voltage amplitude for a given power level: A = 10^(dB / 20). */
  const double a_lo = pow (10.0, LO_DB / 20.0);
  const double a_hi = pow (10.0, HI_DB / 20.0);

  FILE *csv = fopen ("agc_step_response.csv", "w");
  if (csv)
    fprintf (csv, "sample,in_db,out_db,gain_db\n");

  printf ("=== AGC step-response convergence ===\n");
  printf ("ref = %.1f dB   loop_bw = %g   alpha = %g\n", REF_DB,
          (double)LOOP_BW, (double)ALPHA);
  printf ("input power: %.0f dB -> %.0f dB at sample %d\n\n", LO_DB, HI_DB,
          N_STEP);
  printf ("%-7s  %8s  %8s  %8s\n", "sample", "in_dB", "out_dB", "gain_dB");
  printf ("-------  --------  --------  --------\n");

  for (int n = 0; n < N_TOTAL; n++)
    {
      double amp = (n < N_STEP) ? a_lo : a_hi;
      double ph = 2.0 * M_PI * F_TONE * (double)n;
      float _Complex x = (float)(amp * cos (ph)) + (float)(amp * sin (ph)) * I;

      float _Complex y = agc_step (agc, x);

      double in_db = db_power (x);
      double out_db = db_power (y);
      double gain_db = agc->gain_db; /* loop-filter integrator state */

      if (csv)
        fprintf (csv, "%d,%.6f,%.6f,%.6f\n", n, in_db, out_db, gain_db);

      /* Print every 250th sample plus the two bracketing the step. */
      if (n % 250 == 0 || n == N_STEP - 1 || n == N_STEP)
        printf ("%-7d  %+8.3f  %+8.3f  %+8.3f\n", n, in_db, out_db, gain_db);
    }

  if (csv)
    fclose (csv);

  printf ("\nOutput power settles back to the %.1f dB reference after each\n"
          "step.  Wrote agc_step_response.csv (%d rows).\n",
          REF_DB, N_TOTAL);

  agc_destroy (agc);
  return 0;
}
