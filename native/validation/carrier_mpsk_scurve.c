/**
 * @file carrier_mpsk_scurve.c
 * @brief Validation: the M-PSK carrier loop's open-loop discriminator S-curve
 *        e(phi) is the decision-directed sawtooth — period 2*pi/M, unit slope
 *        at the origin, peak +-sin(pi/M).
 *
 * For a decision-directed M-PSK phase discriminator the error for an input
 * prompt P = a * exp(j*phi) (a a constellation point) is
 *   e = Im(P * conj(ahat)) / |P|,  ahat = nearest constellation point to P.
 * While the decision is correct (|phi| < pi/M) the nearest point is `a`
 * itself, so e = Im(exp(j*phi)) = sin(phi): a sine through the origin with
 * unit slope. As phi crosses +-pi/M the slicer jumps to the adjacent point and
 * e snaps to sin(phi -+ 2*pi/M) — a SAWTOOTH of period 2*pi/M, the signature
 * of an M-fold phase ambiguity. By rotational symmetry the curve is identical
 * for every constellation point, so we average over all M to be robust to any
 * asymmetry.
 *
 * The harness drives the real carrier_mpsk_update() discriminator (a fresh,
 * frozen loop per phi: norm_freq 0 so the wipe-off is identity, bn_fll 0 so
 * the FLL is inert and skipped on the first symbol) and reads s.last_error =
 * e(phi).
 *
 * Validated, per M in {2,4,8}: e(0)=0; unit slope de/dphi|_0 = 1; peak |e| =
 * sin(pi/M) just inside the decision boundary; periodicity e(phi) = e(phi -
 * 2*pi/M); a zero crossing at every k*2*pi/M.
 *
 * Usage:  carrier_mpsk_scurve [--check]
 */
#include "carrier_mpsk/carrier_mpsk_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TWOPI 6.283185307179586

/* Open-loop discriminator e(phi), averaged over the M constellation points
 * (identical by symmetry; the average just guards against asymmetry). */
static double
scurve_e (int m, double phi)
{
  carrier_mpsk_state_t s;
  carrier_mpsk_init (&s, 0.01, 0.707, 0.0, 1, 0.0, m);
  float complex rot = (float complex)cexp (I * phi);
  double        acc = 0.0;
  for (int g = 0; g < m; g++)
    {
      carrier_mpsk_reset (&s); /* clears have_prev so the FLL stays inert */
      float complex p = mpsk_constellation (g, m) * rot;
      carrier_mpsk_update (&s, p);
      acc += s.last_error;
    }
  return acc / (double)m;
}

static int
check_order (int m)
{
  double seg  = TWOPI / (double)m;      /* one sawtooth period          */
  double peak = sin (M_PI / (double)m); /* discriminator peak         */
  int    fail = 0;

  /* e(0) = 0 */
  double e0 = scurve_e (m, 0.0);
  if (fabs (e0) > 1e-6)
    fail = 1;

  /* unit slope at the origin (central difference) */
  double d     = 1e-3;
  double slope = (scurve_e (m, d) - scurve_e (m, -d)) / (2.0 * d);
  if (fabs (slope - 1.0) > 1e-2)
    fail = 1;

  /* peak just inside the decision boundary phi -> (pi/M)^- */
  double e_peak = scurve_e (m, M_PI / (double)m - 1e-3);
  if (fabs (e_peak - peak) > 5e-3)
    fail = 1;

  /* discontinuity: just past the boundary the error flips toward -peak */
  double e_after = scurve_e (m, M_PI / (double)m + 1e-3);
  if (e_after > 0.0 || fabs (e_after + peak) > 5e-3)
    fail = 1;

  /* periodicity e(phi) == e(phi - 2pi/M) sampled across one period */
  double max_per = 0.0;
  for (double phi = -seg / 2 + 1e-2; phi < seg / 2; phi += seg / 16)
    {
      double diff = fabs (scurve_e (m, phi) - scurve_e (m, phi + seg));
      if (diff > max_per)
        max_per = diff;
    }
  if (max_per > 5e-3)
    fail = 1;

  printf ("  M=%d  period=2pi/%d=%.4f  e(0)=%.2e  slope=%.4f  "
          "peak=%.4f(theory %.4f)  per_err=%.2e\n",
          m, m, seg, e0, slope, e_peak, peak, max_per);
  return fail;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;

  printf ("M-PSK carrier discriminator S-curve  e(phi)=Im(P conj ahat)/|P|\n");
  for (int mi = 0; mi < 3; mi++)
    {
      int m = (mi == 0) ? 2 : (mi == 1) ? 4 : 8;
      int f = check_order (m);
      if (f)
        {
          fprintf (stderr, "  M=%d S-curve deviates from theory\n", m);
          fail = 1;
        }
    }

  if (check && fail)
    {
      fprintf (stderr, "M-PSK S-curve FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: decision-directed sawtooth (period 2pi/M, unit slope, "
            "peak sin(pi/M)) for M=2,4,8\n");
  return 0;
}
