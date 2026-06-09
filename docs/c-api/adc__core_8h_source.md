

# File adc\_core.h

[**File List**](files.md) **>** [**adc**](dir_a6be6b8cb61d5f2be55c0b2f94afbd88.md) **>** [**adc\_core.h**](adc__core_8h.md)

[Go to the documentation of this file](adc__core_8h.md)


```C++

#ifndef ADC_CORE_H
#define ADC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double   scale;     /* 2^(bits-1) * 10^(-dbfs/20) */
    int64_t  clip_max;  /* 2^(bits-1) - 1              */
    int64_t  clip_min;  /* -2^(bits-1)                 */
    int      bits;      /* ADC resolution, 1..64        */
    float    dbfs;      /* full-scale reference level   */
    int      dithering; /* 0 = off; non-zero = TPDF     */
    uint8_t  clipped;   /* sticky saturation flag       */
    uint32_t rng;       /* xorshift32 PRNG state        */
} adc_state_t;

adc_state_t *adc_create(int bits, float dbfs, int dithering);

void adc_destroy(adc_state_t *state);

void adc_reset(adc_state_t *state);

JM_FORCEINLINE JM_HOT int64_t
adc_step(adc_state_t *state, float x)
{
        double s = (double)x * state->scale;
        if (state->dithering) {
            /* TPDF dither: two xorshift32 draws summed → triangular PDF
             * over [-1, +1] LSB.  Each uniform is in [-0.5, +0.5] LSB. */
            state->rng ^= state->rng << 13;
            state->rng ^= state->rng >> 17;
            state->rng ^= state->rng << 5;
            uint32_t r2 = state->rng * 2654435761u;
            s += ((double)(int32_t)state->rng + (double)(int32_t)r2)
                 * (1.0 / 4294967296.0);
        }
        state->clipped |= (uint8_t)(s > (double)state->clip_max
                                  || s < (double)state->clip_min);
        int64_t v = llround(s);
        if (v > state->clip_max) v = state->clip_max;
        if (v < state->clip_min) v = state->clip_min;
        return (int64_t)v;
}

void adc_steps(
    adc_state_t       *state,
    const float       *input,
    int64_t           *output,
    size_t             n);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CORE_H */
```


