#include "acc_q15/acc_q15_core.h"
#include "q15_mac.h"

acc_q15_state_t *
acc_q15_create (int64_t acc)
{
  acc_q15_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->acc = acc;
  return obj;
}

void
acc_q15_destroy (acc_q15_state_t *state)
{
  free (state);
}

void
acc_q15_reset (acc_q15_state_t *state)
{
  state->acc = 0;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (acc_q15, acc_q15_state_t, ACC_Q15_STATE_MAGIC,
                     ACC_Q15_STATE_VERSION)

void
acc_q15_steps (acc_q15_state_t *state, const int16_t *input, size_t n)
{
  /* #pragma omp simd */
  for (size_t i = 0; i < n; i++)
    acc_q15_step (state, input[i]);
}

int64_t
acc_q15_get_acc (const acc_q15_state_t *state)
{
  return state->acc;
}

void
acc_q15_set_acc (acc_q15_state_t *state, int64_t val)
{
  state->acc = val;
}

int64_t
acc_q15_get (acc_q15_state_t *state)
{
  return state->acc;
}

int64_t
acc_q15_dump (acc_q15_state_t *state)
{
  int64_t v  = state->acc;
  state->acc = 0;
  return v;
}

void
acc_q15_madd (acc_q15_state_t *state, const int16_t *a, size_t a_len,
              const int16_t *b, size_t b_len)
{
  size_t n = a_len < b_len ? a_len : b_len;
#if defined(__AVX2__)
  state->acc += dot_q15_avx2 (a, b, n);
#else
  state->acc += dot_q15_scalar (a, b, n);
#endif
}
