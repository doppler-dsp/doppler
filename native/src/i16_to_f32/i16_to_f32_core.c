#include "i16_to_f32/i16_to_f32_core.h"

i16_to_f32_state_t *
i16_to_f32_create(float scale)
{
    if (scale <= 0.0f)
        return NULL;
    i16_to_f32_state_t *state = calloc(1, sizeof(*state));
    if (!state)
        return NULL;
    state->iscale = 1.0f / scale;
    return state;
}

void
i16_to_f32_destroy(i16_to_f32_state_t *state)
{
    free(state);
}

void
i16_to_f32_reset(i16_to_f32_state_t *state)
{
    (void)state; /* no dynamic state to reset */
}

void i16_to_f32_steps(
    i16_to_f32_state_t *state,
    const int16_t    *input,
    float          *output,
    size_t               n)
{
    /* #pragma omp simd */
    for (size_t i = 0; i < n; i++)
        output[i] = i16_to_f32_step(state, input[i]);
}


