

# File mpsk\_receiver\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk\_receiver**](dir_a1dc26622ebd32726f4fc723db7ccb3b.md) **>** [**mpsk\_receiver\_core.h**](mpsk__receiver__core_8h.md)

[Go to the documentation of this file](mpsk__receiver__core_8h.md)


```C++

#ifndef MPSK_RECEIVER_CORE_H
#define MPSK_RECEIVER_CORE_H

#include "agc/agc_core.h"
#include "carrier_nda/carrier_nda_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "farrow/farrow_core.h"
#include "fir/fir_core.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include "symsync/symsync_core.h"
#include "telemetry/telemetry.h"
#include <complex.h>
#ifdef __cplusplus
extern "C"
{
#endif

  enum
  {
    MPSK_RX_PULSE_IANDD = 0, 
    MPSK_RX_PULSE_RRC   = 1  
  };

  typedef struct
  {
    carrier_nda_state_t car;  
    symsync_state_t     sync; 
    fir_state_t        *mf;   
    float              *mf_taps;  
    int                 m;        
    size_t              sps;      
    int                 n;        
    int                 pulse;    
    double              rrc_beta; 
    int                 rrc_span; 
    int                 acq_to_track; 
    double              lock_thresh;  
    size_t          warmup_syms; 
    lockdet_state_t handover;    
    int           tracking;      
    size_t        sym_count;     
    int           differential;  
    int           have_prev_idx; 
    unsigned      prev_idx;      
    float complex sym_rot;       
    /* Telemetry attachment (the receiver's own "lock" probe; the timing
     * probes ride the embedded symsync's attachment via the forwarded
     * attach). NULL ctx = detached. Never serialized: the hand-written
     * triplet packs children + running fields only. */
    dp_tlm_t *tlm_ctx;         
    int32_t   tlm_id_lock;     
    int32_t   tlm_id_tracking; 
  } mpsk_receiver_state_t;

  mpsk_receiver_state_t *
  mpsk_receiver_create (int m, size_t sps, int n, int pulse, double rrc_beta,
                        int rrc_span, double bn_carrier, double zeta,
                        double bn_timing, int acq_to_track, double lock_thresh,
                        double init_norm_freq, size_t warmup_syms,
                        int differential);

  void mpsk_receiver_destroy (mpsk_receiver_state_t *state);

  void mpsk_receiver_reset (mpsk_receiver_state_t *state);

  size_t mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state);
  size_t mpsk_receiver_steps (mpsk_receiver_state_t *state,
                              const float complex *x, size_t x_len,
                              float complex *out, size_t max_out);

  size_t mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state);
  size_t mpsk_receiver_bits (mpsk_receiver_state_t *state,
                             const float complex *x, size_t x_len,
                             uint8_t *out, size_t max_out);

  double mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state);
  void mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val);
  double mpsk_receiver_get_lock (const mpsk_receiver_state_t *state);

  void mpsk_receiver_configure_lock (mpsk_receiver_state_t *state,
                                     double up_thresh, double down_thresh,
                                     uint32_t n_up, uint32_t n_down);

  int mpsk_receiver_set_telemetry (mpsk_receiver_state_t *state, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);
  double mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_m (const mpsk_receiver_state_t *state);
  size_t mpsk_receiver_get_sps (const mpsk_receiver_state_t *state);
  int    mpsk_receiver_get_n (const mpsk_receiver_state_t *state);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition: carrier_nda + symsync + matched-filter children +
 * running tracking/handover state; MF taps restored by create. */
#define MPSK_RECEIVER_STATE_MAGIC DP_FOURCC ('M', 'P', 'S', 'K')
#define MPSK_RECEIVER_STATE_VERSION 4u /* v4: handover lockdet counters */
  size_t mpsk_receiver_state_bytes (const mpsk_receiver_state_t *state);
  void   mpsk_receiver_get_state (const mpsk_receiver_state_t *state,
                                  void                        *blob);
  int mpsk_receiver_set_state (mpsk_receiver_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RECEIVER_CORE_H */
```


