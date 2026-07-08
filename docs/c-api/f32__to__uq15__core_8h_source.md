

# File f32\_to\_uq15\_core.h

[**File List**](files.md) **>** [**f32\_to\_uq15**](dir_4e8c99e54919bb49218552fb8f2fb678.md) **>** [**f32\_to\_uq15\_core.h**](f32__to__uq15__core_8h.md)

[Go to the documentation of this file](f32__to__uq15__core_8h.md)


```C++

#ifndef F32_TO_UQ15_CORE_H
#define F32_TO_UQ15_CORE_H

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
} f32_to_uq15_state_t;

f32_to_uq15_state_t *f32_to_uq15_create(float scale);

void f32_to_uq15_destroy(f32_to_uq15_state_t *state);

void f32_to_uq15_reset(f32_to_uq15_state_t *state);

JM_FORCEINLINE JM_HOT uint16_t
f32_to_uq15_step(f32_to_uq15_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    /* Shift Q15 to offset-binary: 0 → 32768, -32768 → 0, 32767 → 65535 */
    return (uint16_t)((int32_t)v + 32768);
}

void f32_to_uq15_steps(
    f32_to_uq15_state_t *state,
    const float    *input,
    uint16_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
#define F32_TO_UQ15_STATE_MAGIC DP_FOURCC ('F','U','1','5')
#define F32_TO_UQ15_STATE_VERSION 1u
size_t f32_to_uq15_state_bytes (const f32_to_uq15_state_t *state);
void f32_to_uq15_get_state (const f32_to_uq15_state_t *state, void *blob);
int f32_to_uq15_set_state (f32_to_uq15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_UQ15_CORE_H */
```


