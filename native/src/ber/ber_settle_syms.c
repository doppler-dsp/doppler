/*
 * ber_settle_syms.c — ber module-level function. Implementation notes and
 * the measured consequences of getting each one wrong live on the
 * declaration in ber/ber_core.h.
 */
#include "doppler/ber/ber_core.h"
#include "doppler/mpsk/mpsk_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

size_t
dp_ber_settle_syms (double bn_timing, double bn_carrier)
{
  double s = 0.0;
  if (bn_timing > 0.0)
    s += 5.0 / bn_timing;
  if (bn_carrier > 0.0)
    s += 5.0 / bn_carrier;
  return (size_t)(2.0 * s);
}
