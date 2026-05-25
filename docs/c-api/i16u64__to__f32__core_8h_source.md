

# File i16u64\_to\_f32\_core.h

[**File List**](files.md) **>** [**i16u64\_to\_f32**](dir_8835689c72c9893bedb52cd5868912e0.md) **>** [**i16u64\_to\_f32\_core.h**](i16u64__to__f32__core_8h.md)

[Go to the documentation of this file](i16u64__to__f32__core_8h.md)


```C++

#ifndef I16U64_TO_F32_CORE_H
#define I16U64_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i16u64_to_f32_state_t;

i16u64_to_f32_state_t *i16u64_to_f32_create(float scale);

void i16u64_to_f32_destroy(i16u64_to_f32_state_t *state);

void i16u64_to_f32_reset(i16u64_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
i16u64_to_f32_step(const i16u64_to_f32_state_t *state, uint64_t x)
{
    /* Extract lower 16 bits as signed int16, then scale to float. */
    int16_t v = (int16_t)(uint16_t)(x & 0xFFFFull);
    return (float)v * state->iscale;
}

void i16u64_to_f32_steps(
    i16u64_to_f32_state_t *state,
    const uint64_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16U64_TO_F32_CORE_H */
```


