

# File f32\_to\_i16u64\_core.h

[**File List**](files.md) **>** [**f32\_to\_i16u64**](dir_212e21299d76aa740bbad8810e4bf50a.md) **>** [**f32\_to\_i16u64\_core.h**](f32__to__i16u64__core_8h.md)

[Go to the documentation of this file](f32__to__i16u64__core_8h.md)


```C++

#ifndef F32_TO_I16U64_CORE_H
#define F32_TO_I16U64_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float scale; /* multiply factor applied before saturation */
} f32_to_i16u64_state_t;

f32_to_i16u64_state_t *f32_to_i16u64_create(float scale);

void f32_to_i16u64_destroy(f32_to_i16u64_state_t *state);

void f32_to_i16u64_reset(f32_to_i16u64_state_t *state);

JM_FORCEINLINE JM_HOT uint64_t
f32_to_i16u64_step(const f32_to_i16u64_state_t *state, float x)
{
    /* Saturate to int16, then zero-extend into the lower 16 bits of uint64. */
    float s = state->scale * x;
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint64_t)(uint16_t)v;
}

void f32_to_i16u64_steps(
    f32_to_i16u64_state_t *state,
    const float    *input,
    uint64_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16U64_CORE_H */
```


