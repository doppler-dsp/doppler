

# File f32\_to\_i8\_core.h

[**File List**](files.md) **>** [**f32\_to\_i8**](dir_b1f46cddbee3624386fd88f96d7cfb35.md) **>** [**f32\_to\_i8\_core.h**](f32__to__i8__core_8h.md)

[Go to the documentation of this file](f32__to__i8__core_8h.md)


```C++

#ifndef F32_TO_I8_CORE_H
#define F32_TO_I8_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_i8_state_t;

f32_to_i8_state_t *f32_to_i8_create(float scale);

void f32_to_i8_destroy(f32_to_i8_state_t *state);

void f32_to_i8_reset(f32_to_i8_state_t *state);

JM_FORCEINLINE JM_HOT int8_t
f32_to_i8_step(f32_to_i8_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 127.0f || s < -128.0f);
    s = fmaxf(s, -128.0f);
    s = fminf(s,  127.0f);
    return (int8_t)lroundf(s);
}

void f32_to_i8_steps(
    f32_to_i8_state_t *state,
    const float    *input,
    int8_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
#define F32_TO_I8_STATE_MAGIC DP_FOURCC ('F','2','_','8')
#define F32_TO_I8_STATE_VERSION 1u
size_t f32_to_i8_state_bytes (const f32_to_i8_state_t *state);
void f32_to_i8_get_state (const f32_to_i8_state_t *state, void *blob);
int f32_to_i8_set_state (f32_to_i8_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I8_CORE_H */
```


