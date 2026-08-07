

# File wfm\_dsp.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the documentation of this file](wfm__dsp_8h.md)


```C++

#ifndef WFM_DSP_H
#define WFM_DSP_H

#include "clib_common.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t
wfm_rrc_ntaps(int sps, int span)
{
    return (size_t)(2 * span * sps + 1);
}

static inline double
wfm_rrc_h(double t, double beta)
{
    if (fabs(t) < 1e-9)
        /* limit at t = 0 */
        return 1.0 - beta + 4.0 * beta / M_PI;
    if (beta > 0.0 && fabs(fabs(t) - 1.0 / (4.0 * beta)) < 1e-9)
    {
        /* limit at t = ±1/(4β) (0/0 in the general form) */
        double a = M_PI / (4.0 * beta);
        return (beta / sqrt(2.0))
               * ((1.0 + 2.0 / M_PI) * sin(a) + (1.0 - 2.0 / M_PI) * cos(a));
    }
    double pt  = M_PI * t;
    double num = sin(pt * (1.0 - beta)) + 4.0 * beta * t * cos(pt * (1.0 + beta));
    double den = pt * (1.0 - (4.0 * beta * t) * (4.0 * beta * t));
    return num / den;
}

void wfm_rrc_taps(double beta, int sps, int span, float *taps);

static inline size_t
wfm_rrc_bank_ntaps(int span)
{
    return (size_t)(2 * span + 1);
}

void wfm_polyphase_bank(const float *proto, size_t proto_len,
                        size_t num_phases, size_t num_taps, float *bank);

void wfm_rrc_polyphase_bank(double beta, int sps, int span, float *bank);

void wfm_dsss_spread(const float _Complex *syms, size_t n_sym,
                     const uint8_t *code, size_t sf, float _Complex *out);

size_t wfm_frame_dsss_nchips(size_t acq_len, size_t acq_reps, size_t data_len,
                             size_t sync_len, size_t payload_len, int crc);

size_t wfm_frame_dsss_chips(const uint8_t *acq_code, size_t acq_len,
                            size_t acq_reps, const uint8_t *data_code,
                            size_t data_len, const uint8_t *sync,
                            size_t sync_len, const uint8_t *payload,
                            size_t payload_len, int crc, uint8_t *out);

static inline size_t
wfm_cont_dsss_nchips(size_t n_chips)
{
    return n_chips;
}

size_t wfm_cont_dsss_chips(const uint8_t *code, size_t code_len,
                           const uint8_t *data, size_t n_data,
                           double chips_per_symbol, size_t n_chips,
                           uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DSP_H */
```


