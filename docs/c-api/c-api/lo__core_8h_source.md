

# File lo\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the documentation of this file](lo__core_8h.md)


```C++

#ifndef LO_CORE_H
#define LO_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint32_t phase;
    uint32_t phase_inc;
    float norm_freq;
  } lo_state_t;

  lo_state_t *lo_create (float norm_freq);

  void lo_destroy (lo_state_t *lo);

  void lo_reset (lo_state_t *lo);

  void lo_set_freq (lo_state_t *lo, float norm_freq);

  float lo_get_freq (const lo_state_t *lo);
  uint32_t lo_get_phase (const lo_state_t *lo);
  uint32_t lo_get_phase_inc (const lo_state_t *lo);

  void lo_set_phase (lo_state_t *lo, uint32_t phase);

  void lo_execute_cf32 (lo_state_t *lo, float _Complex *out, size_t n);

  void lo_execute_cf32_ctrl (lo_state_t *lo, const float *ctrl,
                             float _Complex *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
```
