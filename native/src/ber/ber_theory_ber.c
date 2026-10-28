/*
 * ber_theory_ber.c — ber module-level function. Implementation notes and
 * the measured consequences of getting each one wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "doppler/ber/ber_core.h"
#include "doppler/mpsk/mpsk_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double
dp_ber_theory_ber (int m, double esn0)
{
  int bps = mpsk_bps (m);
  /* No closed form for this M: NaN, never another M's rate (#1913). */
  if (bps == 0)
    return NAN;
  /* No signal: a coin flip per bit, for every M. 0.5 is CHOSEN as the
     chance rate, not read off the formulas below: BPSK/QPSK's Q(0) does
     reach it at esn0 = 0+, but 8-PSK's SER/log2(M) approximation reaches
     2Q(0)/3 = 1/3 there, so its curve steps from 0.5 to 0.333 just above
     0 -- the approximation is a high-Es/N0 one and says nothing honest at
     no signal. It is the same choice ber_theory_ser() makes with its own
     chance rate. A negative ratio would otherwise reach sqrt() and return
     NaN (#2651). */
  if (esn0 <= 0.0)
    return 0.5;
  if (m <= 4)
    return dp_ber_qfunc (sqrt (2.0 * esn0 / (double)bps));
  return dp_ber_theory_ser (m, esn0) / (double)bps;
}
