

# File hbdecim\_r2c\_core.h

[**File List**](files.md) **>** [**hbdecim**](dir_3828151286b0ff520a0d701b39db5af1.md) **>** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md)

[Go to the documentation of this file](hbdecim__r2c__core_8h.md)


```C++


#ifndef HBDECIM_R2C_CORE_H
#define HBDECIM_R2C_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct hbdecim_r2c_state hbdecim_r2c_state_t;

  hbdecim_r2c_state_t *hbdecim_r2c_create (size_t num_taps, const float *h);

  void hbdecim_r2c_destroy (hbdecim_r2c_state_t *r);

  void hbdecim_r2c_reset (hbdecim_r2c_state_t *r);

  double hbdecim_r2c_get_rate (const hbdecim_r2c_state_t *r);

  size_t hbdecim_r2c_get_num_taps (const hbdecim_r2c_state_t *r);

  size_t hbdecim_r2c_execute (hbdecim_r2c_state_t *r, const float *in,
                              size_t num_in, float _Complex *out,
                              size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_R2C_CORE_H */
```


