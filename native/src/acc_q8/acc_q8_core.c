#include "doppler/acc_q8/acc_q8_core.h"

dp_acc_q8_state_t *
dp_acc_q8_create (int32_t acc)
{
  dp_acc_q8_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->acc = acc;
  return obj;
}

void
dp_acc_q8_destroy (dp_acc_q8_state_t *state)
{
  free (state);
}

void
dp_acc_q8_reset (dp_acc_q8_state_t *state)
{
  state->acc = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_acc_q8, dp_acc_q8_state_t, ACC_Q8_STATE_MAGIC,
                     ACC_Q8_STATE_VERSION)

void
dp_acc_q8_steps (dp_acc_q8_state_t *state, const int8_t *input, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    dp_acc_q8_step (state, input[i]);
}

int32_t
dp_acc_q8_get_acc (const dp_acc_q8_state_t *state)
{
  return state->acc;
}

void
dp_acc_q8_set_acc (dp_acc_q8_state_t *state, int32_t val)
{
  state->acc = val;
}

int32_t
dp_acc_q8_get (dp_acc_q8_state_t *state)
{
  return state->acc;
}

int32_t
dp_acc_q8_dump (dp_acc_q8_state_t *state)
{
  int32_t v  = state->acc;
  state->acc = 0;
  return v;
}

void
dp_acc_q8_madd (dp_acc_q8_state_t *state, const int8_t *a, size_t a_len,
                const int8_t *b, size_t b_len)
{
  size_t n = a_len < b_len ? a_len : b_len;
  for (size_t i = 0; i < n; i++)
    state->acc += (int)a[i] * (int)b[i];
}
