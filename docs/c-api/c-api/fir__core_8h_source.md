

# File fir\_core.h

[**File List**](files.md) **>** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the documentation of this file](fir__core_8h.md)


```C++


#ifndef FIR_CORE_H
#define FIR_CORE_H

#include "clib_common.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct fir_state fir_state_t;

  /* ── Lifecycle ──────────────────────────────────────────────────────── */

  fir_state_t *fir_create (const float _Complex *taps, size_t num_taps);

  fir_state_t *fir_create_real (const float *taps, size_t num_taps);

  void fir_reset (fir_state_t *f);

  void fir_destroy (fir_state_t *f);

  size_t fir_num_taps (const fir_state_t *f);

  int fir_is_real (const fir_state_t *f);

  /* ── Execute — complex taps ─────────────────────────────────────────── */

  int fir_execute_cf32 (fir_state_t *f, const float _Complex *in,
                        float _Complex *out, size_t num_samples);

  int fir_execute_ci8 (fir_state_t *f, const int8_t *in, float _Complex *out,
                       size_t num_samples);

  int fir_execute_ci16 (fir_state_t *f, const int16_t *in, float _Complex *out,
                        size_t num_samples);

  int fir_execute_ci32 (fir_state_t *f, const int32_t *in, float _Complex *out,
                        size_t num_samples);

  /* ── Execute — real taps ─────────────────────────────────────────────── */

  int fir_execute_real_cf32 (fir_state_t *f, const float _Complex *in,
                             float _Complex *out, size_t num_samples);

  int fir_execute_real_ci8 (fir_state_t *f, const int8_t *in,
                            float _Complex *out, size_t num_samples);

  int fir_execute_real_ci16 (fir_state_t *f, const int16_t *in,
                             float _Complex *out, size_t num_samples);

  int fir_execute_real_ci32 (fir_state_t *f, const int32_t *in,
                             float _Complex *out, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* FIR_CORE_H */
```
