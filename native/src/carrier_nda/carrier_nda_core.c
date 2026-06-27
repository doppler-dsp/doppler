#include "carrier_nda/carrier_nda_core.h"

#include <stdlib.h>

/* Per-M lock-signal scale: normalizes the lock metric across constellations
 * (see docs/design/mpsk.md §2.3). */
static double
lock_scale_for (int m)
{
  if (m == 2)
    return 1.0;
  if (m == 4)
    return 0.619;
  return 0.412; /* m == 8 */
}

/* Seed the loop integrator so the per-arm-update frequency estimate
 * (lf.integ / arm_len, rad/sample) matches the requested carrier offset, and
 * point the NCO at the same frequency — de-rotation is correct from the first
 * sample, before any update runs. */
static void
seed (carrier_nda_state_t *s, double init_norm_freq)
{
  lo_init (&s->nco, init_norm_freq);
  s->lf.integ   = init_norm_freq * 2.0 * M_PI * (double)s->arm_len;
  s->arm_acc    = 0.0f;
  s->arm_cnt    = 0;
  s->lock       = 0.0;
  s->last_error = 0.0;
}

void
carrier_nda_init (carrier_nda_state_t *s, double bn, double zeta,
                  double init_norm_freq, size_t sps, int n, int m)
{
  s->sps     = sps ? sps : 1;
  s->m       = m;
  s->n       = n > 0 ? n : 1;
  s->arm_len = s->sps / (size_t)s->n;
  if (s->arm_len == 0)
    s->arm_len = 1;
  s->lock_scale     = lock_scale_for (m);
  s->bn             = bn;
  s->zeta           = zeta;
  s->seed_norm_freq = init_norm_freq;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per arm dump */
  seed (s, init_norm_freq);
}

carrier_nda_state_t *
carrier_nda_create (double bn, double zeta, double init_norm_freq, size_t sps,
                    int n, int m)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (sps == 0 || n <= 0 || sps % (size_t)n != 0)
    return NULL; /* arm length must be a whole number of samples */
  carrier_nda_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  carrier_nda_init (obj, bn, zeta, init_norm_freq, sps, n, m);
  return obj;
}

void
carrier_nda_destroy (carrier_nda_state_t *state)
{
  free (state);
}

void
carrier_nda_reset (carrier_nda_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

/* Output bound: emitted samples == input length (the de-rotated stream). */
size_t
carrier_nda_steps_max_out (carrier_nda_state_t *state)
{
  (void)state;
  return 0;
}

size_t
carrier_nda_steps (carrier_nda_state_t *state, const float complex *x,
                   size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t i = 0; i < x_len; i++)
    {
      float complex d = carrier_nda_wipeoff (state, x[i]);
      double        pe, lk;
      if (carrier_nda_arm_step (state, d, &pe, &lk))
        {
          state->lock += CARRIER_NDA_LOCK_ALPHA * (lk - state->lock);
          carrier_nda_steer (state, pe);
        }
      if (emitted < max_out)
        out[emitted++] = d;
    }
  return emitted;
}

double
carrier_nda_get_norm_freq (const carrier_nda_state_t *state)
{
  return state->nco.norm_freq;
}

void
carrier_nda_set_norm_freq (carrier_nda_state_t *state, double val)
{
  state->seed_norm_freq = val;
  loop_filter_reset (&state->lf);
  seed (state, val);
}

double
carrier_nda_get_lock (const carrier_nda_state_t *state)
{
  return state->lock;
}

double
carrier_nda_get_last_error (const carrier_nda_state_t *state)
{
  return state->last_error;
}

double
carrier_nda_get_bn (const carrier_nda_state_t *state)
{
  return state->bn;
}

void
carrier_nda_set_bn (carrier_nda_state_t *state, double val)
{
  state->bn = val;
  loop_filter_configure (&state->lf, val, state->zeta, 1.0);
}

int
carrier_nda_get_m (const carrier_nda_state_t *state)
{
  return state->m;
}

int
carrier_nda_get_n (const carrier_nda_state_t *state)
{
  return state->n;
}

size_t
carrier_nda_get_sps (const carrier_nda_state_t *state)
{
  return state->sps;
}
