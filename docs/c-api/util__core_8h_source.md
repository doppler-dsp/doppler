

# File util\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md) **>** [**util\_core.h**](util__core_8h.md)

[Go to the documentation of this file](util__core_8h.md)


```C++

#ifndef UTIL_CORE_H
#define UTIL_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum
  {
    DP_BITORDER_BIG    = 0, 
    DP_BITORDER_LITTLE = 1  
  } dp_bitorder_t;

  size_t int_to_bin (uint64_t v, unsigned n_bits, uint8_t *out,
                     size_t out_len, int bitorder);

  int bin_to_int (const uint8_t *bits, size_t n_bits, uint64_t *out,
                  int bitorder);

  size_t hex_to_bin (const char *hex, uint8_t *out, size_t out_len,
                     int bitorder);

  size_t bin_to_hex (const uint8_t *bits, size_t n_bits, char *out,
                     size_t out_len, int bitorder);

  JM_FORCEINLINE float complex
  square_clip (float complex y, float lin)
  {
    float r = fminf (fmaxf (crealf (y), -lin), lin);
    float i = fminf (fmaxf (cimagf (y), -lin), lin);
    return r + i * I;
  }

  JM_FORCEINLINE double
  saturate (double v, double lo, double hi, double nan_to)
  {
    if (v >= lo && v <= hi)
      return v; /* the common case; false for NaN, which falls through */
    if (v < lo)
      return lo;
    if (v > hi)
      return hi;
    return nan_to; /* nothing else can reach here */
  }

  JM_FORCEINLINE double
  ema_step (double state, double x, double alpha)
  {
    /* Loop-invariant, and folded away entirely when alpha is a
       compile-time constant, so the common path pays nothing. */
    if (alpha >= 1.0)
      return x;
    return state + alpha * (x - state);
  }

  JM_FORCEINLINE double
  ema_alpha_decim (double alpha, size_t d)
  {
    if (d <= 1)
      return alpha; /* exact by construction, not by luck */
    if (alpha <= 0.0)
      return 0.0;
    if (alpha >= 1.0)
      return 1.0; /* log1p(-1) is -inf; answer it directly */
    return -expm1 ((double)d * log1p (-alpha));
  }
#ifdef __cplusplus
}
#endif

#endif /* UTIL_CORE_H */
```


