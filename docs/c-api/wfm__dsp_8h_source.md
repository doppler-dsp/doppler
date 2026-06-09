

# File wfm\_dsp.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfm\_dsp.h**](wfm__dsp_8h.md)

[Go to the documentation of this file](wfm__dsp_8h.md)


```C++

#ifndef WFM_DSP_H
#define WFM_DSP_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline size_t
wfm_rrc_ntaps(int sps, int span)
{
    return (size_t)(2 * span * sps + 1);
}

void wfm_rrc_taps(double beta, int sps, int span, float *taps);

void wfm_dsss_spread(const float _Complex *syms, size_t n_sym,
                     const uint8_t *code, size_t sf, float _Complex *out);

#ifdef __cplusplus
}
#endif

#endif /* WFM_DSP_H */
```


