

# File f32\_to\_i32\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**f32\_to\_i32**](dir_b3babbbd6f70adf16ee9e6e7966b639c.md) **>** [**f32\_to\_i32\_core.h**](f32__to__i32__core_8h.md)

[Go to the documentation of this file](f32__to__i32__core_8h.md)


```C++

#ifndef DP_F32_TO_I32_CORE_H
#define DP_F32_TO_I32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   scale;   
    uint8_t clipped; 
} dp_f32_to_i32_state_t;

dp_f32_to_i32_state_t *dp_f32_to_i32_create(float scale);

void dp_f32_to_i32_destroy(dp_f32_to_i32_state_t *state);

void dp_f32_to_i32_reset(dp_f32_to_i32_state_t *state);

JM_FORCEINLINE JM_HOT int32_t
dp_f32_to_i32_step(dp_f32_to_i32_state_t *state, float x)
{
    double s = (double)state->scale * (double)x;
    /* Detect saturation before clamping; set sticky flag. */
    /* Written as "not inside" so a NaN, which fails every comparison,
     * latches the flag too; the lower clamp is spelled the same way so a
     * NaN leaves as the negative full-scale code rather than reaching
     * lround(), where it has no defined result. */
    state->clipped |= (uint8_t)!(s <= 2147483647.0 && s >= -2147483648.0);
    if (s > 2147483647.0)
        s = 2147483647.0;
    if (!(s >= -2147483648.0))
        s = -2147483648.0;
    return (int32_t)lround(s);
}

void dp_f32_to_i32_steps(
    dp_f32_to_i32_state_t *state,
    const float    *input,
    int32_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
#define F32_TO_I32_STATE_MAGIC DP_FOURCC ('F','2','3','2')
#define F32_TO_I32_STATE_VERSION 1u
size_t dp_f32_to_i32_state_bytes (const dp_f32_to_i32_state_t *state);
void dp_f32_to_i32_get_state (const dp_f32_to_i32_state_t *state, void *blob);
int dp_f32_to_i32_set_state (dp_f32_to_i32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I32_CORE_H */
```


