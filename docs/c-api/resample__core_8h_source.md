

# File resample\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resample**](dir_430486ea22038fad478027f2dc6550c6.md) **>** [**resample\_core.h**](resample__core_8h.md)

[Go to the documentation of this file](resample__core_8h.md)


```C++

#ifndef RESAMPLE_CORE_H
#define RESAMPLE_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

  double kaiser_beta (double atten);

  int kaiser_num_taps (int num_phases, double atten, double pb, double sb);

  void ciccompmf(double *h, uint32_t N, uint32_t R, uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* RESAMPLE_CORE_H */
```


