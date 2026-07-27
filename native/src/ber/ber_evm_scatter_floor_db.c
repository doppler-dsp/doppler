/*
 * ber_evm_scatter_floor_db.c — ber module-level function. Implementation notes
 * and the measured consequences of getting each one wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "ber/ber_core.h"
#include "mpsk/mpsk_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double
ber_evm_scatter_floor_db (int m)
{
  double t;
  if (m < 2)
    m = 2;
  t = M_PI / (double)m;
  return 10.0 * log10 (2.0 - 2.0 * sin (t) / t);
}
