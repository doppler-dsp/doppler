

# File adc\_core.h

[**File List**](files.md) **>** [**adc**](dir_eddcd1edcea89729b407f545f79f2d08.md) **>** [**adc\_core.h**](adc__core_8h.md)

[Go to the documentation of this file](adc__core_8h.md)


```C++

#ifndef DP_ADC_CORE_H
#define DP_ADC_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double   scale;     
    int64_t  clip_max;  
    int64_t  clip_min;  
    int      bits;      
    float    dbfs;      
    int      dithering; 
    uint8_t  clipped;   
    uint32_t rng;       
} dp_adc_state_t;

dp_adc_state_t *dp_adc_create(int bits, float dbfs, int dithering);

void dp_adc_destroy(dp_adc_state_t *state);

void dp_adc_reset(dp_adc_state_t *state);

JM_FORCEINLINE JM_HOT int64_t
dp_adc_step(dp_adc_state_t *state, float x)
{
    /* jm: body sourced from [adc] impl/impl_file in objects/adc.toml — edit
     * there, not here; `jm apply` overwrites this. */
        double s = (double)x * state->scale;
        if (state->dithering) {
            /* TPDF dither: two SUCCESSIVE xorshift32 draws summed →
             * triangular PDF over [-1, +1] LSB.  Each uniform is in
             * [-0.5, +0.5] LSB.  The second is a draw of its own: a multiply
             * of the first is one variable, not two (#2651). */
            uint32_t r = state->rng;
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            uint32_t r1 = r;
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            state->rng = r;
            s += ((double)(int32_t)r1 + (double)(int32_t)r)
                 * (1.0 / 4294967296.0);
        }
        state->clipped |= (uint8_t)(s > (double)state->clip_max
                                  || s < (double)state->clip_min);
        int64_t v = llround(s);
        if (v > state->clip_max) v = state->clip_max;
        if (v < state->clip_min) v = state->clip_min;
        return (int64_t)v;
}

void dp_adc_steps(
    dp_adc_state_t       *state,
    const float       *input,
    int64_t           *output,
    size_t             n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the dither RNG + sticky clip flag resume exactly into an
 * identically-built instance. */
#define ADC_STATE_MAGIC DP_FOURCC ('A','D','C',' ')
#define ADC_STATE_VERSION 1u
size_t dp_adc_state_bytes (const dp_adc_state_t *state);
void dp_adc_get_state (const dp_adc_state_t *state, void *blob);
int dp_adc_set_state (dp_adc_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CORE_H */
```


