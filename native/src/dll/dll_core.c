#include "dll/dll_core.h"

#include <stdlib.h>
#include <string.h>

/* Re-seed the loop to its create-time code phase + nominal rate, and clear the
 * correlator accumulators. The loop filter integrator is reset by the caller
 * (dll_init / dll_reset) before this runs. */
static void
seed (dll_state_t *s)
{
  s->chip_pos   = s->seed_chip;
  s->code_rate  = 1.0;
  s->acc_e      = 0.0f;
  s->acc_p      = 0.0f;
  s->acc_l      = 0.0f;
  s->last_error = 0.0;
}

static void
configure_geometry (dll_state_t *s, size_t code_len, size_t sps,
                    double init_chip, double bn, double zeta, double spacing)
{
  s->sf        = code_len ? code_len : 1;
  s->sps       = sps ? sps : 1;
  s->inv_sps   = 1.0 / (double)s->sps;
  s->spacing   = spacing;
  s->seed_chip = init_chip;
  s->bn        = bn;
  s->zeta      = zeta;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per period */
}

void
dll_init (dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
          double init_chip, double bn, double zeta, double spacing)
{
  configure_geometry (s, code_len, sps, init_chip, bn, zeta, spacing);
  s->code      = code; /* borrowed */
  s->owns_code = 0;
  seed (s);
}

dll_state_t *
dll_create (const uint8_t *code, size_t code_len, size_t sps, double init_chip,
            double bn, double zeta, double spacing)
{
  if (!code || code_len == 0)
    return NULL;
  dll_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  uint8_t *copy = malloc (code_len);
  if (!copy)
    {
      free (obj);
      return NULL;
    }
  memcpy (copy, code, code_len);
  configure_geometry (obj, code_len, sps, init_chip, bn, zeta, spacing);
  obj->code      = copy;
  obj->owns_code = 1;
  seed (obj);
  return obj;
}

void
dll_destroy (dll_state_t *state)
{
  if (!state)
    return;
  if (state->owns_code)
    free ((void *)state->code);
  free (state);
}

void
dll_reset (dll_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

void
dll_configure (dll_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
dll_steps_max_out (dll_state_t *state)
{
  (void)state;
  return 0;
}

size_t
dll_steps (dll_state_t *state, const float complex *x, size_t x_len,
           float complex *out, size_t max_out)
{
  size_t emitted = 0;
  double tsamps  = (double)(state->sf * state->sps);
  for (size_t n = 0; n < x_len; n++)
    {
      dll_accumulate (state, x[n]);
      if (state->chip_pos < (double)state->sf)
        continue;
      /* code-period boundary: dump the prompt, steer the loop, emit */
      float complex prompt = state->acc_p;
      dll_update (state);
      if (emitted < max_out)
        out[emitted++] = prompt / (float)tsamps;
      state->acc_e = 0.0f;
      state->acc_p = 0.0f;
      state->acc_l = 0.0f;
    }
  return emitted;
}

double
dll_get_bn (const dll_state_t *state)
{
  return state->bn;
}

void
dll_set_bn (dll_state_t *state, double val)
{
  dll_configure (state, val, state->zeta);
}

double
dll_get_code_phase (const dll_state_t *state)
{
  return state->chip_pos;
}

double
dll_get_code_rate (const dll_state_t *state)
{
  return state->code_rate;
}

double
dll_get_last_error (const dll_state_t *state)
{
  return state->last_error;
}
