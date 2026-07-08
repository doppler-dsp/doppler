

# File hbdecim\_r2c\_core.h

[**File List**](files.md) **>** [**hbdecim**](dir_3828151286b0ff520a0d701b39db5af1.md) **>** [**hbdecim\_r2c\_core.h**](hbdecim__r2c__core_8h.md)

[Go to the documentation of this file](hbdecim__r2c__core_8h.md)


```C++


#ifndef HBDECIM_R2C_CORE_H
#define HBDECIM_R2C_CORE_H

#include "clib_common.h"
#include "dp_state.h"

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

  /* ── Serializable state (reusable elastic-resume convention) ──────────────
   * Mutable per-stream state only — the even/odd delay rings, their write
   * heads, the pending odd sample, and the output parity.  Coefficients and
   * sizes are config (rebuilt from num_taps on the resumed instance).  Size is
   * derived from even_cap, so a same-num_taps instance round-trips exactly. */

  /* Standard bytes interface; see dp_state.h. */
#define HBDECIM_R2C_STATE_MAGIC DP_FOURCC ('H', 'B', 'R', '2')
#define HBDECIM_R2C_STATE_VERSION 1u

  size_t hbdecim_r2c_state_bytes (const hbdecim_r2c_state_t *r);
  void hbdecim_r2c_get_state (const hbdecim_r2c_state_t *r, void *blob);
  int hbdecim_r2c_set_state (hbdecim_r2c_state_t *r, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_R2C_CORE_H */
```


