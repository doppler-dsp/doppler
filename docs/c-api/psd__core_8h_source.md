

# File psd\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**psd**](dir_80b4a8b440284bffd6bf0fbeb7bfe41f.md) **>** [**psd\_core.h**](psd__core_8h.md)

[Go to the documentation of this file](psd__core_8h.md)


```C++

#ifndef DP_PSD_CORE_H
#define DP_PSD_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dp_fft_state_t *fft;          
    dp_acc_trace_state_t *avg;    
    float *w;                  
    float _Complex *frame;      
    float _Complex *spec;       
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
} dp_psd_state_t;

dp_psd_state_t *dp_psd_create(size_t n, double fs, int window, float beta,
                            size_t pad, double full_scale, size_t bits,
                            int mode, double alpha);

void dp_psd_destroy(dp_psd_state_t *state);

void dp_psd_reset(dp_psd_state_t *state);

void dp_psd_accumulate(dp_psd_state_t *state, const float _Complex *x,
                      size_t x_len);

void dp_psd_accumulate_real(dp_psd_state_t *state, const float *x, size_t x_len);

size_t dp_psd_power_twosided_max_out(dp_psd_state_t *state);

size_t dp_psd_power_twosided(dp_psd_state_t *state, size_t cap, float *out,
                          size_t max_out);

size_t dp_psd_power_onesided_max_out(dp_psd_state_t *state);

size_t dp_psd_power_onesided(dp_psd_state_t *state, size_t cap, float *out,
                          size_t max_out);

size_t dp_psd_psd_db_max_out(dp_psd_state_t *state);

size_t dp_psd_psd_db(dp_psd_state_t *state, size_t n, float *out,
                  size_t max_out);

size_t dp_psd_psd_dbhz_max_out(dp_psd_state_t *state);

size_t dp_psd_psd_dbhz(dp_psd_state_t *state, size_t n, float *out,
                    size_t max_out);

size_t dp_psd_band_power_max_out(dp_psd_state_t *state);

size_t dp_psd_band_power(dp_psd_state_t *state, const double *bands,
                        size_t bands_len, float *out, size_t max_out);

double dp_psd_total_band_power(dp_psd_state_t *state, const double *bands,
                              size_t bands_len);

double dp_psd_occupied_bw(dp_psd_state_t *state, double fraction);

double dp_psd_noise_floor(dp_psd_state_t *state);

double dp_psd_snr(dp_psd_state_t *state, double lo_hz, double hi_hz);

double dp_psd_sfdr(dp_psd_state_t *state, float min_db);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * delegates to the acc_trace power averager; window/plan/scratch
 * are config, rebuilt by create. */
#define PSD_STATE_MAGIC DP_FOURCC ('P','S','D',' ')
#define PSD_STATE_VERSION 1u
size_t dp_psd_state_bytes (const dp_psd_state_t *state);
void dp_psd_get_state (const dp_psd_state_t *state, void *blob);
int dp_psd_set_state (dp_psd_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* PSD_CORE_H */
```


