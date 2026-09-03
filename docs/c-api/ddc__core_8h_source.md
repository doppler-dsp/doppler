

# File ddc\_core.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the documentation of this file](ddc__core_8h.md)


```C++

#ifndef DDC_CORE_H
#define DDC_CORE_H

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "agc/agc_core.h"
#include "dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct ddc_state
  {
    lo_state_t            *lo; 
    RateConverter_state_t *rc; 
    bool narrow_pulse;
  } ddc_state_t;

ddc_state_t *ddc_create(double norm_freq, double rate);

  ddc_state_t *ddc_create_matched (double norm_freq, double rate, int pulse,
                                   double beta, size_t span, double pulse_sps,
                                   size_t num_phases);

void ddc_destroy(ddc_state_t *state);

void ddc_reset(ddc_state_t *state);

double ddc_get_norm_freq(const ddc_state_t *state);

void ddc_set_norm_freq(ddc_state_t *state, double val);

double ddc_get_rate(const ddc_state_t *state);

size_t ddc_execute(ddc_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

  size_t ddc_execute_ctrl (ddc_state_t *state, const float _Complex *x,
                           size_t x_len, double rate_ctrl, double freq_ctrl,
                           float _Complex *out, size_t max_out);

  size_t ddc_execute_ctrl_push (ddc_state_t *state, float _Complex x,
                                double rate_ctrl, double freq_ctrl,
                                float _Complex *out, size_t max_out);

  size_t ddc_execute_ctrl_push_tap (ddc_state_t *state, float _Complex x,
                                    double rate_ctrl, double freq_ctrl,
                                    float _Complex *out, size_t max_out,
                                    float _Complex *lo_out, int *n_lo);

  size_t ddc_execute_ctrl_push_tap2 (ddc_state_t *state, float _Complex x,
                                     double rate_ctrl, double freq_ctrl,
                                     float _Complex *out, size_t max_out,
                                     float _Complex *lo_out, int *n_lo,
                                     float _Complex *pre_out, int *n_pre);

  double ddc_get_bank_sps (const ddc_state_t *state);

  bool ddc_get_narrow_pulse (const ddc_state_t *state);

bool ddc_get_clipped(const ddc_state_t *state);

  int ddc_set_telemetry (ddc_state_t *state, dp_tlm_t *tlm, const char *prefix,
                         uint32_t decim);

size_t ddc_execute_max_out(ddc_state_t *state, size_t x_len);

  /* ── Serializable state — complex DDC (LO + RateConverter) ─────────────────
   * Standard bytes interface (see dp_state.h):
   * `[dp_state_hdr_t][ddc_extra_t][lo][rc]`.  Like ddcr without the real-input
   * halfband front end; `rate` is the layout key. */

  typedef struct
  {
    double rate; 
  } ddc_extra_t;

#define DDC_STATE_MAGIC DP_FOURCC ('D', 'D', 'C', '_')
#define DDC_STATE_VERSION 1u

  size_t ddc_state_bytes (const ddc_state_t *state);
  void ddc_get_state (const ddc_state_t *state, void *blob);
  int ddc_set_state (ddc_state_t *state, const void *blob);
  size_t ddc_run (ddc_state_t *state, const void *state_in, void *state_out,
                  const float _Complex *in, size_t n_in, float _Complex *out,
                  size_t max_out);

size_t ddc_execute_ctrl_max_out(ddc_state_t *state, size_t x_len);
size_t ddc_execute_ctrl_push_max_out(ddc_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
```


