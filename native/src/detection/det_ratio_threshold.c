#include "doppler/clib_common.h"
#include "doppler/detection/detection_core.h"
#include <math.h>

double
dp_det_ratio_threshold (double pfa, int n)
{
  /* R > r is X/Y > (1 + r)/(1 - r), X = sum(I^2) and Y = sum(Q^2)
     chi-square on n degrees each under noise, so r = (g - 1)/(g + 1) at
     F(n, n)'s upper point g. dp_det_threshold_f() refuses what is out of
     range with a NaN, which passes through. */
  const double g = dp_det_threshold_f (pfa, n);
  return (g - 1.0) / (g + 1.0);
}
