

# File resamp.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**resamp.h**](resamp_8h.md)

[Go to the documentation of this file](resamp_8h.md)


```C++


#ifndef DP_RESAMP_H
#define DP_RESAMP_H

#include <dp/stream.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_resamp_cf32 dp_resamp_cf32_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_resamp_cf32_t *dp_resamp_cf32_create (size_t num_phases, size_t num_taps,
                                           const float *bank, double rate);

  void dp_resamp_cf32_destroy (dp_resamp_cf32_t *r);

  void dp_resamp_cf32_reset (dp_resamp_cf32_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double dp_resamp_cf32_rate (const dp_resamp_cf32_t *r);

  size_t dp_resamp_cf32_num_phases (const dp_resamp_cf32_t *r);

  size_t dp_resamp_cf32_num_taps (const dp_resamp_cf32_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_resamp_cf32_execute (dp_resamp_cf32_t *r, const float _Complex *in,
                                 size_t num_in, float _Complex *out,
                                 size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_RESAMP_H */
```
