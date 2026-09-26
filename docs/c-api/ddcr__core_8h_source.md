

# File ddcr\_core.h

[**File List**](files.md) **>** [**ddcr**](dir_18bb1adfae8df578c1da0090bbf10ccf.md) **>** [**ddcr\_core.h**](ddcr__core_8h.md)

[Go to the documentation of this file](ddcr__core_8h.md)


```C++

#ifndef DP_DDCR_CORE_H
#define DP_DDCR_CORE_H

#include "doppler/dp_complex.h"
#include <stdbool.h>
#include <stddef.h>
#include "doppler/lo/lo_core.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/hbdecim/hbdecim_r2c_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/fir/fir_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/agc/agc_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct ddcr_state
  {
    hbdecim_r2c_state_t   *r2c;  
    dp_lo_state_t            *lo;   
    dp_RateConverter_state_t *rc;   
    double                 rate; 
    bool narrow_pulse;
  } dp_ddcr_state_t;

  dp_ddcr_state_t *dp_ddcr_create (double norm_freq, double rate);

  dp_ddcr_state_t *ddcr_create_matched (double norm_freq, double rate, int pulse,
                                     double beta, size_t span,
                                     double pulse_sps, size_t num_phases);

  void dp_ddcr_destroy (dp_ddcr_state_t *s);

  void dp_ddcr_reset (dp_ddcr_state_t *s);

  /* ── Serializable state — the elastic / pure-transducer face ───────────────
   *
   * Composes the leaf serializers of the whole chain (hbdecim_r2c -> LO ->
   * RateConverter) into one flat POD, so a fresh DDCR built from the same
   * (norm_freq, rate) descriptor resumes a stream bit-exactly on any
   * thread/process/pod.  Standard bytes interface (see dp_state.h): the blob is
   * `[dp_state_hdr_t][ddcr_extra_t][r2c][lo][rc]`, each child a self-contained
   * sub-blob with its own envelope.  `rate` is the layout key. */

  typedef struct
  {
    double rate; 
  } ddcr_extra_t;

#define DDCR_STATE_MAGIC DP_FOURCC ('D', 'D', 'C', 'R')
#define DDCR_STATE_VERSION 1u

  size_t dp_ddcr_state_bytes (const dp_ddcr_state_t *s);
  void dp_ddcr_get_state (const dp_ddcr_state_t *s, void *blob);
  int dp_ddcr_set_state (dp_ddcr_state_t *s, const void *blob);

  size_t dp_ddcr_run (dp_ddcr_state_t *s, const void *state_in, void *state_out,
                   const float *in, size_t n_in, float _Complex *out,
                   size_t max_out);

  double dp_ddcr_get_norm_freq (const dp_ddcr_state_t *s);

  void dp_ddcr_set_norm_freq (dp_ddcr_state_t *s, double norm_freq);

  double dp_ddcr_get_rate (const dp_ddcr_state_t *s);

  size_t dp_ddcr_execute (dp_ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

  size_t dp_ddcr_execute_max_out (dp_ddcr_state_t *s);
  size_t dp_ddcr_execute_ctrl_max_out (dp_ddcr_state_t *s);
  size_t dp_ddcr_execute_ctrl_push_max_out (dp_ddcr_state_t *s);

  size_t dp_ddcr_execute_ctrl (dp_ddcr_state_t *s, const float *x, size_t n_in,
                            double rate_ctrl, double freq_ctrl,
                            float _Complex *out, size_t max_out);

  size_t dp_ddcr_execute_ctrl_push (dp_ddcr_state_t *s, float x, double rate_ctrl,
                                 double freq_ctrl, float _Complex *out,
                                 size_t max_out);

  size_t ddcr_execute_ctrl_push_tap (dp_ddcr_state_t *s, float x,
                                     double rate_ctrl, double freq_ctrl,
                                     float _Complex *out, size_t max_out,
                                     float _Complex *lo_out, int *n_lo);

  size_t ddcr_execute_ctrl_push_tap2 (dp_ddcr_state_t *s, float x,
                                      double rate_ctrl, double freq_ctrl,
                                      float _Complex *out, size_t max_out,
                                      float _Complex *lo_out, int *n_lo,
                                      float _Complex *pre_out, int *n_pre);

  double ddcr_get_bank_sps (const dp_ddcr_state_t *s);

  bool dp_ddcr_get_narrow_pulse (const dp_ddcr_state_t *s);

  bool dp_ddcr_get_clipped (const dp_ddcr_state_t *s);

  int ddcr_set_telemetry (dp_ddcr_state_t *s, dp_tlm_t *tlm, const char *prefix,
                          uint32_t decim);


#ifdef __cplusplus
}
#endif

#endif /* DDCR_CORE_H */
```


