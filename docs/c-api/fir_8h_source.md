

# File fir.h

[**File List**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**fir.h**](fir_8h.md)

[Go to the documentation of this file](fir_8h.md)


```C++


#ifndef DP_FIR_H
#define DP_FIR_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_fir dp_fir_t;

  /* ------------------------------------------------------------------
   * Lifecycle — shared by both real-tap and complex-tap filters
   * ------------------------------------------------------------------ */

  dp_fir_t *dp_fir_create (const float _Complex *taps, size_t num_taps);

  dp_fir_t *dp_fir_create_real (const float *taps, size_t num_taps);

  void dp_fir_reset (dp_fir_t *f);

  void dp_fir_destroy (dp_fir_t *f);

  /* ------------------------------------------------------------------
   * Execute — CF32 input (native compute type)
   * ------------------------------------------------------------------ */

  int dp_fir_execute_cf32 (dp_fir_t *f, const float _Complex *in,
                           float _Complex *out, size_t num_samples);

  /* ------------------------------------------------------------------
   * Execute — compact IQ inputs (upcasting hot paths)
   * ------------------------------------------------------------------ */

  int dp_fir_execute_ci8 (dp_fir_t *f, const int8_t *in, float _Complex *out,
                          size_t num_samples);

  int dp_fir_execute_ci16 (dp_fir_t *f, const int16_t *in, float _Complex *out,
                           size_t num_samples);

  int dp_fir_execute_ci32 (dp_fir_t *f, const int32_t *in, float _Complex *out,
                           size_t num_samples);

  /* ------------------------------------------------------------------
   * Execute — real-tap paths (DDC/DUC common case)
   * ------------------------------------------------------------------ */

  int dp_fir_execute_real_cf32 (dp_fir_t *f, const float _Complex *in,
                                float _Complex *out, size_t num_samples);

  int dp_fir_execute_real_ci8 (dp_fir_t *f, const int8_t *in,
                               float _Complex *out, size_t num_samples);

  int dp_fir_execute_real_ci16 (dp_fir_t *f, const int16_t *in,
                                float _Complex *out, size_t num_samples);

  int dp_fir_execute_real_ci32 (dp_fir_t *f, const int32_t *in,
                                float _Complex *out, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* DP_FIR_H */
```
