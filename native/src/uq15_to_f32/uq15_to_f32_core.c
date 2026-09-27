#include "doppler/uq15_to_f32/uq15_to_f32_core.h"

dp_uq15_to_f32_state_t *
dp_uq15_to_f32_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  dp_uq15_to_f32_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->iscale = 1.0f / scale;
  return state;
}

void
dp_uq15_to_f32_destroy (dp_uq15_to_f32_state_t *state)
{
  free (state);
}

void
dp_uq15_to_f32_reset (dp_uq15_to_f32_state_t *state)
{
  (void)state; /* no dynamic state to reset */
}

void
dp_uq15_to_f32_steps (dp_uq15_to_f32_state_t *state, const uint16_t *input,
                      float *output, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = dp_uq15_to_f32_step (state, input[i]);
}
