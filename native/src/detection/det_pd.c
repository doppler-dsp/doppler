#include "doppler/detection/detection_core.h"
#include <math.h>
double
dp_det_pd (double snr, int dwell, double threshold)
{
  return dp_marcum_q (1, sqrt (2.0 * dwell) * snr, threshold);
}
