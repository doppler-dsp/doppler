

# File timing\_est\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**timing\_est**](dir_ef0d777ccea6407e2e2ec8e704716d8f.md) **>** [**timing\_est\_core.h**](timing__est__core_8h.md)

[Go to the documentation of this file](timing__est__core_8h.md)


```C++

#ifndef DP_TIMING_EST_CORE_H
#define DP_TIMING_EST_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/nco/nco_core.h"
#ifdef __cplusplus
extern "C" {
#endif

#define TIMING_EST_STATE_MAGIC DP_FOURCC ('T', 'M', 'G', 'E')
#define TIMING_EST_STATE_VERSION 1u

typedef struct {
    double tau;     
    double rate;    
    double quality; 
    double power;   
    double symbols; 
    int has_rate;   
} dp_timing_estimate_t;

typedef struct {
    double   sps;       
    uint32_t inc;       
    uint32_t phase;     
    uint64_t count;     
    /* the block in progress */
    double   c_re;      
    double   c_im;      
    double   pwr;       
    uint64_t blk_n;     
    uint64_t blk_first; 
    /* the previous block, for the rate */
    int      have_prev; 
    double   prev_tau;  
    double   prev_mid;  
} dp_timing_est_state_t;

dp_timing_est_state_t *dp_timing_est_create(double sps);

void dp_timing_est_destroy(dp_timing_est_state_t *state);

void dp_timing_est_reset(dp_timing_est_state_t *state);

JM_FORCEINLINE void
dp_timing_est_accumulate (dp_timing_est_state_t *state, double power)
{
  if (state->blk_n == 0)
    state->blk_first = state->count;
  double th = 6.283185307179586476925 * nco_word_to_norm (state->phase);
  state->c_re += power * cos (th);
  state->c_im -= power * sin (th);
  state->pwr += power;
  state->phase += state->inc;
  state->count++;
  state->blk_n++;
}

dp_timing_estimate_t dp_timing_est_finish(dp_timing_est_state_t *state);

size_t dp_timing_est_state_bytes(const dp_timing_est_state_t *state);

void dp_timing_est_get_state(const dp_timing_est_state_t *state, void *blob);

int dp_timing_est_set_state(dp_timing_est_state_t *state, const void *blob);

dp_timing_estimate_t dp_timing_est_estimate(dp_timing_est_state_t *state, const float _Complex *in, size_t n_in);
#ifdef __cplusplus
}
#endif

#endif /* DP_TIMING_EST_CORE_H */
```


