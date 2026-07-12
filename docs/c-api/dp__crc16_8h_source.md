

# File dp\_crc16.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_crc16.h**](dp__crc16_8h.md)

[Go to the documentation of this file](dp__crc16_8h.md)


```C++

#ifndef DP_CRC16_H
#define DP_CRC16_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t
dp_crc16_ccitt (const uint8_t *bits, size_t n)
{
  uint16_t crc = 0xFFFFu;
  for (size_t i = 0; i < n; i++)
    {
      crc ^= (uint16_t)((bits[i] & 1u) << 15);
      crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                            : (uint16_t)(crc << 1);
    }
  return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* DP_CRC16_H */
```


