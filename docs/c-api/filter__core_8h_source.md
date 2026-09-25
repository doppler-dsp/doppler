

# File filter\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**filter**](dir_820039e5f1fe5ce84fe724a5385272f3.md) **>** [**filter\_core.h**](filter__core_8h.md)

[Go to the documentation of this file](filter__core_8h.md)


```C++

#ifndef FILTER_CORE_H
#define FILTER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/resample/resample_core.h" /* kaiser_num_taps — used by design_lowpass's
                                        generated out_size allocation expression */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Declare module-level functions here. */

void design_lowpass(double fpass, double fstop, double atten_db, float *out);
#ifdef __cplusplus
}
#endif

#endif /* FILTER_CORE_H */
```


