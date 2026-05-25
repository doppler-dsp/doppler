

# File i16\_to\_f32\_core.h

[**File List**](files.md) **>** [**i16\_to\_f32**](dir_5ec56354373793af7b5bc8e9296f5472.md) **>** [**i16\_to\_f32\_core.h**](i16__to__f32__core_8h.md)

[Go to the documentation of this file](i16__to__f32__core_8h.md)


```C++

#ifndef I16_TO_F32_CORE_H
#define I16_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i16_to_f32_state_t;

i16_to_f32_state_t *i16_to_f32_create(float scale);

void i16_to_f32_destroy(i16_to_f32_state_t *state);

void i16_to_f32_reset(i16_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
i16_to_f32_step(const i16_to_f32_state_t *state, int16_t x)
{
    return (float)x * state->iscale;
}

void i16_to_f32_steps(
    i16_to_f32_state_t *state,
    const int16_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16_TO_F32_CORE_H */
```


