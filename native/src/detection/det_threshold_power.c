#include "detection/detection_core.h"
#include <math.h>
double
det_threshold_power (double pfa)
{
  return -log (pfa);
}
