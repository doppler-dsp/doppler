#include "doppler/detection/detection_core.h"
#include <math.h>
double
dp_det_cn0_to_snr (double cn0_dbhz, double fs)
{
  /* Power SNR per sample is (C/N0)/fs; the model's snr is its root. */
  return sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
}
