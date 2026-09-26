

# File dsss\_burst\_receiver\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dsss\_burst\_receiver**](dir_630068a67b306c85c8348e5ba842eaef.md) **>** [**dsss\_burst\_receiver\_core.h**](dsss__burst__receiver__core_8h.md)

[Go to the documentation of this file](dsss__burst__receiver__core_8h.md)


```C++

#ifndef DP_DSSS_BURST_RECEIVER_CORE_H
#define DP_DSSS_BURST_RECEIVER_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/buffer/buffer.h"
#include "doppler/dp_state.h"

typedef struct
{
  uint64_t preamble_start; 
  double   doppler_hz_est; 
  double   doppler_res_hz; 
  double   cn0_dbhz_est;   
  double   est_freq_hz;    
  double   est_rate_hz;    
  double   demod_cn0_dbhz; 
  double   demod_timing_chips; 
  uint8_t  frame_valid;    
} dsss_br_event_t;

#include "doppler/burst_capture/burst_capture_core.h"
#include "doppler/burst_acq/burst_acq_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/burst_demod/burst_demod_core.h"
#include "doppler/burst_despreader/burst_despreader_core.h"
#include "doppler/ppe/ppe_core.h"
#include "doppler/corr/corr_core.h"
#include "doppler/corr2d/corr2d_core.h"
#include "doppler/fft2d/fft2d_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/conv/conv_core.h"
#include "doppler/rs/rs_core.h"
#include "doppler/gold/gold_core.h"
#include "doppler/mpsk/mpsk_core.h"
#include "doppler/cvt/cvt_core.h"
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
  size_t   frame_syms;   
  /* ── Derived geometry ───────────────────────────────────────────────── */
  size_t code_period; 
  size_t burst_len;   
  /* ── The composed children (each certified separately) ──────────────── */
  dp_burst_capture_state_t *cap;   
  dp_burst_demod_state_t   *demod; 
  /* ── The DetectionEvent, describing the most recent completed burst ─── */
  uint64_t preamble_start; 
  double   doppler_hz_est; 
  double   doppler_res_hz; 
  double   cn0_dbhz_est;   
  double   est_freq_hz;    
  double   est_rate_hz;    
  double   demod_cn0_dbhz; 
  double   demod_timing_chips; 
  int      frame_valid;    
  /* ── The completed bursts of the LAST push ───────────────────────────
   * Scratch, deliberately NOT serialized: it describes the most recent
   * push() only, so keeping it out of the blob is what lets state_bytes()
   * stay a pure function of configuration (finding F5). Grows on demand,
   * because the count scales with the caller's block size, not with any
   * configuration. */
  dsss_br_event_t *ev;     
  size_t           ev_cap; 
  float  *llr;     
  size_t  llr_cap; 
  size_t  llr_len; 
  size_t  frame_nbits; 
  size_t           ev_len; 
  /* ── Bookkeeping ────────────────────────────────────────────────────── */
  uint64_t n_bursts; 
/*<<property_struct_fields>>*/
} dp_dsss_burst_receiver_state_t;

dp_dsss_burst_receiver_state_t *dp_dsss_burst_receiver_create(const uint8_t *acq_code, size_t acq_code_len, const uint8_t *data_code, size_t data_code_len, const uint8_t *sync, size_t sync_len, size_t reps, size_t spc, double chip_rate, size_t frame_syms, double cn0_dbhz, double doppler_uncertainty, double pfa, double pd, double carrier_hz, double max_rate, size_t est_segments);

void dp_dsss_burst_receiver_destroy(dp_dsss_burst_receiver_state_t *state);

void dp_dsss_burst_receiver_reset(dp_dsss_burst_receiver_state_t *state);









size_t dp_dsss_burst_receiver_push_max_out(dp_dsss_burst_receiver_state_t *state, size_t x_len);

size_t dp_dsss_burst_receiver_push(dp_dsss_burst_receiver_state_t *state, const float _Complex *x, size_t x_len, uint8_t *out, size_t max_out);

size_t dp_dsss_burst_receiver_events_max_out(dp_dsss_burst_receiver_state_t *state);

size_t dp_dsss_burst_receiver_llrs(dp_dsss_burst_receiver_state_t *state, size_t n, float *out, size_t max_out);

size_t dp_dsss_burst_receiver_llrs_max_out(dp_dsss_burst_receiver_state_t *state, size_t n);


size_t dp_dsss_burst_receiver_events(dp_dsss_burst_receiver_state_t *state, size_t n, dsss_br_event_t *out, size_t max_out);
int dp_dsss_burst_receiver_configure_search_raw(dp_dsss_burst_receiver_state_t *state, size_t doppler_bins, size_t n_noncoh);
uint64_t dp_dsss_burst_receiver_get_preamble_start(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_doppler_hz_est(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_doppler_res_hz(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_cn0_dbhz_est(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_est_freq_hz(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_est_rate_hz(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_demod_cn0_dbhz(const dp_dsss_burst_receiver_state_t *state);
double dp_dsss_burst_receiver_get_demod_timing_chips(const dp_dsss_burst_receiver_state_t *state);
size_t dp_dsss_burst_receiver_get_pending(const dp_dsss_burst_receiver_state_t *state);
uint64_t dp_dsss_burst_receiver_get_dropped(const dp_dsss_burst_receiver_state_t *state);
uint64_t dp_dsss_burst_receiver_get_n_bursts(const dp_dsss_burst_receiver_state_t *state);

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
#define DSSS_BURST_RECEIVER_STATE_VERSION 7u

size_t dp_dsss_burst_receiver_state_bytes(const dp_dsss_burst_receiver_state_t *state);

void dp_dsss_burst_receiver_get_state(const dp_dsss_burst_receiver_state_t *state, void *blob);

int dp_dsss_burst_receiver_set_state(dp_dsss_burst_receiver_state_t *state, const void *blob);
size_t dp_dsss_burst_receiver_get_min_gap (
    const dp_dsss_burst_receiver_state_t *state);
size_t dp_dsss_burst_receiver_get_refine_span(const dp_dsss_burst_receiver_state_t *state);
size_t dp_dsss_burst_receiver_get_retain_span(const dp_dsss_burst_receiver_state_t *state);
bool dp_dsss_burst_receiver_get_frame_valid(const dp_dsss_burst_receiver_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DSSS_BURST_RECEIVER_CORE_H */
```


