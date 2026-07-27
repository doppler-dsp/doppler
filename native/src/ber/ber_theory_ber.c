/*
 * ber_theory_ber.c — ber module-level function. Implementation notes and
 * the measured consequences of getting each one wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "ber/ber_core.h"
#include "mpsk/mpsk_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double
ber_theory_ber (int m, double esn0)
{
  int bps = mpsk_bps (m);
  if (bps < 1)
    bps = 1;
  if (m <= 4)
    return ber_qfunc (sqrt (2.0 * esn0 / (double)bps));
  return ber_theory_ser (m, esn0) / (double)bps;
}
