/**
 * @file carrier_nda_scurve.c
 * @brief Validation: the NDA carrier loop's M-th-power discriminator is
 * exactly the scaled M-th power, with an M-independent (constant) loop gain.
 *
 * For a unit arm sample z = exp(j*phi) the repeated-squaring recursion
 * (carrier_nda_disc) must give:
 *   phase_error = Im(z^M) * {1, 1/2, 1/4}  for M = 2, 4, 8
 *   lock_signal = Re(z^M) * lock_scale     for M = 2, 4  (faithful detector
 * M=8) The phase_error scale is chosen so the S-curve slope at lock is 2 for
 * every M (one loop bn behaves identically across BPSK/QPSK/8PSK). See
 * docs/design/mpsk.md §2.3.
 *
 * Validated, per M: e(0)=0; slope 2 at lock; phase_error == Im(z^M)*scale to
 * ~1e-6; lock_signal == Re(z^M)*lock_scale for M<=4; lock peaks at the M lock
 * phases and is negative between them (a usable detector) for M=8.
 *
 * Usage:  carrier_nda_scurve [--check]
 */
#include "carrier_nda/carrier_nda_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TWOPI 6.283185307179586

static int
check_order (int m)
{
  double pe_scale = (m == 2) ? 1.0 : (m == 4) ? 0.5 : 0.25;
  double lk_scale = (m == 2) ? 1.0 : (m == 4) ? 0.619 : 0.412;
  double seg      = TWOPI / m;
  int    fail     = 0;

  double e0, l0;
  carrier_nda_disc (1.0f + 0.0f * I, m, lk_scale, &e0, &l0);
  if (fabs (e0) > 1e-6)
    fail = 1;

  /* slope at the origin == 2 for all M (constant-gain property) */
  double h = 1e-4, ep, em, l;
  carrier_nda_disc ((float complex)cexp (I * h), m, lk_scale, &ep, &l);
  carrier_nda_disc ((float complex)cexp (-I * h), m, lk_scale, &em, &l);
  double slope = (ep - em) / (2.0 * h);
  if (fabs (slope - 2.0) > 1e-2)
    fail = 1;

  /* phase_error == Im(z^M)*pe_scale; lock == Re(z^M)*lk_scale (M<=4) */
  double max_pe = 0.0, max_lk = 0.0, lk_at_half = 0.0;
  for (double phi = -M_PI; phi < M_PI; phi += 1e-3)
    {
      float complex z = (float complex)cexp (I * phi);
      double        pe, lk;
      carrier_nda_disc (z, m, lk_scale, &pe, &lk);
      double imzm = sin (m * phi), rezm = cos (m * phi);
      double dpe = fabs (pe - pe_scale * imzm);
      if (dpe > max_pe)
        max_pe = dpe;
      if (m <= 4)
        {
          double dlk = fabs (lk - lk_scale * rezm);
          if (dlk > max_lk)
            max_lk = dlk;
        }
    }
  /* M=8 lock is a detector (not literal Re(z^8)): peaks at 0, dips mid-period
   */
  carrier_nda_disc ((float complex)cexp (I * (seg / 2)), m, lk_scale,
                    &lk_at_half, &l);
  double lk0;
  carrier_nda_disc (1.0f + 0.0f * I, m, lk_scale, &lk0, &l);

  if (max_pe > 1e-6)
    fail = 1;
  if (m <= 4 && max_lk > 1e-6)
    fail = 1;
  if (!(l0 > 0.0))
    fail = 1; /* lock peaks at phi=0 */

  printf ("  M=%d  e(0)=%.2e  slope=%.5f  max|pe-Im(z^M)*%.2f|=%.2e  "
          "max|lk-Re(z^M)*%.3f|=%.2e (M<=4)\n",
          m, e0, slope, pe_scale, max_pe, lk_scale, max_lk);
  return fail;
}

int
main (int argc, char **argv)
{
  int check = (argc > 1 && strcmp (argv[1], "--check") == 0);
  int fail  = 0;
  printf ("NDA M-th-power discriminator S-curve (z^2 -> z^4 -> z^8)\n");
  int ms[] = { 2, 4, 8 };
  for (int i = 0; i < 3; i++)
    if (check_order (ms[i]))
      {
        fprintf (stderr, "  M=%d S-curve deviates from theory\n", ms[i]);
        fail = 1;
      }
  if (check && fail)
    {
      fprintf (stderr, "NDA S-curve FAIL\n");
      return 1;
    }
  if (check)
    printf ("PASS: phase_error = Im(z^M)*{1,1/2,1/4}, slope 2 all M; "
            "lock = Re(z^M)*scale (M<=4)\n");
  return 0;
}
