#include "f32_to_i16/f32_to_i16_core.h"

f32_to_i16_state_t *
f32_to_i16_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  f32_to_i16_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->scale = scale;
  return state;
}

void
f32_to_i16_destroy (f32_to_i16_state_t *state)
{
  free (state);
}

void
f32_to_i16_reset (f32_to_i16_state_t *state)
{
  state->clipped = 0;
}

void
f32_to_i16_steps (f32_to_i16_state_t *state, const float *input,
                  int16_t *output, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = f32_to_i16_step (state, input[i]);
}
