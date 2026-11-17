/**
 * state_filter_demo.c -- a frequency ramp the second-order loop cannot hold.
 *
 * A carrier loop that updates once per code period and needs a narrow noise
 * bandwidth meets a Doppler that drifts: the carrier frequency is a ramp.
 * The second-order filter (dp_loop_filter) nulls a frequency step but not a
 * ramp: its steady phase error is r/ki for a ramp of r cycles per update per
 * update. The third-order state filter keeps a rate estimate in cascade with
 * the frequency estimate, so the same ramp is tracked with zero steady
 * phase error, at the same noise bandwidth, from the same two numbers.
 *
 * Shows:
 *   1. Both loops closed on a ramp, against the closed form the object owns
 *   2. The lifecycle a C caller manages: create, step, destroy, and a
 *      create() that refuses a bn*t past the stability bound
 *   3. The exact bandwidth and stability for a detector delay and loop gain
 *
 * Build and run:
 *   make build
 *   ./build/native/examples/state_filter_demo
 */
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/state_filter/state_filter_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define BN 0.02
#define RAMP 2e-7 /* the carrier frequency drifts by this per update */
#define N 40000

int
main (void)
{
  printf ("=== doppler state filter demo ===\n\n");

  /* 1. Close both loops on the ramp. The detector is ideal: the error is
   *    the input phase less the oscillator's, and the oscillator
   *    accumulates the control. */
  dp_loop_filter_state_t  *second = dp_loop_filter_create (BN, 0.707, 1.0);
  dp_state_filter_state_t *third  = dp_state_filter_create (BN, 1.0);
  if (!second || !third)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }
  double freq = 0.0, w = 0.0, p2 = 0.0, p3 = 0.0, e2 = 0.0, e3 = 0.0;
  for (int n = 0; n < N; n++)
    {
      freq += RAMP;
      w += freq;
      e2 = w - p2;
      p2 += dp_loop_filter_step (second, e2);
      e3 = w - p3;
      p3 += dp_state_filter_step (third, e3);
    }
  const double predicted
      = dp_state_filter_steady_error (2, second->ki, 0.0, 2, RAMP);
  printf ("--- 1. A ramp of %g cycles/update^2 into a loop of bn = %g ---\n",
          RAMP, BN);
  printf ("  second order: steady phase error %+.3e cycles (r/ki = %+.3e)\n",
          e2, predicted);
  printf ("  third order:  steady phase error %+.3e cycles\n\n", e3);

  /* 2. The bound: bn*t past it is refused, with NULL. */
  printf ("--- 2. The domain ---\n");
  dp_state_filter_state_t *too_wide = dp_state_filter_create (0.5, 1.0);
  printf ("  create(bn = 0.5): %s (bn*t is limited to %g)\n\n",
          too_wide ? "accepted" : "refused", STATE_FILTER_BNT_MAX);

  /* 3. What the loop delivers for a detector it cannot see. */
  const double bw_ideal = dp_state_filter_bandwidth (third, 0.0, 1.0);
  const double bw_late  = dp_state_filter_bandwidth (third, 1.0, 1.0);
  const double bw_low   = dp_state_filter_bandwidth (third, 0.0, 0.7);
  printf ("--- 3. The exact bandwidth ---\n");
  printf ("  ideal %.5f, one update late %.5f, slope 0.7 %.5f\n\n", bw_ideal,
          bw_late, bw_low);

  int ok = 1;
  ok &= fabs (e2 / predicted - 1.0) < 1e-4; /* the closed form */
  ok &= fabs (e3) < 1e-6 * fabs (e2);       /* the ramp is nulled */
  ok &= too_wide == NULL;                   /* the bound is enforced */
  ok &= fabs (bw_ideal / BN - 1.0) < 1e-6;  /* the bandwidth asked for */
  ok &= bw_late > bw_ideal && bw_ideal > bw_low;

  dp_loop_filter_destroy (second);
  dp_state_filter_destroy (third);
  if (!ok)
    {
      fprintf (stderr, "FAILED\n");
      return 1;
    }
  printf ("OK: the ramp is nulled, and the bound is enforced\n");
  return 0;
}
