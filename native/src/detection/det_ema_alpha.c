#include "detection/detection_core.h"
#include <math.h>

double
det_ema_alpha (double snr_in_db, double snr_out_db)
{
  double gin  = pow (10.0, snr_in_db / 10.0);
  double gout = pow (10.0, snr_out_db / 10.0);
  if (gout <= gin)
    return 1.0; /* the raw samples already meet the target */
  /* SNR_out = SNR_in * (2 - alpha) / alpha  =>  solve for alpha. */
  return 2.0 * gin / (gin + gout);
}
