

# File dp\_crc16.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_crc16.h**](dp__crc16_8h.md)

[Go to the documentation of this file](dp__crc16_8h.md)


```C++

#ifndef DP_CRC16_H
#define DP_CRC16_H

#include "doppler/dp_crc.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t
dp_crc16_ccitt (const uint8_t *bits, size_t n)
{
  const dp_crc_t model = DP_CRC_16_IBM_3740;
  return (uint16_t)dp_crc_bits (&model, bits, n);
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC16_H */
```


