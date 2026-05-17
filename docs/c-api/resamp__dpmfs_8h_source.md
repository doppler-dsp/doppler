

# File resamp\_dpmfs.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**resamp\_dpmfs.h**](resamp__dpmfs_8h.md)

[Go to the documentation of this file](resamp__dpmfs_8h.md)


```C++


#ifndef DP_RESAMP_DPMFS_H
#define DP_RESAMP_DPMFS_H

#include <dp/stream.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_resamp_dpmfs dp_resamp_dpmfs_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_resamp_dpmfs_t *dp_resamp_dpmfs_create (size_t M, size_t N,
                                             const float *c0, const float *c1,
                                             double rate);

  void dp_resamp_dpmfs_destroy (dp_resamp_dpmfs_t *r);

  void dp_resamp_dpmfs_reset (dp_resamp_dpmfs_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double dp_resamp_dpmfs_rate (const dp_resamp_dpmfs_t *r);

  size_t dp_resamp_dpmfs_num_taps (const dp_resamp_dpmfs_t *r);

  size_t dp_resamp_dpmfs_poly_order (const dp_resamp_dpmfs_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_resamp_dpmfs_execute (dp_resamp_dpmfs_t *r, const float _Complex *in,
                                  size_t num_in, float _Complex *out,
                                  size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_RESAMP_DPMFS_H */
```
