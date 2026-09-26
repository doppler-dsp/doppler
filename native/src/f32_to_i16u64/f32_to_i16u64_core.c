#include "doppler/f32_to_i16u64/f32_to_i16u64_core.h"

dp_f32_to_i16u64_state_t *
dp_f32_to_i16u64_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  dp_f32_to_i16u64_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->scale = scale;
  return state;
}

void
dp_f32_to_i16u64_destroy (dp_f32_to_i16u64_state_t *state)
{
  free (state);
}

void
dp_f32_to_i16u64_reset (dp_f32_to_i16u64_state_t *state)
{
  state->clipped = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_f32_to_i16u64, dp_f32_to_i16u64_state_t,
                     F32_TO_I16U64_STATE_MAGIC, F32_TO_I16U64_STATE_VERSION)

void
dp_f32_to_i16u64_steps (dp_f32_to_i16u64_state_t *state, const float *input,
                        uint64_t *output, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = dp_f32_to_i16u64_step (state, input[i]);
}
