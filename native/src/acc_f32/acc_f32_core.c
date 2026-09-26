#include "doppler/acc_f32/acc_f32_core.h"

dp_acc_f32_state_t *
dp_acc_f32_create (float acc)
{
  dp_acc_f32_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->acc = acc;
  return state;
}

void
dp_acc_f32_destroy (dp_acc_f32_state_t *state)
{
  free (state);
}

void
dp_acc_f32_reset (dp_acc_f32_state_t *state)
{
  state->acc = 0.0f;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_acc_f32, dp_acc_f32_state_t, ACC_F32_STATE_MAGIC,
                     ACC_F32_STATE_VERSION)

/* JM_RESTRICT removes the aliasing hazard between state->acc and input[],
 * which otherwise serialises the loop regardless of -ffast-math.
 * The explicit JM_VEC_F32 accumulator processes JM_SIMD_WIDTH_F32 floats
 * per iteration; JM_HSUM_F32 folds to a scalar at the end. */
#if JM_SIMD_WIDTH_F32 > 1
JM_HOT void
dp_acc_f32_steps (dp_acc_f32_state_t *JM_RESTRICT state,
                  const float *JM_RESTRICT input, size_t n)
{
  JM_VEC_F32 vacc = JM_ZERO_F32 ();
  size_t     i    = 0;
  for (; i + JM_SIMD_WIDTH_F32 <= n; i += JM_SIMD_WIDTH_F32)
    vacc = JM_ADD_F32 (vacc, JM_LOAD_F32 (input + i));
  state->acc += JM_HSUM_F32 (vacc);
  for (; i < n; i++)
    state->acc += input[i];
}
#else
JM_HOT void
dp_acc_f32_steps (dp_acc_f32_state_t *JM_RESTRICT state,
                  const float *JM_RESTRICT input, size_t n)
{
  for (size_t i = 0; i < n; i++)
    state->acc += input[i];
}
#endif

float
dp_acc_f32_get_acc (const dp_acc_f32_state_t *state)
{
  return state->acc;
}

void
dp_acc_f32_set_acc (dp_acc_f32_state_t *state, float value)
{
  state->acc = value;
}

float
dp_acc_f32_get (dp_acc_f32_state_t *state)
{
  return state->acc;
}

float
dp_acc_f32_dump (dp_acc_f32_state_t *state)
{
  float v    = state->acc;
  state->acc = 0.0f;
  return v;
}

void
dp_acc_f32_madd (dp_acc_f32_state_t *state, const float *x, size_t x_len,
                 const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * h[i];
}

void
dp_acc_f32_add2d (dp_acc_f32_state_t *state, const float *x, size_t x_len)
{
  for (size_t i = 0; i < x_len; i++)
    state->acc += x[i];
}

void
dp_acc_f32_madd2d (dp_acc_f32_state_t *state, const float *x, size_t x_len,
                   const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * h[i];
}
