/*
 * kaiser_beta.c — resample module-level function.
 */
#include "resample/resample_core.h"
#include <math.h>

double
kaiser_beta(double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}
