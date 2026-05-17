

# File ddc.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc.h**](ddc_8h.md)

[Go to the documentation of this file](ddc_8h.md)


```C++


#ifndef NATIVE_DDC_H
#define NATIVE_DDC_H

#include "ddc/resamp_dpmfs.h"
#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_ddc dp_ddc_t;

  /* ------------------------------------------------------------------
   * Lifecycle — default coefficients
   * ------------------------------------------------------------------ */

  dp_ddc_t *dp_ddc_create (float norm_freq, size_t num_in, double rate);

  /* ------------------------------------------------------------------
   * Lifecycle — custom resampler
   * ------------------------------------------------------------------ */

  dp_ddc_t *dp_ddc_create_custom (float norm_freq, size_t num_in,
                                  dp_resamp_dpmfs_t *r);

  void dp_ddc_destroy (dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  size_t dp_ddc_max_out (const dp_ddc_t *ddc);

  size_t dp_ddc_nout (const dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Control
   * ------------------------------------------------------------------ */

  void dp_ddc_set_freq (dp_ddc_t *ddc, float norm_freq);

  float dp_ddc_get_freq (const dp_ddc_t *ddc);

  void dp_ddc_reset (dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_ddc_execute (dp_ddc_t *ddc, const float _Complex *in,
                         size_t num_in, float _Complex *out, size_t max_out);

  /* ==================================================================
   * Architecture D2 — real-input DDC
   * ==================================================================
   *
   * @brief Digital Down-Converter for real ADC input (Architecture D2).
   *
   * Chains a real-to-complex modified halfband (embedded fs/4 shift,
   * zero extra multiplications) with a fine NCO at fs/2 and a DPMFS
   * resampler.  Approximately 2× cheaper than Architecture D
   * (NCO then halfband then DPMFS) for any carrier frequency or
   * decimation rate.
   *
   * ### Signal chain
   *
   * ```
   * float in (fs_in)
   *   → dp_hbdecim_r2cf32  (fs/4 shift embedded, decimates by 2)
   *   → fine NCO at fs_in/2  (arbitrary carrier tune)
   *   → DPMFS resample to fs_out
   * CF32 out (fs_out)
   * ```
   *
   * ### norm_freq convention
   *
   * Same as dp_ddc_create: norm_freq = −carrier_offset / fs_in.
   * Negative values shift a positive-offset carrier to DC.
   *
   * ### Rate
   *
   * @p rate = fs_out / fs_in (total, over the full real-input chain).
   * Must satisfy rate < 0.5 (the halfband already decimates by 2).
   *
   * ### Usage
   *
   * ```c
   * #include <dp/ddc.h>
   *
   * // Real 4× decimating D2 DDC; carrier at +0.1·fs
   * dp_ddc_real_t *ddc = dp_ddc_real_create(-0.1f, 4096, 0.25);
   *
   * float _Complex out[dp_ddc_real_max_out(ddc)];
   * size_t n = dp_ddc_real_execute(ddc, in, 4096, out,
   *                                dp_ddc_real_max_out(ddc));
   * dp_ddc_real_destroy(ddc);
   * ```
   */

  typedef struct dp_ddc_real dp_ddc_real_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_ddc_real_t *dp_ddc_real_create (float norm_freq, size_t num_in,
                                     double rate);

  void dp_ddc_real_destroy (dp_ddc_real_t *ddc);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  size_t dp_ddc_real_max_out (const dp_ddc_real_t *ddc);

  size_t dp_ddc_real_nout (const dp_ddc_real_t *ddc);

  /* ------------------------------------------------------------------
   * Control
   * ------------------------------------------------------------------ */

  void dp_ddc_real_set_freq (dp_ddc_real_t *ddc, float norm_freq);

  float dp_ddc_real_get_freq (const dp_ddc_real_t *ddc);

  void dp_ddc_real_reset (dp_ddc_real_t *ddc);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  size_t dp_ddc_real_execute (dp_ddc_real_t *ddc, const float *in,
                              size_t num_in, float _Complex *out,
                              size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_DDC_H */
```
