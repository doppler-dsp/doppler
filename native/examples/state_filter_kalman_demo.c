/**
 * state_filter_kalman_demo.c -- a loop sized from the signal, and a fast
 * start.
 *
 * The fixed-gain third-order filter is sized from a bandwidth. A caller who
 * knows the detector's noise and how fast the carrier's rate can change would
 * rather give those two figures and get the bandwidth that is optimal for
 * them: that is the Kalman flavor, which computes its gains each update from a
 * covariance recursion. It takes the rms detector error when locked (sigma_e)
 * and the rms change of the rate per update (sigma_a), both in the loop's own
 * units, or a bandwidth alone.
 *
 * Shows:
 *   1. Sizing from the signal: the one-line conversion a caller with a
 *      carrier-to-noise density and a jerk figure makes (an example, not an
 *      API: the filter has no detector model in it)
 *   2. A high-gain start: an initial frequency uncertainty larger than the
 *      steady state's settles a frequency step faster than the fixed-gain loop
 *      of the same final bandwidth, and a start past what the stability
 *      rectangle allows is refused
 *   3. configure() with every argument 0: the gains go to the steady state at
 *      once, the estimates carry, so a loop that has pulled in ends its
 *      high-gain start early
 *   4. The lifecycle a C caller manages: create, step, configure, destroy
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/state_filter_kalman_demo
 */
#include "doppler/state_filter/state_filter_core.h"
#include "doppler/state_filter_kalman/state_filter_kalman_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CN0_DBHZ 40.0 /* carrier-to-noise density at the detector        */
#define T 1e-3        /* one loop update per millisecond                  */
#define JERK 500.0    /* worst carrier jerk, cycles per second cubed      */
#define F0 2e-3       /* the frequency step, cycles per update            */
#define N 4000
#define PI 3.14159265358979323846

/* Updates until the phase error stays under 5% of its peak after a frequency
 * step of F0, with an ideal detector and no noise. `kalman` selects the
 * filter: a Kalman one, or the fixed-gain one. */
static int
settle (void *filter, int kalman)
{
  static double err[N];
  double        phase_in = 0.0, phase_osc = 0.0, peak = 0.0;
  for (int i = 0; i < N; i++)
    {
      phase_in += F0;
      err[i] = phase_in - phase_osc;
      phase_osc += kalman ? dp_state_filter_kalman_step (filter, err[i])
                          : dp_state_filter_step (filter, err[i]);
      if (fabs (err[i]) > peak)
        peak = fabs (err[i]);
    }
  int last = 0;
  for (int i = 0; i < N; i++)
    if (fabs (err[i]) > 0.05 * peak)
      last = i;
  return last + 1;
}

int
main (void)
{
  printf ("=== doppler Kalman state filter demo ===\n\n");

  /* 1. The two figures from a C/N0 and a jerk. The per-update carrier-to-
   *    noise ratio is C/N0 times the update period; an arctan detector's rms
   *    error at that ratio is 1 / (2 pi sqrt (2 snr)) cycles; the rate's
   *    change per update is the jerk times T^3. */
  const double snr     = pow (10.0, CN0_DBHZ / 10.0) * T;
  const double sigma_e = 1.0 / (2.0 * PI * sqrt (2.0 * snr));
  const double sigma_a = JERK * T * T * T;

  /* t is in units of the update here (the figures are per update), so the
   * bandwidth below is per update; times 1/T it is in Hz. */
  dp_state_filter_kalman_state_t *steady
      = dp_state_filter_kalman_create (sigma_e, sigma_a, 1.0, 0.0);
  if (!steady)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }
  const double bn = dp_state_filter_kalman_bandwidth (steady, 0.0, 1.0);
  printf ("--- 1. Sized from the signal ---\n");
  printf ("  sigma_e %.4f cycles, sigma_a %.2e cycles/update^2\n", sigma_e,
          sigma_a);
  printf ("  delivered bandwidth %.4f cycles/update (%.1f Hz)\n\n", bn,
          bn / T);

  /* 2. A high-gain start against the fixed-gain loop of the same bandwidth. */
  const double sigma_f = dp_state_filter_kalman_sigma_f (steady);
  dp_state_filter_kalman_state_t *fast
      = dp_state_filter_kalman_create (sigma_e, sigma_a, 1.0, 4.0 * sigma_f);
  dp_state_filter_state_t *fixed = dp_state_filter_create (bn, 1.0);
  /* a start far past what the stability rectangle allows is refused */
  dp_state_filter_kalman_state_t *too_far
      = dp_state_filter_kalman_create (sigma_e, sigma_a, 1.0, 50.0 * sigma_f);
  if (!fast || !fixed)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }
  const int n_fixed = settle (fixed, 0);
  const int n_fast  = settle (fast, 1);
  printf ("--- 2. A high-gain start ---\n");
  printf ("  settling to a frequency step: fixed-gain %d updates, start of 4 "
          "times sigma_f %d (%.2f)\n",
          n_fixed, n_fast, (double)n_fast / (double)n_fixed);
  printf ("  a start of 50 times sigma_f: %s\n\n",
          too_far ? "accepted" : "refused");

  /* 3. End the start early: configure() with every argument 0 puts the gains
   *    at the steady state at once and keeps the estimates. */
  dp_state_filter_kalman_state_t *ended
      = dp_state_filter_kalman_create (sigma_e, sigma_a, 1.0, 4.0 * sigma_f);
  if (!ended)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }
  for (int i = 0; i < 60; i++)
    dp_state_filter_kalman_step (ended, 0.05);
  const double kr_mid = ended->base.kr, integ_mid = ended->base.integ;
  const int    rc
      = dp_state_filter_kalman_configure (ended, 0.0, 0.0, 0.0, 0.0, 0.0);
  printf ("--- 3. configure() ends the start ---\n");
  printf ("  rate gain kr: %.3e mid-start, %.3e after configure() (steady "
          "state %.3e); frequency estimate carried: %s\n\n",
          kr_mid, ended->base.kr, steady->base.kr,
          ended->base.integ == integ_mid ? "yes" : "no");

  int ok = 1;
  ok &= bn > 1e-6 && bn < 0.12
        && dp_state_filter_kalman_stable (steady, 2.0, 1.0);
  ok &= fabs (dp_state_filter_kalman_bandwidth (steady, 0.0, 1.0) / bn - 1.0)
        < 1e-12;
  ok &= n_fast < 0.8 * n_fixed; /* the start settles the step faster */
  ok &= too_far == NULL;        /* a start past the rectangle is refused */
  ok &= fabs (kr_mid / steady->base.kr - 1.0) > 1e-3; /* mid-start: moving */
  ok &= rc == DP_OK && ended->base.integ == integ_mid;
  ok &= fabs (ended->base.kr / steady->base.kr - 1.0) < 1e-12;

  dp_state_filter_kalman_destroy (steady);
  dp_state_filter_kalman_destroy (fast);
  dp_state_filter_kalman_destroy (ended);
  dp_state_filter_destroy (fixed);
  if (!ok)
    {
      fprintf (stderr, "FAILED\n");
      return 1;
    }
  printf ("OK: sized from the signal, the start settles faster, and "
          "configure() ends it\n");
  return 0;
}
