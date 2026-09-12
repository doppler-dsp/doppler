

# File f32\_to\_i32\_core.h

[**File List**](files.md) **>** [**f32\_to\_i32**](dir_9f277c348fdc2d73ff85df72003f099b.md) **>** [**f32\_to\_i32\_core.h**](f32__to__i32__core_8h.md)

[Go to the documentation of this file](f32__to__i32__core_8h.md)


```C++

#ifndef F32_TO_I32_CORE_H
#define F32_TO_I32_CORE_H

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
} f32_to_i32_state_t;

f32_to_i32_state_t *f32_to_i32_create(float scale);

void f32_to_i32_destroy(f32_to_i32_state_t *state);

void f32_to_i32_reset(f32_to_i32_state_t *state);

JM_FORCEINLINE JM_HOT int32_t
f32_to_i32_step(f32_to_i32_state_t *state, float x)
{
    double s = (double)state->scale * (double)x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 2147483647.0 || s < -2147483648.0);
    if (s > 2147483647.0)
        s = 2147483647.0;
    if (s < -2147483648.0)
        s = -2147483648.0;
    return (int32_t)lround(s);
}

void f32_to_i32_steps(
    f32_to_i32_state_t *state,
    const float    *input,
    int32_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
#define F32_TO_I32_STATE_MAGIC DP_FOURCC ('F','2','3','2')
#define F32_TO_I32_STATE_VERSION 1u
size_t f32_to_i32_state_bytes (const f32_to_i32_state_t *state);
void f32_to_i32_get_state (const f32_to_i32_state_t *state, void *blob);
int f32_to_i32_set_state (f32_to_i32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I32_CORE_H */
```


