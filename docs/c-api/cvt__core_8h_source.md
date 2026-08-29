

# File cvt\_core.h

[**File List**](files.md) **>** [**cvt**](dir_7aebb15fbd538257eeb7884581a8ab59.md) **>** [**cvt\_core.h**](cvt__core_8h.md)

[Go to the documentation of this file](cvt__core_8h.md)


```C++

#ifndef CVT_CORE_H
#define CVT_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Declare module-level functions here. */

  typedef enum
  {
    DP_BITORDER_BIG    = 0, 
    DP_BITORDER_LITTLE = 1  
  } dp_bitorder_t;

  static inline size_t
  cvt_unit_width (size_t done, size_t total)
  {
    const size_t left = total - done;
    return (left >= 8u) ? 8u : left;
  }

  static inline size_t
  cvt_bit_slot (size_t i, size_t width, int bitorder)
  {
    return (bitorder == DP_BITORDER_BIG) ? i : (width - 1u - i);
  }

size_t int_to_bin(uint64_t v, uint32_t n_bits, uint8_t *out, size_t out_len, int bitorder);
size_t hex_to_bin(const char * hex, uint8_t *out, size_t out_len, int bitorder);
uint64_t bin_to_int(const uint8_t *bits, size_t bits_len, int bitorder);
size_t bin_to_hex(const uint8_t *bits, size_t bits_len, uint8_t *out, size_t out_len, int bitorder);
size_t bin_to_nrz(const uint8_t *bits, size_t bits_len, float *out, size_t out_len);
size_t nrz_to_bin(const float *nrz, size_t nrz_len, uint8_t *out, size_t out_len);
#ifdef __cplusplus
}
#endif

#endif /* CVT_CORE_H */
```


