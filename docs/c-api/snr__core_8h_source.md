

# File snr\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**snr**](dir_02c47206b4cbe462773a89d72d8a4c36.md) **>** [**snr\_core.h**](snr__core_8h.md)

[Go to the documentation of this file](snr__core_8h.md)


```C++

#ifndef DP_SNR_CORE_H
#define DP_SNR_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_complex.h"

#ifdef __cplusplus
extern "C"
{
#endif

  double dp_snr_data_aided_db (const float _Complex *soft, size_t soft_len,
                            const uint8_t *sign_bits, size_t sign_bits_len);

  double dp_snr_m2m4_db (const float _Complex *x, size_t x_len);

  void dp_snr_data_aided_db_series (const float _Complex *soft, size_t soft_len,
                                 const uint8_t *sign_bits,
                                 size_t sign_bits_len, size_t window,
                                 double *out);

  void dp_snr_m2m4_db_series (const float _Complex *x, size_t x_len,
                           size_t window, double *out);

#ifdef __cplusplus
}
#endif
#endif /* SNR_CORE_H */
```


