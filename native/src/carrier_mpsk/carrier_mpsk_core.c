#include "doppler/carrier_mpsk/carrier_mpsk_core.h"

#include <stdlib.h>

/* Seed the loop integrator so the per-symbol frequency estimate
 * (lf.integ / tsamps, rad/sample) matches the requested carrier offset, and
 * point the NCO at the same frequency — de-rotation is correct from the first
 * sample, before any update runs. */
static void
seed (dp_carrier_mpsk_state_t *s, double init_norm_freq)
{
  lo_init (&s->nco, init_norm_freq);
  s->lf.integ    = init_norm_freq * 2.0 * M_PI * (double)s->tsamps;
  s->acc         = 0.0f;
  s->acc_n       = 0;
  s->prev        = 0.0f;
  s->prev_abs    = 0.0;
  s->have_prev   = 0;
  s->lock_metric = 0.0;
  s->last_error  = 0.0;
}

void
carrier_mpsk_init (dp_carrier_mpsk_state_t *s, double bn, double zeta,
                   double init_norm_freq, size_t tsamps, double bn_fll, int m)
{
  s->tsamps         = tsamps ? tsamps : 1;
  s->bn             = bn;
  s->zeta           = zeta;
  s->bn_fll         = bn_fll;
  s->k_fll          = 4.0 * bn_fll; /* 1st-order FLL aiding gain */
  s->m              = m;
  s->seed_norm_freq = init_norm_freq;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per symbol */
  seed (s, init_norm_freq);
}

dp_carrier_mpsk_state_t *
dp_carrier_mpsk_create (double bn, double zeta, double init_norm_freq,
                        size_t tsamps, double bn_fll, int m)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  dp_carrier_mpsk_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  carrier_mpsk_init (obj, bn, zeta, init_norm_freq, tsamps, bn_fll, m);
  return obj;
}

void
dp_carrier_mpsk_destroy (dp_carrier_mpsk_state_t *state)
{
  free (state);
}

void
dp_carrier_mpsk_reset (dp_carrier_mpsk_state_t *state)
{
  dp_loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

/* Serializable state — pointer-free POD whole-struct snapshot
 * (see DP_DEFINE_POD_STATE in dp_state.h). */
DP_DEFINE_POD_STATE (dp_carrier_mpsk, dp_carrier_mpsk_state_t,
                     CARRIER_MPSK_STATE_MAGIC, CARRIER_MPSK_STATE_VERSION)

void
dp_carrier_mpsk_configure (dp_carrier_mpsk_state_t *state, double bn,
                           double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  dp_loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
dp_carrier_mpsk_steps_max_out (dp_carrier_mpsk_state_t *state)
{
  (void)state;
  return 0; /* one symbol per sps inputs, so symbols <= inputs */
}

size_t
dp_carrier_mpsk_steps (dp_carrier_mpsk_state_t *state, const float _Complex *x,
                       size_t x_len, float _Complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      state->acc += carrier_mpsk_wipeoff (state, x[n]);
      if (++state->acc_n < state->tsamps)
        continue;
      /* symbol boundary: dump, steer the loop, emit the prompt */
      float _Complex prompt = state->acc;
      carrier_mpsk_update (state, prompt);
      if (emitted < max_out)
        out[emitted++] = prompt / (float)state->tsamps;
      state->acc   = 0.0f;
      state->acc_n = 0;
    }
  return emitted;
}

double
dp_carrier_mpsk_get_bn (const dp_carrier_mpsk_state_t *state)
{
  return state->bn;
}

void
dp_carrier_mpsk_set_bn (dp_carrier_mpsk_state_t *state, double val)
{
  dp_carrier_mpsk_configure (state, val, state->zeta);
}

double
dp_carrier_mpsk_get_norm_freq (const dp_carrier_mpsk_state_t *state)
{
  return state->nco.norm_freq;
}

void
dp_carrier_mpsk_set_norm_freq (dp_carrier_mpsk_state_t *state, double val)
{
  state->seed_norm_freq = val;
  dp_loop_filter_reset (&state->lf);
  seed (state, val);
}

double
dp_carrier_mpsk_get_lock_metric (const dp_carrier_mpsk_state_t *state)
{
  return state->lock_metric;
}

double
dp_carrier_mpsk_get_last_error (const dp_carrier_mpsk_state_t *state)
{
  return state->last_error;
}

double
dp_carrier_mpsk_get_bn_fll (const dp_carrier_mpsk_state_t *state)
{
  return state->bn_fll;
}

void
dp_carrier_mpsk_set_bn_fll (dp_carrier_mpsk_state_t *state, double val)
{
  state->bn_fll = val;
  state->k_fll  = 4.0 * val;
}

int
dp_carrier_mpsk_get_m (const dp_carrier_mpsk_state_t *state)
{
  return state->m;
}
