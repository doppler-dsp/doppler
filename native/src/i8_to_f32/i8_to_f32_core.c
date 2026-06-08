#include "i8_to_f32/i8_to_f32_core.h"

i8_to_f32_state_t *
i8_to_f32_create (float scale)
{
  if (scale <= 0.0f)
    return NULL;
  i8_to_f32_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->iscale = 1.0f / scale;
  return obj;
}

void
i8_to_f32_destroy (i8_to_f32_state_t *state)
{
  free (state);
}

void
i8_to_f32_reset (i8_to_f32_state_t *state)
{
  (void)state; /* no dynamic state to reset */
}

void
i8_to_f32_steps (i8_to_f32_state_t *state, const int8_t *input, float *output,
                 size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    output[i] = i8_to_f32_step (state, input[i]);
}
