#include "pdespread/pdespread_core.h"

#include <stdlib.h>
#include <string.h>

/* Re-seed the per-epoch partial accumulators and segment index. The embedded
 * DLL (code phase, rate, correlators, loop integrator) is re-seeded by its own
 * dll_init / dll_reset before this runs. */
static void
seed (pdespread_state_t *s)
{
  s->seg_idx    = 0;
  s->sum_e      = 0.0;
  s->sum_l      = 0.0;
  s->last_error = 0.0;
}

pdespread_state_t *
pdespread_create (const uint8_t *code, size_t code_len, size_t sps, size_t k,
                  double init_chip, double bn, double zeta, double spacing)
{
  if (!code || code_len == 0 || k == 0)
    return NULL;
  pdespread_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->code_copy = malloc (code_len);
  if (!obj->code_copy)
    {
      free (obj);
      return NULL;
    }
  memcpy (obj->code_copy, code, code_len);
  /* The DLL borrows the owned code copy (dll_init does not free it). */
  dll_init (&obj->dll, obj->code_copy, code_len, sps, init_chip, bn, zeta,
            spacing);
  obj->k         = k;
  obj->seg_chips = (double)code_len / (double)k;
  obj->seg_norm  = (double)(code_len * sps) / (double)k;
  seed (obj);
  return obj;
}

void
pdespread_destroy (pdespread_state_t *state)
{
  if (!state)
    return;
  free (state->code_copy);
  free (state);
}

void
pdespread_reset (pdespread_state_t *state)
{
  dll_reset (&state->dll);
  seed (state);
}

size_t
pdespread_steps_max_out (pdespread_state_t *state)
{
  (void)state;
  return 0; /* caller sizes to the input length (>= partials emitted) */
}

size_t
pdespread_steps (pdespread_state_t *state, const float complex *x,
                 size_t x_len, float complex *out, size_t max_out)
{
  dll_state_t *d       = &state->dll;
  size_t       emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      dll_accumulate (d, x[n]); /* E/P/L correlate + advance the code phase */
      /* A partial segment closes every sf/k chips of code phase; dump its
         prompt, fold its early/late envelopes into the non-coherent sums, and
         reset the correlators. (while: robust if a segment is ever < 1 step.)
       */
      while (state->seg_idx < state->k
             && d->chip_pos >= (double)(state->seg_idx + 1) * state->seg_chips)
        {
          if (emitted < max_out)
            out[emitted++] = d->acc_p / (float)state->seg_norm;
          state->sum_e += (double)cabsf (d->acc_e);
          state->sum_l += (double)cabsf (d->acc_l);
          d->acc_e = 0.0f;
          d->acc_p = 0.0f;
          d->acc_l = 0.0f;
          state->seg_idx++;
          if (state->seg_idx == state->k)
            {
              /* Epoch boundary: non-coherent early-late discriminator steers
                 the code NCO via the embedded loop filter (mirrors dll_update,
                 but on the partial-envelope sums instead of the coherent
                 epoch). */
              double me = state->sum_e, ml = state->sum_l;
              double e          = (me - ml) / (me + ml + DLL_EPS);
              state->last_error = e;
              loop_filter_step (&d->lf, e);
              d->code_rate = 1.0 + d->lf.integ;
              d->chip_pos -= (double)d->sf;
              d->chip_pos += d->lf.kp * e; /* proportional phase nudge */
              state->sum_e   = 0.0;
              state->sum_l   = 0.0;
              state->seg_idx = 0;
            }
        }
    }
  return emitted;
}

double
pdespread_get_code_phase (const pdespread_state_t *state)
{
  return state->dll.chip_pos;
}

double
pdespread_get_code_rate (const pdespread_state_t *state)
{
  return state->dll.code_rate;
}

double
pdespread_get_last_error (const pdespread_state_t *state)
{
  return state->last_error;
}

size_t
pdespread_get_k (const pdespread_state_t *state)
{
  return state->k;
}
