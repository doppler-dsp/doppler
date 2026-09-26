

# File uq15\_to\_f32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**uq15\_to\_f32**](dir_289e6f8543a5d0b92e78da373782efe4.md) **>** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md)

[Go to the documentation of this file](uq15__to__f32__core_8h.md)


```C++

#ifndef DP_UQ15_TO_F32_CORE_H
#define DP_UQ15_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} dp_uq15_to_f32_state_t;

dp_uq15_to_f32_state_t *dp_uq15_to_f32_create(float scale);

void dp_uq15_to_f32_destroy(dp_uq15_to_f32_state_t *state);

void dp_uq15_to_f32_reset(dp_uq15_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
dp_uq15_to_f32_step(const dp_uq15_to_f32_state_t *state, uint16_t x)
{
    /* Remove offset-binary bias in int32_t to avoid UB from int16 overflow */
    return (float)((int32_t)x - 32768) * state->iscale;
}

void dp_uq15_to_f32_steps(
    dp_uq15_to_f32_state_t *state,
    const uint16_t    *input,
    float          *output,
    size_t               n);

#ifdef __cplusplus
}
#endif

#endif /* UQ15_TO_F32_CORE_H */
```


