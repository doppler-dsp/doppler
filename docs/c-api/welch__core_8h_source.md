

# File welch\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**welch**](dir_aeb9e26b0edb1fd5fc61c8cd35fcdcfb.md) **>** [**welch\_core.h**](welch__core_8h.md)

[Go to the documentation of this file](welch__core_8h.md)


```C++

#ifndef WELCH_CORE_H
#define WELCH_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    fft_state_t *fft;          
    acc_trace_state_t *avg;    
    float *w;                  
    float complex *frame;      
    float complex *spec;       
    float *pwr;                
    float *dbbuf;              
    double cg;                 
    double s2;                 
    double enbw;               
    size_t n;                  
    size_t nfft;               
    double fs;                 
    double full_scale;         
} welch_state_t;

welch_state_t *welch_create(size_t n, double fs, int window, float beta,
                            size_t pad, double full_scale, int mode,
                            double alpha);

void welch_destroy(welch_state_t *state);

void welch_reset(welch_state_t *state);

void welch_accumulate(welch_state_t *state, const float complex *x,
                      size_t x_len);

void welch_accumulate_real(welch_state_t *state, const float *x, size_t x_len);

size_t welch_power_twosided_max_out(welch_state_t *state);

size_t welch_power_twosided(welch_state_t *state, size_t cap, float *out);

size_t welch_power_onesided_max_out(welch_state_t *state);

size_t welch_power_onesided(welch_state_t *state, size_t cap, float *out);

size_t welch_psd_db_max_out(welch_state_t *state);

size_t welch_psd_db(welch_state_t *state, size_t n, float *out);

size_t welch_psd_dbhz_max_out(welch_state_t *state);

size_t welch_psd_dbhz(welch_state_t *state, size_t n, float *out);

size_t welch_band_power_max_out(welch_state_t *state);

size_t welch_band_power(welch_state_t *state, const double *bands,
                        size_t bands_len, float *out);

double welch_total_band_power(welch_state_t *state, const double *bands,
                              size_t bands_len);

double welch_occupied_bw(welch_state_t *state, double fraction);

double welch_noise_floor(welch_state_t *state);

double welch_snr(welch_state_t *state, double lo_hz, double hi_hz);

double welch_sfdr(welch_state_t *state, float min_db);
#ifdef __cplusplus
}
#endif

#endif /* WELCH_CORE_H */
```


