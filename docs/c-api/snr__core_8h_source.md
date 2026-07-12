

# File snr\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**snr**](dir_a0dc77cb6789ae5cf19b2d0651b00ce2.md) **>** [**snr\_core.h**](snr__core_8h.md)

[Go to the documentation of this file](snr__core_8h.md)


```C++

#ifndef SNR_CORE_H
#define SNR_CORE_H

#include "clib_common.h"
#include <complex.h>

#ifdef __cplusplus
extern "C"
{
#endif

  double snr_data_aided_db (const float complex *soft, size_t soft_len,
                            const uint8_t *sign_bits, size_t sign_bits_len);

  double snr_m2m4_db (const float complex *x, size_t x_len);

  void snr_data_aided_db_series (const float complex *soft, size_t soft_len,
                                 const uint8_t *sign_bits,
                                 size_t sign_bits_len, size_t window,
                                 double *out);

  void snr_m2m4_db_series (const float complex *x, size_t x_len,
                           size_t window, double *out);

#ifdef __cplusplus
}
#endif
#endif /* SNR_CORE_H */
```


