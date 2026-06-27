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
  s->seg_idx    = 0;
  s->sum_e      = 0.0;
  s->sum_l      = 0.0;
}

/* Set the partial-correlation count and its derived geometry (>= 1). */
static void
set_segments (dll_state_t *s, size_t segments)
{
  s->segments  = segments ? segments : 1;
  s->seg_chips = (double)s->sf / (double)s->segments;
  s->seg_norm  = (double)(s->sf * s->sps) / (double)s->segments;
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
  set_segments (s,
                1); /* default: coherent full-epoch (dll_create overrides) */
}

void
dll_init (dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
          double init_chip, double bn, double zeta, double spacing)
{
  configure_geometry (s, code_len, sps, init_chip, bn, zeta, spacing);
  /* In-place init of a caller-owned (possibly stack) state: loop_filter_init
     preserves the integrator (it doubles as a reconfigure), so zero it here —
     seed() sets code_rate = 1.0 and assumes integ == 0. dll_create() gets this
     free via calloc; an embedded/stack dll_state_t would otherwise start with
     a garbage code rate. */
  loop_filter_reset (&s->lf);
  s->code      = code; /* borrowed */
  s->owns_code = 0;
  seed (s);
}

dll_state_t *
dll_create (const uint8_t *code, size_t code_len, size_t sps, double init_chip,
            double bn, double zeta, double spacing, size_t segments)
{
  if (!code || code_len == 0 || segments == 0)
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
  set_segments (obj, segments);
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
  /* segments == 1: coherent full-epoch integrate-and-dump (one prompt/period).
   */
  if (state->segments <= 1)
    {
      double tsamps = (double)(state->sf * state->sps);
      for (size_t n = 0; n < x_len; n++)
        {
          dll_accumulate (state, x[n]);
          if (state->chip_pos < (double)state->sf)
            continue;
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
  /* segments > 1: dump a partial prompt every sf/segments chips, fold each
     partial's early/late envelopes into the non-coherent epoch sums, and steer
     the code NCO once per epoch on (sum|E|-sum|L|)/(sum|E|+sum|L|) — which a
     data flip cannot collapse (only the one straddling segment degrades). */
  for (size_t n = 0; n < x_len; n++)
    {
      dll_accumulate (state, x[n]);
      while (state->seg_idx < state->segments
             && state->chip_pos
                    >= (double)(state->seg_idx + 1) * state->seg_chips)
        {
          if (emitted < max_out)
            out[emitted++] = state->acc_p / (float)state->seg_norm;
          state->sum_e += (double)cabsf (state->acc_e);
          state->sum_l += (double)cabsf (state->acc_l);
          state->acc_e = 0.0f;
          state->acc_p = 0.0f;
          state->acc_l = 0.0f;
          state->seg_idx++;
          if (state->seg_idx == state->segments)
            {
              double me = state->sum_e, ml = state->sum_l;
              double e          = (me - ml) / (me + ml + DLL_EPS);
              state->last_error = e;
              loop_filter_step (&state->lf, e);
              state->code_rate = 1.0 + state->lf.integ;
              state->chip_pos -= (double)state->sf;
              state->chip_pos += state->lf.kp * e; /* proportional nudge */
              state->sum_e   = 0.0;
              state->sum_l   = 0.0;
              state->seg_idx = 0;
            }
        }
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

size_t
dll_get_segments (const dll_state_t *state)
{
  return state->segments;
}
