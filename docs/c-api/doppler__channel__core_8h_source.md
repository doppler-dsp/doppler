

# File doppler\_channel\_core.h

[**File List**](files.md) **>** [**doppler\_channel**](dir_597302de6cb0d177e5e89858f2abab7e.md) **>** [**doppler\_channel\_core.h**](doppler__channel__core_8h.md)

[Go to the documentation of this file](doppler__channel__core_8h.md)


```C++

#ifndef DOPPLER_CHANNEL_CORE_H
#define DOPPLER_CHANNEL_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "resamp/resamp_core.h"
#ifdef __cplusplus
extern "C" {
#endif

#define DOPPLER_CHANNEL_STATE_MAGIC DP_FOURCC('D', 'P', 'C', 'H')
#define DOPPLER_CHANNEL_STATE_VERSION 1u

#define DOPPLER_CHANNEL_MAX_BLOCK 65536u

typedef struct {
    double fs;                 /* receive sample rate, Hz                  */
    double carrier_hz;         /* RF carrier fc, Hz — drives the offset    */
    double doppler_ppm;        /* d0, ppm of nominal                       */
    double doppler_rate_ppm_s; /* d-dot, ppm/s                             */

    resamp_state_t *rs; /* the dilation — never hand-rolled here     */

    /* Two separate clocks, on purpose. The resampler's per-sample rate
       deviation is indexed by INPUT sample; the carrier phase is a function of
       receive time, which is the OUTPUT sample index. They differ by the
       dilation itself, so conflating them would fold a second copy of the
       Doppler into the carrier. */
    uint64_t n_in;  /* input samples consumed                    */
    uint64_t n_out; /* output samples produced                   */

    double *ctrl;         /* per-sample rate deviation scratch         */
    size_t ctrl_cap;
} doppler_channel_state_t;

static inline double
doppler_channel_excess(const doppler_channel_state_t *s, double t)
{
    return (s->doppler_ppm * t + 0.5 * s->doppler_rate_ppm_s * t * t) * 1e-6;
}

static inline double
doppler_channel_scale(const doppler_channel_state_t *s, double t)
{
    return 1.0 + (s->doppler_ppm + s->doppler_rate_ppm_s * t) * 1e-6;
}

static inline double
doppler_channel_phase(const doppler_channel_state_t *s, double t)
{
    return s->carrier_hz * doppler_channel_excess(s, t);
}

doppler_channel_state_t *doppler_channel_create(double fs, double carrier_hz, double doppler_ppm, double doppler_rate_ppm_s);

void doppler_channel_destroy(doppler_channel_state_t *state);

void doppler_channel_reset(doppler_channel_state_t *state);

size_t doppler_channel_state_bytes(const doppler_channel_state_t *state);

void doppler_channel_get_state(const doppler_channel_state_t *state, void *blob);

int doppler_channel_set_state(doppler_channel_state_t *state, const void *blob);

size_t doppler_channel_execute_max_out(doppler_channel_state_t *state);

size_t doppler_channel_execute(doppler_channel_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);

double doppler_channel_get_elapsed_s(const doppler_channel_state_t *state);

double doppler_channel_get_offset_hz(const doppler_channel_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_CHANNEL_CORE_H */
```


