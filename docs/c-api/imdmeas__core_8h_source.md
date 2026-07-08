

# File imdmeas\_core.h

[**File List**](files.md) **>** [**imdmeas**](dir_2f7e0f9e46c443ab8712f0318288e016.md) **>** [**imdmeas\_core.h**](imdmeas__core_8h.md)

[Go to the documentation of this file](imdmeas__core_8h.md)


```C++

#ifndef IMDMEAS_CORE_H
#define IMDMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "psd/psd_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    psd_state_t *psd;     /* shared averaging PSD core (window+FFT+avg)   */
    float         *pwr;     /* metric working buffer, one-sided power       */
    double enbw;            /* window equivalent noise bandwidth (bins)     */
    double beta;            /* auto-selected Kaiser shape (from DR target)   */
    size_t lobe_bins;       /* main-lobe half-width L, for power integration*/
    size_t spur_guard_bins; /* tone keep-out for the two-tone search (>= L) */
    size_t n;               /* capture / frame length                      */
    size_t nfft;            /* zero-padded transform length                */
    double fs;              /* sample rate (Hz)                            */
} imdmeas_state_t;

imdmeas_state_t *imdmeas_create(size_t n, double fs, double full_scale,
                                size_t bits, double dynamic_range_db);

void imdmeas_destroy(imdmeas_state_t *state);

void imdmeas_reset(imdmeas_state_t *state);

imd_meas_t imdmeas_analyze(imdmeas_state_t *state, const float *x, size_t n_in);

size_t imdmeas_spectrum_dbfs_max_out(imdmeas_state_t *state);

size_t imdmeas_spectrum_dbfs(imdmeas_state_t *state, const float *x,
                             size_t x_len, float *out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
```


