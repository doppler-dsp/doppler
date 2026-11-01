

# File async\_dsss\_pool\_core.h

[**File List**](files.md) **>** [**async\_dsss\_pool**](dir_8a6668f3097fb7847a23b1f65b3df26f.md) **>** [**async\_dsss\_pool\_core.h**](async__dsss__pool__core_8h.md)

[Go to the documentation of this file](async__dsss__pool__core_8h.md)


```C++

#ifndef DP_ASYNC_DSSS_POOL_CORE_H
#define DP_ASYNC_DSSS_POOL_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_parallel.h"
#include "doppler/dp_state.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/detector/detector_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/corr/corr_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

#define ASYNC_DSSS_POOL_MAX_RECEIVERS ASYNC_DSSS_RX_MAX_PEAKS

#define ASYNC_DSSS_POOL_NO_RECEIVER (-1)

#define ASYNC_DSSS_POOL_STATE_MAGIC   DP_FOURCC ('A', 'D', 'P', 'L')
#define ASYNC_DSSS_POOL_STATE_VERSION 4u

#define ASYNC_DSSS_POOL_GHOST_WINDOW_S 0.2
#define ASYNC_DSSS_POOL_GHOST_EPS_HZ 10.0
#define ASYNC_DSSS_POOL_GHOST_STILL_HZ 10.0
#define ASYNC_DSSS_POOL_GHOST_LOCK_FRAC 0.2

  typedef struct
  {
    dp_async_dsss_receiver_state_t **rx; 
    dp_pool_t *workers; 
    int        threads; 
    float _Complex *sym_buf; 
    size_t          sym_cap; 
    size_t sym_extra; 
    size_t         *n_sym;   
    const float _Complex *feed_x; 
    size_t                feed_n; 
    size_t   period;             
    float _Complex *carry;       
    size_t          carry_len;   
    size_t   n_receivers;        
    uint64_t samples;            
    uint64_t released_lost;      
    uint64_t released_duplicate; 
    uint64_t released_ghost;     
    double   lattice_hz;   
    size_t   ghost_window; 
    double  *car_hist;     
    uint8_t *cl_hist;      
    size_t  *hist_len;     
    size_t   hist_pos;     
    dp_tlm_t *tlm;               
    char      tlm_prefix[DP_TLM_NAME_MAX]; 
    uint32_t  tlm_decim;                   
  } dp_async_dsss_pool_state_t;

  dp_async_dsss_pool_state_t *dp_async_dsss_pool_create (
      const uint8_t *code, size_t code_len, size_t n_receivers,
      double chip_rate, double symbol_rate, double sample_rate, int m,
      double cn0_dbhz, double pfa, double pd, double doppler_uncertainty,
      size_t segments, size_t sps, int differential,
      double refine_max_error_db, size_t refine_samples_per_symbol,
      double refine_design_margin_db, size_t refine_n_fft,
      size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s, bool reacquire, double code_only_s,
      double doppler_rate, int threads);

  void dp_async_dsss_pool_destroy (dp_async_dsss_pool_state_t *state);

  void dp_async_dsss_pool_reset (dp_async_dsss_pool_state_t *state);

  size_t
  dp_async_dsss_pool_state_bytes (const dp_async_dsss_pool_state_t *state);

  void dp_async_dsss_pool_get_state (const dp_async_dsss_pool_state_t *state,
                                     void                             *blob);

  int dp_async_dsss_pool_set_state (dp_async_dsss_pool_state_t *state,
                                    const void                 *blob);

  int dp_async_dsss_pool_get_threads (const dp_async_dsss_pool_state_t *state);

  size_t
  dp_async_dsss_pool_get_coherent_bins (const dp_async_dsss_pool_state_t *state);

  size_t dp_async_dsss_pool_push (dp_async_dsss_pool_state_t *state,
                                  const float _Complex *x, size_t x_len);

  async_dsss_receiver_status_t
  dp_async_dsss_pool_status (const dp_async_dsss_pool_state_t *state,
                             size_t                            receiver);

  size_t
  dp_async_dsss_pool_symbols_max_out (const dp_async_dsss_pool_state_t *state);

  size_t dp_async_dsss_pool_symbols (const dp_async_dsss_pool_state_t *state,
                                     size_t receiver, float _Complex *out,
                                     size_t max_out);

  int dp_async_dsss_pool_set_threads (dp_async_dsss_pool_state_t *state,
                                      int                         n);

  int dp_async_dsss_pool_set_telemetry (dp_async_dsss_pool_state_t *state,
                                        dp_tlm_t *tlm, const char *prefix,
                                        uint32_t decim);

  int dp_async_dsss_pool_ghost_of (const dp_async_dsss_pool_state_t *state,
                                   size_t i, size_t j);

  dp_async_dsss_receiver_state_t *
  dp_async_dsss_pool_receiver (const dp_async_dsss_pool_state_t *state,
                               size_t                            receiver);

#ifdef __cplusplus
}
#endif

#endif /* DP_ASYNC_DSSS_POOL_CORE_H */
```


