#include "i16u32_to_f32/i16u32_to_f32_core.h"

i16u32_to_f32_state_t *
i16u32_to_f32_create(float scale)
{
    if (scale <= 0.0f)
        return NULL;
    i16u32_to_f32_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return NULL;
    state->iscale = 1.0f / scale;
    return state;
}

void
i16u32_to_f32_destroy(i16u32_to_f32_state_t *state)
{
    free(state);
}

void
i16u32_to_f32_reset(i16u32_to_f32_state_t *state)
{
    (void)state;
}

void i16u32_to_f32_steps(
    i16u32_to_f32_state_t *state,
    const uint32_t    *input,
    float          *output,
    size_t               n)
{
    /* #pragma omp simd */
    for (size_t i = 0; i < n; i++)
        output[i] = i16u32_to_f32_step(state, input[i]);
}


