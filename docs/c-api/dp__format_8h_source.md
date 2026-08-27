

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
    SI8  = DP_FMT ('S', 'B'), 
    SI16 = DP_FMT ('S', 'I'), 
    SI32 = DP_FMT ('S', 'L'), 
    SF32 = DP_FMT ('S', 'F'), 
    SF64 = DP_FMT ('S', 'D'), 
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
      case SI8:
        return sizeof (int8_t);
      case SI16:
        return sizeof (int16_t);
      case SI32:
        return sizeof (int32_t);
      case SF32:
        return sizeof (float);
      case SF64:
        return sizeof (double);
      default:
        return 0u;
      }
  }

  static inline unsigned
  dp_format_components (dp_sample_type_t type)
  {
    if (dp_format_size (type) == 0u)
      return 0u;
    return ((unsigned)type & 0xFFu) == (unsigned char)'S' ? 1u : 2u;
  }

  static inline double
  dp_format_full_scale (dp_sample_type_t type)
  {
    switch (type)
      {
      case CI8:
        return 127.0;
      case CI16:
        return 32767.0;
      case CI32:
        return 2147483647.0;
      case CF32:
      case CF64:
        return 1.0;
      case SI8:
        return 127.0;
      case SI16:
        return 32767.0;
      case SI32:
        return 2147483647.0;
      case SF32:
      case SF64:
        return 1.0;
      default:
        return 0.0;
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


