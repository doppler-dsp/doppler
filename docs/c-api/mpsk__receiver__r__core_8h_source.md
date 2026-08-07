

# File mpsk\_receiver\_r\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver\_r**](dir_2235ea4ae040991d93c0b2870a03660e.md) **>** [**mpsk\_receiver\_r\_core.h**](mpsk__receiver__r__core_8h.md)

[Go to the documentation of this file](mpsk__receiver__r__core_8h.md)


```C++

#ifndef MPSK_RECEIVER_R_CORE_H
#define MPSK_RECEIVER_R_CORE_H

#include "clib_common.h"
#include "ddcr/ddcr_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "mpsk_receiver/mpsk_rx_loops.h"
#include <complex.h>
#include "ddc/ddc_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "ratesync/ratesync_core.h"
#include "RateConverter/RateConverter_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "cic/cic_core.h"
#include "fir/fir_core.h"
#include "resample/resample_core.h"
#include "lo/lo_core.h"
#include "nco/nco_core.h"
#include "loop_filter/loop_filter_core.h"
#include "lockdet/lockdet_core.h"
#include "symsync/symsync_core.h"
#include "agc/agc_core.h"
#include "boxcar/boxcar_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include "ber/ber_core.h"
#include "telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    ddcr_state_t   *fe; 
    mpsk_rx_loops_t l;  
    double centre_freq; 
  } mpsk_receiver_r_state_t;

  mpsk_receiver_r_state_t *
  mpsk_receiver_r_create (int m, double sps, size_t m_out, int pulse,
                          double rrc_beta, int rrc_span, double bn_carrier,
                          double zeta, double bn_timing, int acq_to_track,
                          double lock_thresh, double init_norm_freq,
                          size_t warmup_syms, int differential,
                          size_t num_phases, int nda_tap);

  void mpsk_receiver_r_destroy (mpsk_receiver_r_state_t *state);

  void mpsk_receiver_r_reset (mpsk_receiver_r_state_t *state);

  JM_FORCEINLINE JM_HOT int
  mpsk_receiver_r_step_ted (mpsk_receiver_r_state_t *s, float x,
                            float complex *y_out, int ted)
  {
    float complex ys[4];
    float complex zlo;
    int           n_lo = 0;
    size_t        n    = ddcr_execute_ctrl_push_tap (
        s->fe, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), &zlo, &n_lo);
    /* The halfband gates this: n_lo is 0 on every other input, so the arm and
       its discriminator run at fs_in/2, the LO's own rate. */
    if (n_lo)
      mpsk_rx_push_lo (&s->l, zlo);
    int           emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= mpsk_rx_take_output (&s->l, ys[oi], y_out, ted);
    return emitted;
  }

  size_t mpsk_receiver_r_steps_max_out (mpsk_receiver_r_state_t *state);
  size_t mpsk_receiver_r_steps (mpsk_receiver_r_state_t *state, const float *x,
                                size_t x_len, float complex *out,
                                size_t max_out);

  size_t mpsk_receiver_r_bits_max_out (mpsk_receiver_r_state_t *state);
  size_t mpsk_receiver_r_bits (mpsk_receiver_r_state_t *state, const float *x,
                               size_t x_len, uint8_t *out, size_t max_out);

  double mpsk_receiver_r_get_norm_freq (const mpsk_receiver_r_state_t *state);
  double mpsk_receiver_r_get_nco_freq (const mpsk_receiver_r_state_t *state);
  void   mpsk_receiver_r_set_norm_freq (mpsk_receiver_r_state_t *state,
                                        double                   val);
  double mpsk_receiver_r_get_lock (const mpsk_receiver_r_state_t *state);
  int    mpsk_receiver_r_get_locked (const mpsk_receiver_r_state_t *state);
  double mpsk_receiver_r_get_last_error (const mpsk_receiver_r_state_t *state);
  void mpsk_receiver_r_configure_lock (mpsk_receiver_r_state_t *state,
                                       double up_thresh, double down_thresh,
                                       uint32_t n_up, uint32_t n_down);
  int mpsk_receiver_r_set_telemetry (mpsk_receiver_r_state_t *state,
                                     dp_tlm_t *tlm, const char *prefix,
                                     uint32_t decim);
  double mpsk_receiver_r_get_timing_rate (const mpsk_receiver_r_state_t *s);
  int    mpsk_receiver_r_get_tracking (const mpsk_receiver_r_state_t *state);
  int    mpsk_receiver_r_get_m (const mpsk_receiver_r_state_t *state);
  double mpsk_receiver_r_get_sps (const mpsk_receiver_r_state_t *state);
  size_t mpsk_receiver_r_get_m_out (const mpsk_receiver_r_state_t *state);
  int mpsk_receiver_r_get_clipped (const mpsk_receiver_r_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: the front end's and the loops' self-validating child blobs,
 * exactly as the complex twin. */
#define MPSK_RECEIVER_R_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'R')
#define MPSK_RECEIVER_R_STATE_VERSION 2u
  size_t mpsk_receiver_r_state_bytes (const mpsk_receiver_r_state_t *state);
  void   mpsk_receiver_r_get_state (const mpsk_receiver_r_state_t *state,
                                    void                          *blob);
  int    mpsk_receiver_r_set_state (mpsk_receiver_r_state_t *state,
                                    const void              *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_R_CORE_H */
```


