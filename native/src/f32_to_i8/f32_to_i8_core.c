#include "f32_to_i8/f32_to_i8_core.h"

f32_to_i8_state_t *
f32_to_i8_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  f32_to_i8_state_t *state = dp_xcalloc (1, sizeof (*state));
  state->scale             = scale;
  return state;
}

void
f32_to_i8_destroy (f32_to_i8_state_t *state)
{
  free (state);
}

void
f32_to_i8_reset (f32_to_i8_state_t *state)
{
  state->clipped = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (f32_to_i8, f32_to_i8_state_t, F32_TO_I8_STATE_MAGIC,
                     F32_TO_I8_STATE_VERSION)

void
f32_to_i8_steps (f32_to_i8_state_t *state, const float *input, int8_t *output,
                 size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = f32_to_i8_step (state, input[i]);
}
