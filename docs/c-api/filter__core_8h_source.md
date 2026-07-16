

# File filter\_core.h

[**File List**](files.md) **>** [**filter**](dir_8178efb5c7670e7552eaa4222282ba05.md) **>** [**filter\_core.h**](filter__core_8h.md)

[Go to the documentation of this file](filter__core_8h.md)


```C++

#ifndef FILTER_CORE_H
#define FILTER_CORE_H

#include "clib_common.h"
#include "resample/resample_core.h" /* kaiser_num_taps — used by design_lowpass's
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


