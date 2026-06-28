#include "carrier_mpsk/carrier_mpsk_core.h"

#include <stdlib.h>

/* Seed the loop integrator so the per-symbol frequency estimate
 * (lf.integ / tsamps, rad/sample) matches the requested carrier offset, and
 * point the NCO at the same frequency — de-rotation is correct from the first
 * sample, before any update runs. */
static void
seed (carrier_mpsk_state_t *s, double init_norm_freq)
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
carrier_mpsk_init (carrier_mpsk_state_t *s, double bn, double zeta,
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

carrier_mpsk_state_t *
carrier_mpsk_create (double bn, double zeta, double init_norm_freq,
                     size_t tsamps, double bn_fll, int m)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  carrier_mpsk_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  carrier_mpsk_init (obj, bn, zeta, init_norm_freq, tsamps, bn_fll, m);
  return obj;
}

void
carrier_mpsk_destroy (carrier_mpsk_state_t *state)
{
  free (state);
}

void
carrier_mpsk_reset (carrier_mpsk_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

/* ── Serializable state — standard envelope (see dp_state.h) ────────────────
 * Pointer-free POD; a whole-struct snapshot resumes the loop bit-for-bit. */

size_t
carrier_mpsk_state_bytes (const carrier_mpsk_state_t *state)
{
  (void)state;
  return sizeof (dp_state_hdr_t) + sizeof (carrier_mpsk_state_t);
}

void
carrier_mpsk_get_state (const carrier_mpsk_state_t *state, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, carrier_mpsk_state_bytes (state));
  dp_w_hdr (&w, CARRIER_MPSK_STATE_MAGIC, CARRIER_MPSK_STATE_VERSION,
            carrier_mpsk_state_bytes (state));
  dp_w_bytes (&w, state, sizeof *state);
}

int
carrier_mpsk_set_state (carrier_mpsk_state_t *state, const void *blob)
{
  int rc = dp_state_validate (blob, carrier_mpsk_state_bytes (state),
                              CARRIER_MPSK_STATE_MAGIC,
                              CARRIER_MPSK_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, carrier_mpsk_state_bytes (state));
  r.off         = sizeof (dp_state_hdr_t);
  dp_r_bytes (&r, state, sizeof *state);
  return DP_OK;
}

void
carrier_mpsk_configure (carrier_mpsk_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
carrier_mpsk_steps_max_out (carrier_mpsk_state_t *state)
{
  (void)state;
  return 0;
}

size_t
carrier_mpsk_steps (carrier_mpsk_state_t *state, const float complex *x,
                    size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      state->acc += carrier_mpsk_wipeoff (state, x[n]);
      if (++state->acc_n < state->tsamps)
        continue;
      /* symbol boundary: dump, steer the loop, emit the prompt */
      float complex prompt = state->acc;
      carrier_mpsk_update (state, prompt);
      if (emitted < max_out)
        out[emitted++] = prompt / (float)state->tsamps;
      state->acc   = 0.0f;
      state->acc_n = 0;
    }
  return emitted;
}

double
carrier_mpsk_get_bn (const carrier_mpsk_state_t *state)
{
  return state->bn;
}

void
carrier_mpsk_set_bn (carrier_mpsk_state_t *state, double val)
{
  carrier_mpsk_configure (state, val, state->zeta);
}

double
carrier_mpsk_get_norm_freq (const carrier_mpsk_state_t *state)
{
  return state->nco.norm_freq;
}

void
carrier_mpsk_set_norm_freq (carrier_mpsk_state_t *state, double val)
{
  state->seed_norm_freq = val;
  loop_filter_reset (&state->lf);
  seed (state, val);
}

double
carrier_mpsk_get_lock_metric (const carrier_mpsk_state_t *state)
{
  return state->lock_metric;
}

double
carrier_mpsk_get_last_error (const carrier_mpsk_state_t *state)
{
  return state->last_error;
}

double
carrier_mpsk_get_bn_fll (const carrier_mpsk_state_t *state)
{
  return state->bn_fll;
}

void
carrier_mpsk_set_bn_fll (carrier_mpsk_state_t *state, double val)
{
  state->bn_fll = val;
  state->k_fll  = 4.0 * val;
}

int
carrier_mpsk_get_m (const carrier_mpsk_state_t *state)
{
  return state->m;
}
