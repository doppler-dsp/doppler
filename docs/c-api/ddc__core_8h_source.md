

# File ddc\_core.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the documentation of this file](ddc__core_8h.md)


```C++

#ifndef DDC_CORE_H
#define DDC_CORE_H

#include <complex.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* ================================================================== */
  /* Ddc — complex-input DDC                                            */
  /* ================================================================== */

  typedef struct ddc_state ddc_state_t;

ddc_state_t *ddc_create(double norm_freq, double rate);

void ddc_destroy(ddc_state_t *state);

void ddc_reset(ddc_state_t *state);

double ddc_get_norm_freq(const ddc_state_t *state);

void ddc_set_norm_freq(ddc_state_t *state, double val);

double ddc_get_rate(const ddc_state_t *state);

size_t ddc_execute(ddc_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

  /* ================================================================== */
  /* DdcR — real-input DDC (Architecture D2)                           */
  /* ================================================================== */

  typedef struct ddcr_state ddcr_state_t;

  ddcr_state_t *ddcr_create (double norm_freq, double rate);

  void ddcr_destroy (ddcr_state_t *s);

  void ddcr_reset (ddcr_state_t *s);

  double ddcr_get_norm_freq (const ddcr_state_t *s);

  void ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq);

  double ddcr_get_rate (const ddcr_state_t *s);

  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

size_t ddc_execute_max_out(ddc_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
```


