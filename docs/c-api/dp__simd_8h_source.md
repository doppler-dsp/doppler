

# File dp\_simd.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_simd.h**](dp__simd_8h.md)

[Go to the documentation of this file](dp__simd_8h.md)


```C++

#ifndef DP_SIMD_H
#define DP_SIMD_H

#include <stddef.h>

#include "jm_simd.h"

#define DP_SUMSQ_F32(dst, ptr, n)                                             \
  do                                                                          \
    {                                                                         \
      const float *dp_p_ = (ptr);                                             \
      size_t dp_n_ = (size_t)(n);                                             \
      size_t dp_nv_ = dp_n_ - dp_n_ % (size_t)JM_SIMD_WIDTH_F32;              \
      JM_VEC_F32 dp_acc_ = JM_ZERO_F32 ();                                    \
      for (size_t dp_i_ = 0; dp_i_ < dp_nv_;                                  \
           dp_i_ += (size_t)JM_SIMD_WIDTH_F32)                                \
        {                                                                     \
          JM_VEC_F32 dp_v_ = JM_LOAD_F32 (dp_p_ + dp_i_);                     \
          JM_FMA_F32 (dp_acc_, dp_v_, dp_v_);                                 \
        }                                                                     \
      float dp_s_ = JM_HSUM_F32 (dp_acc_);                                    \
      for (size_t dp_i_ = dp_nv_; dp_i_ < dp_n_; dp_i_++)                     \
        dp_s_ += dp_p_[dp_i_] * dp_p_[dp_i_];                                 \
      (dst) = dp_s_;                                                          \
    }                                                                         \
  while (0)

#endif /* DP_SIMD_H */
```


