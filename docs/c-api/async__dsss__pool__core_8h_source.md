

# File async\_dsss\_pool\_core.h

[**File List**](files.md) **>** [**async\_dsss\_pool**](dir_24b3a88e64d64c92fdc3841f769b80af.md) **>** [**async\_dsss\_pool\_core.h**](async__dsss__pool__core_8h.md)

[Go to the documentation of this file](async__dsss__pool__core_8h.md)


```C++

#ifndef ASYNC_DSSS_POOL_CORE_H
#define ASYNC_DSSS_POOL_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "acq/acq_core.h"
#include "dp_event_log/dp_event_log_core.h"
#include "dll/dll_core.h"
#include "costas/costas_core.h"
#include "RateConverter/RateConverter_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "cic/cic_core.h"
#include "resample/resample_core.h"
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "corr2d/corr2d_core.h"
#include "fft2d/fft2d_core.h"
#include "fft/fft_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include "carrier_acq/carrier_acq_core.h"
#include "resamp/resamp_core.h"
#include "hbdecim/hbdecim_core.h"
#include "dp_parallel.h"
#include "dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ASYNC_DSSS_POOL_MAX_EMITTER_ON_TIME_SECS (15.0 * 60.0)

#define ASYNC_DSSS_POOL_STATE_MAGIC DP_FOURCC ('A', 'D', 'P', 'L')
#define ASYNC_DSSS_POOL_STATE_VERSION 3u /* v3: cell receivers only; v2: the flavour */

  typedef struct
  {
    size_t   slot;        
    int      assigned;    
    int      state;       
    uint64_t seed_sample; 
    double   seed_chip_phase; 
    double   seed_doppler_hz; 
    double   seed_cn0_dbhz;   
    double   doppler_hz;      
    double   chip_phase;      
    double   code_rate;       
    double   cn0_dbhz_est;    
    int      code_locked;     
    int      locked;          
    double   lock_metric;     
    uint64_t state_samples;   
    uint64_t both_down_samples; 
    uint64_t assigned_samples;  
  } async_dsss_pool_slot_t;

  typedef struct
  {
    int      assigned;        
    uint64_t seed_sample;     
    double   seed_chip_phase; 
    double   seed_doppler_hz;
    double   seed_cn0_dbhz;
    double   doppler_hz; 
    double   chip_phase;
    int      prev_state; 
    int      prev_code;  
    int      prev_sym;
    int      had_code;   
  } async_dsss_pool_row_t;

  typedef struct
  {
    /* Read-back (property-backed). */
    size_t   n_slots;    
    size_t   n_assigned; 
    uint64_t dropped;    
    uint64_t events;     
    uint64_t samples_consumed; 
    /* Config, restored by create(), never by the blob. */
    uint8_t *code;
    size_t   code_len;
    size_t   spc;
    double   chip_rate;
    double   symbol_rate;
    double   fs;
    double   carrier_freq_hz;
    uint64_t max_on_samples; 
    size_t   max_peaks;
    int      threads;
    /* The children. */
    acq_state_t                  *acq;
    async_dsss_receiver_state_t **rx;   
    dp_pool_t                    *pool; 
    dp_event_log_t               *log;  
    /* The table and the per-push outputs. */
    async_dsss_pool_row_t *rows;    
    acq_result_t          *hits;    
    float _Complex        *sym_buf; 
    size_t                 sym_cap; 
    size_t                *n_sym;   
    /* The feed's scratch: the block every receiver sees, for the fan. */
    const float _Complex *feed_x;
    size_t                feed_n;
  } async_dsss_pool_state_t;

async_dsss_pool_state_t *async_dsss_pool_create(const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa, double pd, double doppler_uncertainty, size_t code_only_epochs, double doppler_rate, size_t max_peaks, size_t n_slots, int threads, double carrier_freq_hz, double lost_confirm_s, double max_emitter_on_time_secs, size_t segments, size_t sps, int differential, double gain, size_t pullin_intervals);

void async_dsss_pool_destroy(async_dsss_pool_state_t *state);

void async_dsss_pool_reset(async_dsss_pool_state_t *state);

size_t async_dsss_pool_push(async_dsss_pool_state_t *state, const float _Complex *x, size_t x_len);

async_dsss_pool_slot_t async_dsss_pool_status(async_dsss_pool_state_t *state, size_t slot);

size_t async_dsss_pool_symbols_max_out(async_dsss_pool_state_t *state);

size_t async_dsss_pool_symbols(async_dsss_pool_state_t *state, size_t slot, float _Complex *out, size_t max_out);

int async_dsss_pool_set_event_log(async_dsss_pool_state_t *state, dp_event_log_t * log);

  /* ── Serializable state (docs/design/state-serialization.md) ──────────
   * A composition: the pool's own counters and the table, then the
   * searcher's blob and every receiver's, each self-validating. Config
   * (the geometry, the slot count, the carrier, the attached log) is
   * restored by create(), not the blob; a blob from a pool of another
   * slot count is rejected. The symbol buffer is scratch -- the last
   * push's symbols do not survive a hand-off. */
  size_t async_dsss_pool_state_bytes (const async_dsss_pool_state_t *state);
  void   async_dsss_pool_get_state (const async_dsss_pool_state_t *state,
                                    void                          *blob);
  int    async_dsss_pool_set_state (async_dsss_pool_state_t *state,
                                    const void              *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_POOL_CORE_H */
```


