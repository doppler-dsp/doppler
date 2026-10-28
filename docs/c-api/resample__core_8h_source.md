

# File resample\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**resample**](dir_4efb1181c8663bd7a6953f73a4eb8dc9.md) **>** [**resample\_core.h**](resample__core_8h.md)

[Go to the documentation of this file](resample__core_8h.md)


```C++

#ifndef DP_RESAMPLE_CORE_H
#define DP_RESAMPLE_CORE_H

#include "doppler/clib_common.h"
#include <math.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

static inline double
dp_kaiser_beta_eval (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

static inline size_t
dp_kaiser_num_taps_eval (size_t num_phases, double atten, double pb,
                         double sb)
{
  const double pi      = 3.14159265358979323846;
  double       pb_ph   = pb / (double)num_phases;
  double       sb_ph   = sb / (double)num_phases;
  double       tw      = sb_ph - pb_ph;
  size_t       proto   = (size_t)(1.0 + (atten - 8.0) / (2.285 * (2.0 * pi * tw)));
  size_t       halflen = proto / 2;
  size_t       htaps   = 2 * halflen + 1;
  return htaps / num_phases + 1;
}

double dp_kaiser_beta(double atten);

int dp_kaiser_num_taps(int num_phases, double atten, double pb, double sb);

void dp_ciccompmf(double *out, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
```


