

# File ddc\_core.h

[**File List**](files.md) **>** [**ddc**](dir_4a67ebc391a3fd8e8259ec0993c7169b.md) **>** [**ddc\_core.h**](ddc__core_8h.md)

[Go to the documentation of this file](ddc__core_8h.md)


```C++

#ifndef DP_DDC_CORE_H
#define DP_DDC_CORE_H

#include "doppler/dp_complex.h"
#include <stdbool.h>
#include <stddef.h>
#include "doppler/lo/lo_core.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/agc/agc_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct ddc_state
  {
    dp_lo_state_t            *lo; 
    dp_RateConverter_state_t *rc; 
    bool narrow_pulse;
  } dp_ddc_state_t;

dp_ddc_state_t *dp_ddc_create(double norm_freq, double rate);

  dp_ddc_state_t *ddc_create_matched (double norm_freq, double rate, int pulse,
                                   double beta, size_t span, double pulse_sps,
                                   size_t num_phases);

void dp_ddc_destroy(dp_ddc_state_t *state);

void dp_ddc_reset(dp_ddc_state_t *state);

double dp_ddc_get_norm_freq(const dp_ddc_state_t *state);

void dp_ddc_set_norm_freq(dp_ddc_state_t *state, double val);

double dp_ddc_get_rate(const dp_ddc_state_t *state);

size_t dp_ddc_execute(dp_ddc_state_t *state, const float _Complex *x, size_t x_len, float _Complex *out, size_t max_out);

  size_t dp_ddc_execute_ctrl (dp_ddc_state_t *state, const float _Complex *x,
                           size_t x_len, double rate_ctrl, double freq_ctrl,
                           float _Complex *out, size_t max_out);

  size_t dp_ddc_execute_ctrl_push (dp_ddc_state_t *state, float _Complex x,
                                double rate_ctrl, double freq_ctrl,
                                float _Complex *out, size_t max_out);

  size_t ddc_execute_ctrl_push_tap (dp_ddc_state_t *state, float _Complex x,
                                    double rate_ctrl, double freq_ctrl,
                                    float _Complex *out, size_t max_out,
                                    float _Complex *lo_out, int *n_lo);

  size_t ddc_execute_ctrl_push_tap2 (dp_ddc_state_t *state, float _Complex x,
                                     double rate_ctrl, double freq_ctrl,
                                     float _Complex *out, size_t max_out,
                                     float _Complex *lo_out, int *n_lo,
                                     float _Complex *pre_out, int *n_pre);

  double ddc_get_bank_sps (const dp_ddc_state_t *state);

  bool dp_ddc_get_narrow_pulse (const dp_ddc_state_t *state);

bool dp_ddc_get_clipped(const dp_ddc_state_t *state);

  int ddc_set_telemetry (dp_ddc_state_t *state, dp_tlm_t *tlm, const char *prefix,
                         uint32_t decim);

size_t dp_ddc_execute_max_out(dp_ddc_state_t *state, size_t x_len);

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

  size_t dp_ddc_state_bytes (const dp_ddc_state_t *state);
  void dp_ddc_get_state (const dp_ddc_state_t *state, void *blob);
  int dp_ddc_set_state (dp_ddc_state_t *state, const void *blob);
  size_t dp_ddc_run (dp_ddc_state_t *state, const void *state_in, void *state_out,
                  const float _Complex *in, size_t n_in, float _Complex *out,
                  size_t max_out);

size_t dp_ddc_execute_ctrl_max_out(dp_ddc_state_t *state, size_t x_len);
size_t dp_ddc_execute_ctrl_push_max_out(dp_ddc_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
```


