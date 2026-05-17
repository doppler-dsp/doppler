

# File spectral\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**spectral**](dir_2aadf81c4f49e887d76ad198d657298d.md) **>** [**spectral\_core.h**](spectral__core_8h.md)

[Go to the documentation of this file](spectral__core_8h.md)


```C++

#ifndef SPECTRAL_CORE_H
#define SPECTRAL_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

float kaiser_enbw(const float *w, size_t w_len);
void  kaiser_window(float *w, size_t w_len, float beta);

void hann_window(float *w, size_t w_len);

void magnitude_db_cf32(const float _Complex *in, size_t n,
                       float *out, float lin_floor, float offset_db);

void magnitude_db_cf64(const double _Complex *in, size_t n,
                       float *out, double lin_floor, float offset_db);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRAL_CORE_H */
```
