

# File resample\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**resample**](dir_4efb1181c8663bd7a6953f73a4eb8dc9.md) **>** [**resample\_core.h**](resample__core_8h.md)

[Go to the documentation of this file](resample__core_8h.md)


```C++

#ifndef RESAMPLE_CORE_H
#define RESAMPLE_CORE_H

#include "doppler/clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

double kaiser_beta(double atten);

int kaiser_num_taps(int num_phases, double atten, double pb, double sb);

void ciccompmf(double *out, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
```


