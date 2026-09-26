

# File i16\_to\_f32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**i16\_to\_f32**](dir_01f347c62069c5233a090c3b4d985bee.md) **>** [**i16\_to\_f32\_core.h**](i16__to__f32__core_8h.md)

[Go to the documentation of this file](i16__to__f32__core_8h.md)


```C++

#ifndef DP_I16_TO_F32_CORE_H
#define DP_I16_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} dp_i16_to_f32_state_t;

dp_i16_to_f32_state_t *dp_i16_to_f32_create(float scale);

void dp_i16_to_f32_destroy(dp_i16_to_f32_state_t *state);

void dp_i16_to_f32_reset(dp_i16_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
dp_i16_to_f32_step(const dp_i16_to_f32_state_t *state, int16_t x)
{
    return (float)x * state->iscale;
}

void dp_i16_to_f32_steps(
    dp_i16_to_f32_state_t *state,
    const int16_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16_TO_F32_CORE_H */
```


