

# File resamp\_dpmfs.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**resamp\_dpmfs.h**](resamp__dpmfs_8h.md)

[Go to the documentation of this file](resamp__dpmfs_8h.md)


```C++


#ifndef NATIVE_DDC_RESAMP_DPMFS_H
#define NATIVE_DDC_RESAMP_DPMFS_H

#include <complex.h>
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

  size_t dp_resamp_dpmfs_execute (dp_resamp_dpmfs_t *r,
                                  const float _Complex *in, size_t num_in,
                                  float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_RESAMP_DPMFS_H */
```
