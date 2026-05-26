

# File f32\_to\_i16u32\_core.h

[**File List**](files.md) **>** [**f32\_to\_i16u32**](dir_5361bfc3c658147f85e2e18e4bfef9b4.md) **>** [**f32\_to\_i16u32\_core.h**](f32__to__i16u32__core_8h.md)

[Go to the documentation of this file](f32__to__i16u32__core_8h.md)


```C++

#ifndef F32_TO_I16U32_CORE_H
#define F32_TO_I16U32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_i16u32_state_t;

f32_to_i16u32_state_t *f32_to_i16u32_create(float scale);

void f32_to_i16u32_destroy(f32_to_i16u32_state_t *state);

void f32_to_i16u32_reset(f32_to_i16u32_state_t *state);

JM_FORCEINLINE JM_HOT uint32_t
f32_to_i16u32_step(f32_to_i16u32_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint32_t)(uint16_t)v;
}

void f32_to_i16u32_steps(
    f32_to_i16u32_state_t *state,
    const float    *input,
    uint32_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16U32_CORE_H */
```


