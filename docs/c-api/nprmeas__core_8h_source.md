

# File nprmeas\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nprmeas**](dir_2ffe7a00bca5d7665b823d0b8c1040c3.md) **>** [**nprmeas\_core.h**](nprmeas__core_8h.md)

[Go to the documentation of this file](nprmeas__core_8h.md)


```C++

#ifndef NPRMEAS_CORE_H
#define NPRMEAS_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "measure/measure_core.h"
#include "welch/welch_core.h"
#include <complex.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    welch_state_t *psd;     /* shared averaging PSD core (window+FFT+avg) */
    float         *pwr;     /* metric working buffer, one-sided power     */
    double enbw;            /* window equivalent noise bandwidth (bins)   */
    size_t n;               /* capture / frame length                     */
    size_t nfft;            /* zero-padded transform length               */
    double fs;              /* sample rate (Hz)                           */
    double full_scale;      /* amplitude that equals 0 dBFS               */
} nprmeas_state_t;

nprmeas_state_t *nprmeas_create(size_t n, double fs, int window, float beta,
                                size_t pad, double full_scale);

void nprmeas_destroy(nprmeas_state_t *state);

void nprmeas_reset(nprmeas_state_t *state);

npr_meas_t nprmeas_analyze(nprmeas_state_t *state, const float *x, size_t n_in,
                           double active_lo, double active_hi, double notch_lo,
                           double notch_hi, double guard_hz);

#ifdef __cplusplus
}
#endif

#endif /* NPRMEAS_CORE_H */
```


