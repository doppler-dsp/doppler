

# File tonemeas\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**tonemeas**](dir_78c9bf326243d2be956f1c1b5de2ee56.md) **>** [**tonemeas\_core.h**](tonemeas__core_8h.md)

[Go to the documentation of this file](tonemeas__core_8h.md)


```C++

#ifndef TONEMEAS_CORE_H
#define TONEMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "psd/psd_core.h"
#include <complex.h>
#include "fft/fft_core.h"
#include "spectral/spectral_core.h"
#include "acc_trace/acc_trace_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg)   */
    float         *pwr;     /* metric working buffer, length nfft          */
    unsigned char *excl;    /* DC/fundamental/harmonic exclusion mask      */
    double enbw;            /* window equivalent noise bandwidth (bins)    */
    double beta;            /* auto-selected Kaiser shape (from DR target)  */
    size_t lobe_bins;       /* main-lobe half-width L, for power integration*/
    size_t spur_guard_bins; /* fundamental keep-out for spur search (>= L)  */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    size_t n_harm;          /* harmonics tracked (k = 2..n_harm)           */
    double fs;              /* sample rate (Hz)                            */
    size_t dc_guard;        /* extra bins excluded beyond L around DC      */
} tonemeas_state_t;

tonemeas_state_t *tonemeas_create(size_t n, double fs, size_t n_harmonics,
                                  double full_scale, size_t bits,
                                  double dynamic_range_db, size_t dc_guard);

void tonemeas_destroy(tonemeas_state_t *state);

void tonemeas_reset(tonemeas_state_t *state);

tone_meas_t tonemeas_analyze(tonemeas_state_t *state, const float *x,
                             size_t n_in);

tone_meas_t tonemeas_analyze_complex(tonemeas_state_t *state,
                                     const float complex *x, size_t n_in);

time_stats_t tonemeas_time_stats(tonemeas_state_t *state, const float *x,
                                 size_t n_in);

size_t tonemeas_spectrum_dbfs_max_out(tonemeas_state_t *state);

size_t tonemeas_spectrum_dbfs(tonemeas_state_t *state, const float *x,
                              size_t x_len, float *out);

#ifdef __cplusplus
}
#endif

#endif /* TONEMEAS_CORE_H */
```


