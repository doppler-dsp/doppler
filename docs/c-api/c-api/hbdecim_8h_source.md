

# File hbdecim.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**hbdecim.h**](hbdecim_8h.md)

[Go to the documentation of this file](hbdecim_8h.md)


```C++
/* Minimal halfband header for DDC: only the r2cf32 API is needed.
 * Redirects dp_hbdecim_r2cf32_* to the lifted hbdecim_r2c_core. */
#ifndef NATIVE_DDC_HBDECIM_H
#define NATIVE_DDC_HBDECIM_H

#include "hbdecim/hbdecim_r2c_core.h"

typedef hbdecim_r2c_state_t dp_hbdecim_r2cf32_t;

#define dp_hbdecim_r2cf32_create hbdecim_r2c_create
#define dp_hbdecim_r2cf32_destroy hbdecim_r2c_destroy
#define dp_hbdecim_r2cf32_reset hbdecim_r2c_reset
#define dp_hbdecim_r2cf32_rate hbdecim_r2c_get_rate
#define dp_hbdecim_r2cf32_num_taps hbdecim_r2c_get_num_taps
#define dp_hbdecim_r2cf32_execute hbdecim_r2c_execute

#endif /* NATIVE_DDC_HBDECIM_H */
```
