

# File dp\_hash64.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_hash64.h**](dp__hash64_8h.md)

[Go to the documentation of this file](dp__hash64_8h.md)


```C++

#ifndef DP_HASH64_H
#define DP_HASH64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_HASH64_INIT UINT64_C (0xcbf29ce484222325)

#define DP_HASH64_PRIME UINT64_C (0x100000001b3)

  static inline uint64_t
  dp_hash64 (uint64_t h, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; i++)
      {
        h ^= p[i];
        h *= DP_HASH64_PRIME;
      }
    return h;
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_HASH64_H */
```


