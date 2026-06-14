

# File imdmeas\_core.h

[**File List**](files.md) **>** [**imdmeas**](dir_2f7e0f9e46c443ab8712f0318288e016.md) **>** [**imdmeas\_core.h**](imdmeas__core_8h.md)

[Go to the documentation of this file](imdmeas__core_8h.md)


```C++

#ifndef IMDMEAS_CORE_H
#define IMDMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "fft/fft_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    fft_state_t   *fft;
    float         *w;
    float complex *frame;
    float complex *spec;
    float         *pwr;
    double cg;
    double s2;
    double enbw;
    size_t lobe_bins;
    size_t n;
    size_t nfft;
    double fs;
    double full_scale;
} imdmeas_state_t;

imdmeas_state_t *imdmeas_create(size_t n, double fs, int window, float beta,
                                size_t pad, double full_scale);

void imdmeas_destroy(imdmeas_state_t *state);

void imdmeas_reset(imdmeas_state_t *state);

size_t imdmeas_analyze(imdmeas_state_t *state, const float *x, size_t n_in,
                       imd_meas_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* IMDMEAS_CORE_H */
```


