

# File ddcr\_core.h

[**File List**](files.md) **>** [**ddcr**](dir_46c04c942eb84c8716610cebe515b046.md) **>** [**ddcr\_core.h**](ddcr__core_8h.md)

[Go to the documentation of this file](ddcr__core_8h.md)


```C++

#ifndef DDCR_CORE_H
#define DDCR_CORE_H

#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "lo/lo_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "hbdecim/hbdecim_r2c_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct ddcr_state
  {
    hbdecim_r2c_state_t   *r2c;  
    lo_state_t            *lo;   
    RateConverter_state_t *rc;   
    double                 rate; 
    bool narrow_pulse;
  } ddcr_state_t;

  ddcr_state_t *ddcr_create (double norm_freq, double rate);

  ddcr_state_t *ddcr_create_matched (double norm_freq, double rate, int pulse,
                                     double beta, size_t span,
                                     double pulse_sps, size_t num_phases);

  void ddcr_destroy (ddcr_state_t *s);

  void ddcr_reset (ddcr_state_t *s);

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

  size_t ddcr_state_bytes (const ddcr_state_t *s);
  void ddcr_get_state (const ddcr_state_t *s, void *blob);
  int ddcr_set_state (ddcr_state_t *s, const void *blob);

  size_t ddcr_run (ddcr_state_t *s, const void *state_in, void *state_out,
                   const float *in, size_t n_in, float _Complex *out,
                   size_t max_out);

  double ddcr_get_norm_freq (const ddcr_state_t *s);

  void ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq);

  double ddcr_get_rate (const ddcr_state_t *s);

  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

  size_t ddcr_execute_max_out (ddcr_state_t *s);
  size_t ddcr_execute_ctrl_max_out (ddcr_state_t *s);
  size_t ddcr_execute_ctrl_push_max_out (ddcr_state_t *s);

  size_t ddcr_execute_ctrl (ddcr_state_t *s, const float *x, size_t n_in,
                            double rate_ctrl, double freq_ctrl,
                            float _Complex *out, size_t max_out);

  size_t ddcr_execute_ctrl_push (ddcr_state_t *s, float x, double rate_ctrl,
                                 double freq_ctrl, float _Complex *out,
                                 size_t max_out);

  size_t ddcr_execute_ctrl_push_tap (ddcr_state_t *s, float x,
                                     double rate_ctrl, double freq_ctrl,
                                     float _Complex *out, size_t max_out,
                                     float _Complex *lo_out, int *n_lo);

  bool ddcr_get_narrow_pulse (const ddcr_state_t *s);

  bool ddcr_get_clipped (const ddcr_state_t *s);


#ifdef __cplusplus
}
#endif

#endif /* DDCR_CORE_H */
```


