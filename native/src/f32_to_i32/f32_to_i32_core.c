#include "doppler/f32_to_i32/f32_to_i32_core.h"

dp_f32_to_i32_state_t *
dp_f32_to_i32_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  dp_f32_to_i32_state_t *state = dp_xcalloc (1, sizeof (*state));
  state->scale                 = scale;
  return state;
}

void
dp_f32_to_i32_destroy (dp_f32_to_i32_state_t *state)
{
  free (state);
}

void
dp_f32_to_i32_reset (dp_f32_to_i32_state_t *state)
{
  state->clipped = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_f32_to_i32, dp_f32_to_i32_state_t,
                     F32_TO_I32_STATE_MAGIC, F32_TO_I32_STATE_VERSION)

void
dp_f32_to_i32_steps (dp_f32_to_i32_state_t *state, const float *input,
                     int32_t *output, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = dp_f32_to_i32_step (state, input[i]);
}
