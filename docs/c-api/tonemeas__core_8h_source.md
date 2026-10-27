

# File tonemeas\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**tonemeas**](dir_e0665d68cb2c2ff7feb28506d37a8bd1.md) **>** [**tonemeas\_core.h**](tonemeas__core_8h.md)

[Go to the documentation of this file](tonemeas__core_8h.md)


```C++

#ifndef DP_TONEMEAS_CORE_H
#define DP_TONEMEAS_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/measure/measure_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/dp_complex.h"
#include "doppler/fft/fft_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/acc_trace/acc_trace_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dp_psd_state_t *psd;     
    float         *pwr;     
    unsigned char *excl;    
    double enbw;            
    double beta;            
    size_t lobe_bins;       
    size_t spur_guard_bins; 
    size_t n;               
    size_t nfft;            
    size_t n_harm;          
    double fs;              
    size_t dc_guard;        
} dp_tonemeas_state_t;

dp_tonemeas_state_t *dp_tonemeas_create(size_t n, double fs, size_t n_harmonics,
                                  double full_scale, size_t bits,
                                  double dynamic_range_db, size_t dc_guard);

void dp_tonemeas_destroy(dp_tonemeas_state_t *state);

void dp_tonemeas_reset(dp_tonemeas_state_t *state);

tone_meas_t dp_tonemeas_analyze(dp_tonemeas_state_t *state, const float *x,
                             size_t n_in);

tone_meas_t dp_tonemeas_analyze_complex(dp_tonemeas_state_t *state,
                                     const float _Complex *x, size_t n_in);

time_stats_t dp_tonemeas_time_stats(dp_tonemeas_state_t *state, const float *x,
                                 size_t n_in);

size_t dp_tonemeas_spectrum_dbfs_max_out(dp_tonemeas_state_t *state);

size_t dp_tonemeas_spectrum_dbfs(dp_tonemeas_state_t *state, const float *x,
                              size_t x_len, float *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* TONEMEAS_CORE_H */
```


