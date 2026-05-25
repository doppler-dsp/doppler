

# File i16u32\_to\_f32\_core.h

[**File List**](files.md) **>** [**i16u32\_to\_f32**](dir_a216b988e44f4b34f41ebc1122731aa5.md) **>** [**i16u32\_to\_f32\_core.h**](i16u32__to__f32__core_8h.md)

[Go to the documentation of this file](i16u32__to__f32__core_8h.md)


```C++

#ifndef I16U32_TO_F32_CORE_H
#define I16U32_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i16u32_to_f32_state_t;

i16u32_to_f32_state_t *i16u32_to_f32_create(float scale);

void i16u32_to_f32_destroy(i16u32_to_f32_state_t *state);

void i16u32_to_f32_reset(i16u32_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
i16u32_to_f32_step(const i16u32_to_f32_state_t *state, uint32_t x)
{
    /* Extract lower 16 bits as signed int16, then scale to float. */
    int16_t v = (int16_t)(uint16_t)(x & 0xFFFFu);
    return (float)v * state->iscale;
}

void i16u32_to_f32_steps(
    i16u32_to_f32_state_t *state,
    const uint32_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16U32_TO_F32_CORE_H */
```


