

# File window.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**window.h**](window_8h.md)

[Go to the documentation of this file](window_8h.md)


```C++


#ifndef DP_WINDOW_H
#define DP_WINDOW_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  void dp_kaiser_window (float *w, size_t n, float beta);

  float dp_kaiser_enbw (const float *w, size_t n);

  void dp_magnitude_db_cf32 (const float _Complex *in, size_t n,
                              float *out, float lin_floor,
                              float offset_db);

  void dp_magnitude_db_cf64 (const double _Complex *in, size_t n,
                              float *out, double lin_floor,
                              float offset_db);

#ifdef __cplusplus
}
#endif

#endif /* DP_WINDOW_H */
```
