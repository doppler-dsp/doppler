

# File f32\_to\_i16\_core.h

[**File List**](files.md) **>** [**f32\_to\_i16**](dir_e25c96329f88166d8f87eefdc2ba64fa.md) **>** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md)

[Go to the documentation of this file](f32__to__i16__core_8h.md)


```C++

#ifndef F32_TO_I16_CORE_H
#define F32_TO_I16_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float scale; /* multiply factor applied before saturation */
} f32_to_i16_state_t;

f32_to_i16_state_t *f32_to_i16_create(float scale);

void f32_to_i16_destroy(f32_to_i16_state_t *state);

void f32_to_i16_reset(f32_to_i16_state_t *state);

JM_FORCEINLINE JM_HOT int16_t
f32_to_i16_step(const f32_to_i16_state_t *state, float x)
{
    /* Scale, round-to-nearest, then saturate to int16 range. */
    float s = state->scale * x;
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    return (int16_t)lroundf(s);
}

void f32_to_i16_steps(
    f32_to_i16_state_t *state,
    const float    *input,
    int16_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16_CORE_H */
```


