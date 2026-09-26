

# File i16u32\_to\_f32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**i16u32\_to\_f32**](dir_c1ecd6bb977db755472db2284b0a6806.md) **>** [**i16u32\_to\_f32\_core.h**](i16u32__to__f32__core_8h.md)

[Go to the documentation of this file](i16u32__to__f32__core_8h.md)


```C++

#ifndef DP_I16U32_TO_F32_CORE_H
#define DP_I16U32_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} dp_i16u32_to_f32_state_t;

dp_i16u32_to_f32_state_t *dp_i16u32_to_f32_create(float scale);

void dp_i16u32_to_f32_destroy(dp_i16u32_to_f32_state_t *state);

void dp_i16u32_to_f32_reset(dp_i16u32_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
dp_i16u32_to_f32_step(const dp_i16u32_to_f32_state_t *state, uint32_t x)
{
    /* Extract lower 16 bits as signed int16, then scale to float. */
    int16_t v = (int16_t)(uint16_t)(x & 0xFFFFu);
    return (float)v * state->iscale;
}

void dp_i16u32_to_f32_steps(
    dp_i16u32_to_f32_state_t *state,
    const uint32_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16U32_TO_F32_CORE_H */
```


