/*
 * ber_esn0_db_for_ser.c — ber module-level function. Implementation notes and
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
ber_esn0_db_for_ser (int m, double ser)
{
  double lo = -10.0, hi = 40.0;
  if (!(ser > 0.0) || ser >= ber_theory_ser (m, pow (10.0, lo / 10.0)))
    return lo;
  if (ser <= ber_theory_ser (m, pow (10.0, hi / 10.0)))
    return hi;
  for (int i = 0; i < 200; i++)
    {
      double mid = 0.5 * (lo + hi);
      if (ber_theory_ser (m, pow (10.0, mid / 10.0)) > ser)
        lo = mid;
      else
        hi = mid;
    }
  return 0.5 * (lo + hi);
}
