

# File dsss\_burst\_receiver\_core.h

[**File List**](files.md) **>** [**dsss\_burst\_receiver**](dir_32a143d35207eb7d99f4a541895f77eb.md) **>** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md)

[Go to the documentation of this file](dsss__burst__receiver__core_8h.md)


```C++

#ifndef DSSS_BURST_RECEIVER_CORE_H
#define DSSS_BURST_RECEIVER_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "buffer/buffer.h"
#include "dp_state.h"

#define DSSS_BR_QCAP 8u

typedef struct
{
  uint64_t anchor;     
  uint64_t start;      
  double   doppler_hz; 
  double   cn0_dbhz;   
  double   margin;     
  int      refined;    
} dsss_br_pending_t;
#include "burst_acq/burst_acq_core.h"
#include "acq/acq_core.h"
#include "burst_demod/burst_demod_core.h"
#include "burst_despreader/burst_despreader_core.h"
#include "ppe/ppe_core.h"
#include "corr/corr_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "spectral/spectral_core.h"
#include "loop_filter/loop_filter_core.h"
#include "detection/detection_core.h"
#include "fft/fft_core.h"
#include "pn/pn_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /* ── Configuration, copied at create() ──────────────────────────────── */
  uint8_t *acq_code;     
  uint8_t *data_code;    
  uint8_t *sync;         
  size_t   acq_code_len; 
  size_t   data_code_len;
  size_t   sync_len;     
  size_t   reps;         
  size_t   spc;          
  double   chip_rate;    
  size_t   payload_len;  
  /* ── Derived geometry ───────────────────────────────────────────────── */
  size_t code_period; 
  size_t burst_len;   
  /* ── The composed children (each certified separately) ──────────────── */
  burst_acq_state_t   *acq;   
  burst_demod_state_t *demod; 
  /* ── Look-back (docs/design/dsss-burst-receiver.md §7.1) ────────────── */
  dp_f32_t *hist;      
  uint64_t samples_fed; 
  /* ── The DetectionEvent, describing the most recent completed burst ─── */
  uint64_t preamble_start; 
  int      frame_valid;    
  double   doppler_hz_est; 
  double   doppler_res_hz; 
  double   cn0_dbhz_est;   
  double   est_freq_hz;    
  double   est_rate_hz;    
  double   est_snr_db;     
  double   refine_margin;  
  /* ── Refine scratch (docs/design/dsss-burst-receiver.md §3.4) ───────── */
  float *ref_sign;   
  float _Complex *corr_buf; 
  size_t refine_span;  
  size_t corr_len;     
  size_t retain_span;  
  size_t chunk_max;    
  /* ── Bursts in flight (at most one is RETURNED per push) ─────────────── */
  dsss_br_pending_t q[DSSS_BR_QCAP]; 
  size_t            q_head;          
  size_t            q_len;           
  uint64_t suppress_until; 
  size_t acq_blob_max; 
  size_t k_lo; 
  size_t k_hi; 
  /* ── Bookkeeping ────────────────────────────────────────────────────── */
  size_t   pending;  
  uint64_t dropped;  
  uint64_t n_bursts; 
/*<<property_struct_fields>>*/
} dsss_burst_receiver_state_t;

dsss_burst_receiver_state_t *dsss_burst_receiver_create(const uint8_t *acq_code, size_t acq_code_len, const uint8_t *data_code, size_t data_code_len, const uint8_t *sync, size_t sync_len, size_t reps, size_t spc, double chip_rate, size_t payload_len, double cn0_dbhz, double doppler_uncertainty, double pfa, double pd, double carrier_hz, double max_rate, size_t est_segments);

void dsss_burst_receiver_destroy(dsss_burst_receiver_state_t *state);

void dsss_burst_receiver_reset(dsss_burst_receiver_state_t *state);









size_t dsss_burst_receiver_push_max_out(dsss_burst_receiver_state_t *state, size_t x_len);

size_t dsss_burst_receiver_push(dsss_burst_receiver_state_t *state, const float complex *x, size_t x_len, uint8_t *out, size_t max_out);
int dsss_burst_receiver_configure_search_raw(dsss_burst_receiver_state_t *state, size_t doppler_bins, size_t n_noncoh);
uint64_t dsss_burst_receiver_get_preamble_start(const dsss_burst_receiver_state_t *state);
int dsss_burst_receiver_get_frame_valid(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_doppler_hz_est(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_doppler_res_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_cn0_dbhz_est(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_freq_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_rate_hz(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_est_snr_db(const dsss_burst_receiver_state_t *state);
double dsss_burst_receiver_get_refine_margin(const dsss_burst_receiver_state_t *state);
size_t dsss_burst_receiver_get_pending(const dsss_burst_receiver_state_t *state);
uint64_t dsss_burst_receiver_get_dropped(const dsss_burst_receiver_state_t *state);
uint64_t dsss_burst_receiver_get_n_bursts(const dsss_burst_receiver_state_t *state);

/* ── Serializable state — the elastic / pure-transducer face ──────────────
 *
 * The composition's checkpoint boundary is BETWEEN bursts, which is what
 * makes burst_demod's deliberate statelessness cost nothing: a burst
 * completes inside one demod() call or is lost (its own validation report
 * certifies that as correct), so there is no mid-demod position to save.
 * What must travel is this object's own stream bookkeeping, the retained
 * look-back the next burst may still need, and the acquisition engine's
 * own state -- delegated to its triplet, never re-packed here.
 */

#define DSSS_BURST_RECEIVER_STATE_MAGIC DP_FOURCC('D', 'B', 'R', 'X')
#define DSSS_BURST_RECEIVER_STATE_VERSION 1u

size_t dsss_burst_receiver_state_bytes(const dsss_burst_receiver_state_t *state);

void dsss_burst_receiver_get_state(const dsss_burst_receiver_state_t *state, void *blob);

int dsss_burst_receiver_set_state(dsss_burst_receiver_state_t *state, const void *blob);
#ifdef __cplusplus
}
#endif

#endif /* DSSS_BURST_RECEIVER_CORE_H */
```


