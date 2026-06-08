#include "detection/detection_core.h"
#include <math.h>
double
det_threshold (double pfa)
{
  return sqrt (-2.0 * log (pfa));
}
