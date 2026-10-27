

# File imdmeas\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**imdmeas**](dir_2a8d4e9dde298cc63e616d81cd7ff06c.md) **>** [**imdmeas\_core.h**](imdmeas__core_8h.md)

[Go to the documentation of this file](imdmeas__core_8h.md)


```C++

#ifndef DP_IMDMEAS_CORE_H
#define DP_IMDMEAS_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/measure/measure_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/dp_complex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dp_psd_state_t *psd;     
    float         *pwr;     
    double enbw;            
    double beta;            
    size_t lobe_bins;       
    size_t spur_guard_bins; 
    size_t n;               
    size_t nfft;            
    double fs;              
} dp_imdmeas_state_t;

dp_imdmeas_state_t *dp_imdmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

void dp_imdmeas_destroy(dp_imdmeas_state_t *state);

void dp_imdmeas_reset(dp_imdmeas_state_t *state);

imd_meas_t dp_imdmeas_analyze(dp_imdmeas_state_t *state, const float *x, size_t n_in);

size_t dp_imdmeas_spectrum_dbfs_max_out(dp_imdmeas_state_t *state);

size_t dp_imdmeas_spectrum_dbfs(dp_imdmeas_state_t *state, const float *x,
                             size_t x_len, float *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
```


