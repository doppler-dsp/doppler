#include "doppler/f32_to_i16/f32_to_i16_core.h"

dp_f32_to_i16_state_t *
dp_f32_to_i16_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  dp_f32_to_i16_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->scale = scale;
  return state;
}

void
dp_f32_to_i16_destroy (dp_f32_to_i16_state_t *state)
{
  free (state);
}

void
dp_f32_to_i16_reset (dp_f32_to_i16_state_t *state)
{
  state->clipped = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_f32_to_i16, dp_f32_to_i16_state_t,
                     F32_TO_I16_STATE_MAGIC, F32_TO_I16_STATE_VERSION)

void
dp_f32_to_i16_steps (dp_f32_to_i16_state_t *state, const float *input,
                     int16_t *output, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = dp_f32_to_i16_step (state, input[i]);
}
