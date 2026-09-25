

# File nprmeas\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**nprmeas**](dir_df189228e030028408da81bd0afea7e3.md) **>** [**nprmeas\_core.h**](nprmeas__core_8h.md)

[Go to the documentation of this file](nprmeas__core_8h.md)


```C++

#ifndef NPRMEAS_CORE_H
#define NPRMEAS_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/measure/measure_core.h"
#include "doppler/psd/psd_core.h"
#include "doppler/dp_complex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg) */
    float         *pwr;     /* metric working buffer, one-sided power     */
    double enbw;            /* window equivalent noise bandwidth (bins)   */
    double beta;            /* auto-selected Kaiser shape (from DR target) */
    size_t spur_guard_bins; /* min notch keep-out (bins) from window skirt */
    size_t n;               
    size_t nfft;            
    double fs;              
} nprmeas_state_t;

nprmeas_state_t *nprmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

void nprmeas_destroy(nprmeas_state_t *state);

void nprmeas_reset(nprmeas_state_t *state);

npr_meas_t nprmeas_analyze(nprmeas_state_t *state, const float *x, size_t n_in,
                           double active_lo, double active_hi, double notch_lo,
                           double notch_hi, double guard_hz);

size_t nprmeas_spectrum_dbfs_max_out(nprmeas_state_t *state);

size_t nprmeas_spectrum_dbfs(nprmeas_state_t *state, const float *x,
                             size_t x_len, float *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NPRMEAS_CORE_H */
```


