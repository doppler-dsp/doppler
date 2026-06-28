#include "costas/costas_core.h"

/* Seed the loop integrator so the per-symbol frequency estimate
 * (lf.integ / tsamps, rad/sample) matches the requested carrier offset,
 * and point the NCO at the same frequency — de-rotation is correct from
 * the first sample, before any update runs. */
static void
seed (costas_state_t *s, double init_norm_freq)
{
  lo_init (&s->nco, init_norm_freq);
  s->lf.integ    = init_norm_freq * 2.0 * M_PI * (double)s->tsamps;
  s->acc         = 0.0f;
  s->acc_n       = 0;
  s->prev        = 0.0f;
  s->have_prev   = 0;
  s->lock_metric = 0.0;
  s->last_error  = 0.0;
}

void
costas_init (costas_state_t *s, double bn, double zeta, double init_norm_freq,
             size_t tsamps, double bn_fll)
{
  s->tsamps         = tsamps ? tsamps : 1;
  s->bn             = bn;
  s->zeta           = zeta;
  s->bn_fll         = bn_fll;
  s->k_fll          = 4.0 * bn_fll; /* 1st-order FLL aiding gain */
  s->seed_norm_freq = init_norm_freq;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per symbol */
  seed (s, init_norm_freq);
}

costas_state_t *
costas_create (double bn, double zeta, double init_norm_freq, size_t tsamps,
               double bn_fll)
{
  costas_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  costas_init (obj, bn, zeta, init_norm_freq, tsamps, bn_fll);
  return obj;
}

void
costas_destroy (costas_state_t *state)
{
  free (state);
}

void
costas_reset (costas_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (costas, costas_state_t, COSTAS_STATE_MAGIC,
                     COSTAS_STATE_VERSION)

void
costas_configure (costas_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
costas_steps_max_out (costas_state_t *state)
{
  (void)state;
  return 0;
}

size_t
costas_steps (costas_state_t *state, const float complex *x, size_t x_len,
              float complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      state->acc += costas_wipeoff (state, x[n]);
      if (++state->acc_n < state->tsamps)
        continue;
      /* symbol boundary: dump, steer the loop, emit the prompt */
      float complex prompt = state->acc;
      costas_update (state, prompt);
      if (emitted < max_out)
        out[emitted++] = prompt / (float)state->tsamps;
      state->acc   = 0.0f;
      state->acc_n = 0;
    }
  return emitted;
}

double
costas_get_bn (const costas_state_t *state)
{
  return state->bn;
}

void
costas_set_bn (costas_state_t *state, double val)
{
  costas_configure (state, val, state->zeta);
}

double
costas_get_norm_freq (const costas_state_t *state)
{
  return state->nco.norm_freq;
}

void
costas_set_norm_freq (costas_state_t *state, double val)
{
  state->seed_norm_freq = val;
  loop_filter_reset (&state->lf);
  seed (state, val);
}

double
costas_get_lock_metric (const costas_state_t *state)
{
  return state->lock_metric;
}

double
costas_get_last_error (const costas_state_t *state)
{
  return state->last_error;
}

double
costas_get_bn_fll (const costas_state_t *state)
{
  return state->bn_fll;
}

void
costas_set_bn_fll (costas_state_t *state, double val)
{
  state->bn_fll = val;
  state->k_fll  = 4.0 * val;
}
