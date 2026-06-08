#include "i32_to_f32/i32_to_f32_core.h"

i32_to_f32_state_t *
i32_to_f32_create(float scale)
{
    if (scale <= 0.0f)
        return NULL;
    i32_to_f32_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->iscale = 1.0f / scale;
    return obj;
}

void
i32_to_f32_destroy(i32_to_f32_state_t *state)
{
    free(state);
}

void
i32_to_f32_reset(i32_to_f32_state_t *state)
{
    (void)state; /* no dynamic state to reset */
}

void i32_to_f32_steps(
    i32_to_f32_state_t *state,
    const int32_t    *input,
    float          *output,
    size_t               n)
{
    /* #pragma omp simd */
    for (size_t i = 0; i < n; i++)
        output[i] = i32_to_f32_step(state, input[i]);
}


