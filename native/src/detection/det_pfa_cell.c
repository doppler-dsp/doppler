#include "doppler/detection/detection_core.h"
#include "doppler/util/util_core.h"
#include <math.h>
double
dp_det_pfa_cell (double pfa, double n_cells)
{
  if (!(pfa > 0.0 && pfa < 1.0) || !(n_cells >= 1.0))
    return NAN;
  /* Sidak: n independent cells each at pc miss together with probability
     (1 - pc)^n, so pc = 1 - (1 - pfa)^(1/n) -- through complement_power,
     which does not cancel when pfa is small. */
  return dp_complement_power (pfa, 1.0 / n_cells);
}
