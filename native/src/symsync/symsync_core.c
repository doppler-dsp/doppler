#include "symsync/symsync_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Nominal NCO increment: the accumulator advances by 1/sps of full scale per
 * input sample, wrapping once per symbol.  The on-time strobe is the wrap and
 * the mid-symbol strobe is the half-scale crossing, giving the Gardner
 * detector its two interpolants per symbol from one accumulator. */
static uint32_t
nominal_inc (size_t sps)
{
  double s = (double)(sps ? sps : 1);
  return (uint32_t)(4294967296.0 / s);
}

static void
seed (symsync_state_t *s)
{
  s->timing.phase     = 0;
  s->timing.phase_inc = s->base_inc;
  s->have_ontime      = 0;
  s->prev_ontime      = 0.0f;
  s->mid              = 0.0f;
  s->last_error       = 0.0;
  s->rate_est         = (double)s->sps;
  s->pwr_avg          = 1.0;
  farrow_reset (&s->farrow);
}

void
symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
              int order, int ted)
{
  /* Zero first so an in-place (stack-embedded) init byte-matches the
   * calloc + init done by symsync_create: seed() sets only the timing NCO's
   * phase/phase_inc, leaving the NCO's norm_freq/nmax to this memset. */
  memset (s, 0, sizeof (*s));
  s->sps      = sps ? sps : 1;
  s->bn       = bn;
  s->zeta     = zeta;
  s->base_inc = nominal_inc (s->sps);
  s->ted      = ted;
  farrow_init (&s->farrow, order);
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* one update per symbol */
  seed (s);
}

symsync_state_t *
symsync_create (size_t sps, double bn, double zeta, int order, int ted)
{
  symsync_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  symsync_init (obj, sps, bn, zeta, order, ted);
  return obj;
}

void
symsync_destroy (symsync_state_t *state)
{
  free (state);
}

void
symsync_reset (symsync_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

int
symsync_set_telemetry (symsync_state_t *state, dp_tlm_t *tlm,
                       const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char *p = prefix ? prefix : "sync";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.rate", p);
  int id_rate = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_freq < 0 || id_rate < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  state->tlm.id_e    = id_e;
  state->tlm.id_freq = id_freq;
  state->tlm.id_rate = id_rate;
  state->tlm.ctx     = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
symsync_tlm_flush (const symsync_state_t *s)
{
  /* The loop control isn't retained per symbol; reconstruct it from the
   * NCO increment it steered (float32 records — the uint32 rounding is
   * far below record precision). */
  double control = (double)s->timing.phase_inc / (double)s->base_inc - 1.0;
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, control);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_rate, s->rate_est);
}

/* Serializable state — pointer-free composition (nco + farrow + loop_filter
 * embedded by value, all POD) + scalar timing state: a whole-struct snapshot,
 * with the telemetry attachment zeroed in blobs and kept live across restore
 * (see DP_DEFINE_POD_STATE_TLM in dp_state.h). */
DP_DEFINE_POD_STATE_TLM (symsync, symsync_state_t, SYMSYNC_STATE_MAGIC,
                         SYMSYNC_STATE_VERSION, tlm)

void
symsync_configure (symsync_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

size_t
symsync_steps_max_out (symsync_state_t *state)
{
  (void)state;
  return 0;
}

size_t
symsync_steps (symsync_state_t *state, const float complex *x, size_t x_len,
               float complex *out, size_t max_out)
{
  size_t        emitted = 0;
  float complex y;
  /* The TED selection is hoisted out of the hot loop: a literal ted lets
   * the force-inlined step constant-fold the detector branch, so each
   * specialised loop body carries exactly one TED. The runtime `s->ted`
   * branch inside the loop kept both detector bodies live across the
   * per-sample path and measured ~30% slower at 64k blocks. */
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loops contain NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * NCO/Farrow hot state (measured ~20% slower detached even though the
   * call never executed). */
  if (!state->tlm.ctx)
    {
      if (state->ted == SYMSYNC_TED_DTTL)
        {
          for (size_t n = 0; n < x_len; n++)
            if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_DTTL)
                && emitted < max_out)
              out[emitted++] = y;
        }
      else
        {
          for (size_t n = 0; n < x_len; n++)
            if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_GARDNER)
                && emitted < max_out)
              out[emitted++] = y;
        }
    }
  else if (state->ted == SYMSYNC_TED_DTTL)
    {
      for (size_t n = 0; n < x_len; n++)
        if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_DTTL))
          {
            if (emitted < max_out)
              out[emitted++] = y;
            symsync_tlm_flush (state);
          }
    }
  else
    {
      for (size_t n = 0; n < x_len; n++)
        if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_GARDNER))
          {
            if (emitted < max_out)
              out[emitted++] = y;
            symsync_tlm_flush (state);
          }
    }
  return emitted;
}

double
symsync_get_bn (const symsync_state_t *state)
{
  return state->bn;
}

void
symsync_set_bn (symsync_state_t *state, double val)
{
  symsync_configure (state, val, state->zeta);
}

double
symsync_get_timing_error (const symsync_state_t *state)
{
  return state->last_error;
}

double
symsync_get_rate (const symsync_state_t *state)
{
  /* effective samples/symbol the loop is tracking (EMA of the instantaneous
   * strobe rate) */
  return state->rate_est;
}
