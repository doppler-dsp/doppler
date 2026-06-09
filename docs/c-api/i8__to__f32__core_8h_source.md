

# File i8\_to\_f32\_core.h

[**File List**](files.md) **>** [**i8\_to\_f32**](dir_fd8e995fbd9a7d674714f99e992f90b2.md) **>** [**i8\_to\_f32\_core.h**](i8__to__f32__core_8h.md)

[Go to the documentation of this file](i8__to__f32__core_8h.md)


```C++

#ifndef I8_TO_F32_CORE_H
#define I8_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i8_to_f32_state_t;

i8_to_f32_state_t *i8_to_f32_create(float scale);

void i8_to_f32_destroy(i8_to_f32_state_t *state);

void i8_to_f32_reset(i8_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
i8_to_f32_step(const i8_to_f32_state_t *state, int8_t x)
{
    return (float)x * state->iscale;
}

void i8_to_f32_steps(
    i8_to_f32_state_t *state,
    const int8_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I8_TO_F32_CORE_H */
```


