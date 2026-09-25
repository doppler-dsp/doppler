#include "detection/detection_core.h"
#include <math.h>
double
det_snr_to_cn0 (double snr, double fs)
{
  return 20.0 * log10 (snr) + 10.0 * log10 (fs);
}
