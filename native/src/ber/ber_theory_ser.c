/*
 * ber_theory_ser.c — ber module-level function. Implementation notes and
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
ber_theory_ser (int m, double esn0)
{
  if (esn0 <= 0.0)
    return (m <= 2) ? 0.5 : 1.0 - 1.0 / (double)m;
  if (m <= 2)
    return ber_qfunc (sqrt (2.0 * esn0));
  if (m == 4)
    return 2.0 * ber_qfunc (sqrt (esn0));
  return 2.0 * ber_qfunc (sqrt (2.0 * esn0) * sin (M_PI / 8.0));
}
