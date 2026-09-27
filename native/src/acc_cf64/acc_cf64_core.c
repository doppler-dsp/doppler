#include "doppler/acc_cf64/acc_cf64_core.h"

dp_acc_cf64_state_t *
dp_acc_cf64_create (double _Complex acc)
{
  dp_acc_cf64_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->acc = acc;
  return state;
}

void
dp_acc_cf64_destroy (dp_acc_cf64_state_t *state)
{
  free (state);
}

void
dp_acc_cf64_reset (dp_acc_cf64_state_t *state)
{
  state->acc = 0.0 + 0.0 * I;
}

/* Serializable state — whole-struct POD snapshot, pointer-free (see
 * DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_acc_cf64, dp_acc_cf64_state_t, ACC_CF64_STATE_MAGIC,
                     ACC_CF64_STATE_VERSION)

/* JM_RESTRICT unlocks auto-vectorisation by eliminating the aliasing hazard.
 * Separate scalar re/im accumulators give the compiler two independent
 * reduction chains — it will vectorise both with -march=native -ffast-math.
 * (Explicit JM_VEC_F64 for interleaved complex requires deinterleaving that
 * is not in the JM macro set; auto-vec gets us the same result cleanly.) */
JM_HOT void
dp_acc_cf64_steps (dp_acc_cf64_state_t *JM_RESTRICT   state,
                   const double _Complex *JM_RESTRICT input, size_t n)
{
  double re = 0.0, im = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      re += creal (input[i]);
      im += cimag (input[i]);
    }
  state->acc += re + im * I;
}

double _Complex dp_acc_cf64_get_acc (const dp_acc_cf64_state_t *state)
{
  return state->acc;
}

void
dp_acc_cf64_set_acc (dp_acc_cf64_state_t *state, double _Complex value)
{
  state->acc = value;
}

double _Complex dp_acc_cf64_get (dp_acc_cf64_state_t *state)
{
  return state->acc;
}

double _Complex dp_acc_cf64_dump (dp_acc_cf64_state_t *state)
{
  double _Complex v = state->acc;
  state->acc        = 0.0 + 0.0 * I;
  return v;
}

void
dp_acc_cf64_madd (dp_acc_cf64_state_t *state, const double _Complex *x,
                  size_t x_len, const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * (double)h[i];
}

void
dp_acc_cf64_add2d (dp_acc_cf64_state_t *state, const double _Complex *x,
                   size_t x_len)
{
  for (size_t i = 0; i < x_len; i++)
    state->acc += x[i];
}

void
dp_acc_cf64_madd2d (dp_acc_cf64_state_t *state, const double _Complex *x,
                    size_t x_len, const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * (double)h[i];
}
