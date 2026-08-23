

# File dp\_format.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_format.h**](dp__format_8h.md)

[Go to the documentation of this file](dp__format_8h.md)


```C++

#ifndef DP_FORMAT_H
#define DP_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_FMT(mode, type)                                                    \
  ((uint16_t)((uint16_t)(unsigned char)(mode)                                 \
              | ((uint16_t)(unsigned char)(type) << 8)))

  typedef enum
  {
    CI8  = DP_FMT ('C', 'B'), 
    CI16 = DP_FMT ('C', 'I'), 
    CI32 = DP_FMT ('C', 'L'), 
    CF32 = DP_FMT ('C', 'F'), 
    CF64 = DP_FMT ('C', 'D'), 
  } dp_sample_type_t;

  static inline size_t
  dp_format_size (dp_sample_type_t type)
  {
    switch (type)
      {
      case CI8:
        return 2u * sizeof (int8_t);
      case CI16:
        return 2u * sizeof (int16_t);
      case CI32:
        return 2u * sizeof (int32_t);
      case CF32:
        return 2u * sizeof (float);
      case CF64:
        return 2u * sizeof (double);
      default:
        return 0u;
      }
  }

  static inline int
  dp_format_is_valid (dp_sample_type_t type)
  {
    return dp_format_size (type) != 0u;
  }

  static inline void
  dp_format_chars (dp_sample_type_t type, char out[2])
  {
    out[0] = (char)((uint16_t)type & 0xFFu);
    out[1] = (char)(((uint16_t)type >> 8) & 0xFFu);
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_FORMAT_H */
```


