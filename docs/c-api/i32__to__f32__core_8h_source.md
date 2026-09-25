

# File i32\_to\_f32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**i32\_to\_f32**](dir_04852aba3b033ffe951e6f7f0437e86c.md) **>** [**i32\_to\_f32\_core.h**](i32__to__f32__core_8h.md)

[Go to the documentation of this file](i32__to__f32__core_8h.md)


```C++

#ifndef I32_TO_F32_CORE_H
#define I32_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i32_to_f32_state_t;

i32_to_f32_state_t *i32_to_f32_create(float scale);

void i32_to_f32_destroy(i32_to_f32_state_t *state);

void i32_to_f32_reset(i32_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
i32_to_f32_step(const i32_to_f32_state_t *state, int32_t x)
{
    return (float)x * state->iscale;
}

void i32_to_f32_steps(
    i32_to_f32_state_t *state,
    const int32_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I32_TO_F32_CORE_H */
```


