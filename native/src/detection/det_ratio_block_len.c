#include "doppler/clib_common.h"
#include "doppler/detection/detection_core.h"
#include <math.h>

int
dp_det_ratio_block_len (double gamma, double phase_var, double pfa, double pd,
                        int max_n)
{
  if (!(isfinite (gamma) && gamma > 0.0)
      || !(isfinite (phase_var) && phase_var >= 0.0)
      || !(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0) || max_n < 2)
    return 0;
  /* The probability rises with n (the threshold falls toward 0 and the
     block's spread with it): double to a length that reaches pd, then
     bisect for the least. */
  int hi = 2;
  while (dp_det_ratio_pd (hi, pfa, gamma, phase_var) < pd)
    {
      if (hi >= max_n)
        return 0;
      hi = hi > max_n / 2 ? max_n : 2 * hi;
    }
  int lo = hi / 2 < 2 ? 1 : hi / 2; /* lo fails (or is below 2) */
  while (hi - lo > 1)
    {
      const int mid = lo + (hi - lo) / 2;
      if (dp_det_ratio_pd (mid, pfa, gamma, phase_var) >= pd)
        hi = mid;
      else
        lo = mid;
    }
  return hi;
}
