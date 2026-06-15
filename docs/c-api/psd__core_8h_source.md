

# File psd\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**psd**](dir_1f3d46873d925f2e533983763479900d.md) **>** [**psd\_core.h**](psd__core_8h.md)

[Go to the documentation of this file](psd__core_8h.md)


```C++

#ifndef PSD_CORE_H
#define PSD_CORE_H

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
    size_t bits;               
} psd_state_t;

psd_state_t *psd_create(size_t n, double fs, int window, float beta,
                            size_t pad, double full_scale, size_t bits,
                            int mode, double alpha);

void psd_destroy(psd_state_t *state);

void psd_reset(psd_state_t *state);

void psd_accumulate(psd_state_t *state, const float complex *x,
                      size_t x_len);

void psd_accumulate_real(psd_state_t *state, const float *x, size_t x_len);

size_t psd_power_twosided_max_out(psd_state_t *state);

size_t psd_power_twosided(psd_state_t *state, size_t cap, float *out);

size_t psd_power_onesided_max_out(psd_state_t *state);

size_t psd_power_onesided(psd_state_t *state, size_t cap, float *out);

size_t psd_psd_db_max_out(psd_state_t *state);

size_t psd_psd_db(psd_state_t *state, size_t n, float *out);

size_t psd_psd_dbhz_max_out(psd_state_t *state);

size_t psd_psd_dbhz(psd_state_t *state, size_t n, float *out);

size_t psd_band_power_max_out(psd_state_t *state);

size_t psd_band_power(psd_state_t *state, const double *bands,
                        size_t bands_len, float *out);

double psd_total_band_power(psd_state_t *state, const double *bands,
                              size_t bands_len);

double psd_occupied_bw(psd_state_t *state, double fraction);

double psd_noise_floor(psd_state_t *state);

double psd_snr(psd_state_t *state, double lo_hz, double hi_hz);

double psd_sfdr(psd_state_t *state, float min_db);
#ifdef __cplusplus
}
#endif

#endif /* PSD_CORE_H */
```


