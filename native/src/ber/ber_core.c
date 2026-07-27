/*
 * ber_core.c — the ber module's own core.
 *
 * Holds only the Gaussian tail shared by the theory curves; each bound module
 * function lives in its own translation unit beside this one (the doppler
 * convention), and the error-rate accumulator is the separate ber_meter
 * component.
 */
#include "ber/ber_core.h"
#include <math.h>

double
ber_qfunc (double x)
{
  return 0.5 * erfc (x / sqrt (2.0));
}
