#include "acc_cf64/acc_cf64_core.h"

acc_cf64_state_t *
acc_cf64_create (double _Complex acc)
{
  acc_cf64_state_t *state = calloc (1, sizeof (*state));
  if (!state)
    return NULL;
  state->acc = acc;
  return state;
}

void
acc_cf64_destroy (acc_cf64_state_t *state)
{
  free (state);
}

void
acc_cf64_reset (acc_cf64_state_t *state)
{
  state->acc = 0.0 + 0.0 * I;
}

/* JM_RESTRICT unlocks auto-vectorisation by eliminating the aliasing hazard.
 * Separate scalar re/im accumulators give the compiler two independent
 * reduction chains — it will vectorise both with -march=native -ffast-math.
 * (Explicit JM_VEC_F64 for interleaved complex requires deinterleaving that
 * is not in the JM macro set; auto-vec gets us the same result cleanly.) */
JM_HOT void
acc_cf64_steps (acc_cf64_state_t *JM_RESTRICT     state,
                const double complex *JM_RESTRICT input, size_t n)
{
  double re = 0.0, im = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      re += creal (input[i]);
      im += cimag (input[i]);
    }
  state->acc += re + im * I;
}

double _Complex acc_cf64_get_acc (const acc_cf64_state_t *state)
{
  return state->acc;
}

void
acc_cf64_set_acc (acc_cf64_state_t *state, double _Complex acc)
{
  state->acc = acc;
}

double complex
acc_cf64_get (acc_cf64_state_t *state)
{
  return state->acc;
}

double complex
acc_cf64_dump (acc_cf64_state_t *state)
{
  double complex v = state->acc;
  state->acc       = 0.0 + 0.0 * I;
  return v;
}

void
acc_cf64_madd (acc_cf64_state_t *state, const double complex *x, size_t x_len,
               const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * (double)h[i];
}

void
acc_cf64_add2d (acc_cf64_state_t *state, const double complex *x, size_t x_len)
{
  for (size_t i = 0; i < x_len; i++)
    state->acc += x[i];
}

void
acc_cf64_madd2d (acc_cf64_state_t *state, const double complex *x,
                 size_t x_len, const float *h, size_t h_len)
{
  size_t n = x_len < h_len ? x_len : h_len;
  for (size_t i = 0; i < n; i++)
    state->acc += x[i] * (double)h[i];
}
