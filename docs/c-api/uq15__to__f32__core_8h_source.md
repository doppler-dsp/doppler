

# File uq15\_to\_f32\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**uq15\_to\_f32**](dir_b44b8aae78dd39801a4344596faf709f.md) **>** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md)

[Go to the documentation of this file](uq15__to__f32__core_8h.md)


```C++

#ifndef UQ15_TO_F32_CORE_H
#define UQ15_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} uq15_to_f32_state_t;

uq15_to_f32_state_t *uq15_to_f32_create(float scale);

void uq15_to_f32_destroy(uq15_to_f32_state_t *state);

void uq15_to_f32_reset(uq15_to_f32_state_t *state);

JM_FORCEINLINE JM_HOT float
uq15_to_f32_step(const uq15_to_f32_state_t *state, uint16_t x)
{
    /* Remove offset-binary bias in int32_t to avoid UB from int16 overflow */
    return (float)((int32_t)x - 32768) * state->iscale;
}

void uq15_to_f32_steps(
    uq15_to_f32_state_t *state,
    const uint16_t    *input,
    float          *output,
    size_t               n);

#ifdef __cplusplus
}
#endif

#endif /* UQ15_TO_F32_CORE_H */
```


