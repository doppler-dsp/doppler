#include "doppler/u8_to_f32/u8_to_f32_core.h"

u8_to_f32_state_t *
u8_to_f32_create (int mode)
{
  if (mode != U8_TO_F32_SHIFT && mode != U8_TO_F32_MIDPOINT)
    return NULL;
  u8_to_f32_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  obj->mode              = mode;
  obj->iscale            = 1.0f / 127.5f;
  return obj;
}

void
u8_to_f32_destroy (u8_to_f32_state_t *state)
{
  free (state);
}

void
u8_to_f32_reset (u8_to_f32_state_t *state)
{
  (void)state; /* no dynamic state to reset */
}

void
u8_to_f32_steps (u8_to_f32_state_t *state, const uint8_t *input, float *output,
                 size_t n)
{
  /* Resolve the mode once per block, not once per sample: each loop is then
     a straight map the compiler can vectorise, with the per-sample maths
     defined only in the header's two helpers. */
  if (state->mode == U8_TO_F32_MIDPOINT)
    {
      for (size_t i = 0; i < n; i++)
        output[i] = u8_to_f32_midpoint (state, input[i]);
    }
  else
    {
      for (size_t i = 0; i < n; i++)
        output[i] = u8_to_f32_shift (input[i]);
    }
}
