

# File mpsk\_receiver\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md)

[Go to the documentation of this file](mpsk__receiver__core_8h.md)


```C++

#ifndef MPSK_RECEIVER_CORE_H
#define MPSK_RECEIVER_CORE_H

#include "clib_common.h"
#include "ddc/ddc_core.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "mpsk_receiver/mpsk_rx_loops.h"
#include <complex.h>
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
    ddc_state_t    *fe; 
    mpsk_rx_loops_t l;  
    /* ── config (restored by create(), never packed in a state blob) ── */
    /* The pulse geometry lives in the front end, which is the only thing
       that uses it; keeping a second copy here would be a shadow of the
       cascade's own configuration, free to drift out of step with it. */
    double centre_freq; 
  } mpsk_receiver_state_t;

  mpsk_receiver_state_t *
  mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                        double rrc_beta, int rrc_span, double bn_carrier,
                        double zeta, double bn_timing, int acq_to_track,
                        double lock_thresh, double init_norm_freq,
                        size_t warmup_syms, int differential,
                        size_t num_phases, int nda_tap, int agc,
                        double bn_agc_ratio);

  double mpsk_receiver_get_agc_gain_db (const mpsk_receiver_state_t *state);

  void mpsk_receiver_destroy (mpsk_receiver_state_t *state);

  void mpsk_receiver_reset (mpsk_receiver_state_t *state);

  JM_FORCEINLINE JM_HOT int
  mpsk_receiver_step_ted (mpsk_receiver_state_t *s, float complex x,
                          float complex *y_out, int ted)
  {
    float complex ys[4];
    float complex zlo;
    int           n_lo = 0;
    float complex zpre;
    int           n_pre = 0;
    size_t        n     = ddc_execute_ctrl_push_tap2 (
        s->fe, x, s->l.timing.ctrl, s->l.freq_ctrl, ys,
        sizeof (ys) / sizeof (ys[0]), &zlo, &n_lo, &zpre, &n_pre);
    /* The two timing-independent NDA taps read here, ahead of the matched
       filter. Each is a no-op unless it is the configured one. */
    if (n_lo)
      mpsk_rx_push_lo (&s->l, zlo);
    if (n_pre)
      mpsk_rx_push_preterm (&s->l, zpre);
    int           emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= mpsk_rx_take_output (&s->l, ys[oi], y_out, ted);
    return emitted;
  }

  size_t mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state);
  size_t mpsk_receiver_steps (mpsk_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  size_t mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state);
  size_t mpsk_receiver_bits (mpsk_receiver_state_t *state,
                             const float complex *x, size_t x_len,
                             uint8_t *out, size_t max_out);

  double mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_nco_freq (const mpsk_receiver_state_t *state);
  void mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val);
  double mpsk_receiver_get_lock (const mpsk_receiver_state_t *state);
  int mpsk_receiver_get_locked (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_last_error (const mpsk_receiver_state_t *state);

  void mpsk_receiver_configure_lock (mpsk_receiver_state_t *state,
                                     double up_thresh, double down_thresh,
                                     uint32_t n_up, uint32_t n_down);

  int mpsk_receiver_set_telemetry (mpsk_receiver_state_t *state, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);
  double mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_m (const mpsk_receiver_state_t *state);
  double mpsk_receiver_get_sps (const mpsk_receiver_state_t *state);
  size_t mpsk_receiver_get_m_out (const mpsk_receiver_state_t *state);
  int mpsk_receiver_get_clipped (const mpsk_receiver_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: the front end's and the loops' self-validating child blobs.
 * Every scalar this object carries across inputs lives in one of them; the
 * cascade, its banks and the LO centre are restored by create. */
#define MPSK_RECEIVER_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'K')
#define MPSK_RECEIVER_STATE_VERSION 6u /* v5: rebuilt on the matched DDC */
  size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *state);
  void   mpsk_receiver_get_state (const mpsk_receiver_state_t *state,
                                  void                        *blob);
  int mpsk_receiver_set_state (mpsk_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_CORE_H */
```


