#include "symsync/symsync_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Default timing-lock-detector rule (see symsync_configure_lock's header
 * doc). Calibrated by direct Monte-Carlo against the real object (not a
 * numpy stand-in), sweeping both pulse shapes doppler ships (raised-
 * cosine matched-filter and rectangular NRZ/I&D) and 40 independent
 * noise-only seeds at SYMSYNC_LOCK_ALPHA = 0.01:
 *   noise-only lock_metric:  max 0.33, mean 0.07  (40 seeds, 4000 syms)
 *   RC-shaped BPSK, 0 dB SNR (worst signal case tested): 0.52
 *   RC-shaped BPSK, 4-20 dB SNR:                         0.73-0.93
 *   rectangular NRZ, noiseless:                          1.00 (exact)
 * up/down sit with real margin above the noise ceiling and below every
 * signal case tested, including the extreme 0 dB SNR point. A first
 * design (an eye-concentration ratio keyed to the mid-symbol sample)
 * was rejected after calibration surfaced a pulse-shape-dependent sign
 * flip (locked pushed the statistic up for rectangular pulses, down for
 * raised-cosine); a second design (this metric's EMA fed through a
 * 1/(1+x) reciprocal) was rejected after calibration showed Jensen's
 * inequality biases that EMA upward under noise, nearly erasing the
 * H0/H1 gap. This is the third design: EMA the normalized on-time power
 * deviation directly (unbiased), then map through a bounded linear
 * clamp -- see symsync_step_ted()'s lock-metric comment. The slow alpha
 * (vs. Costas's 0.1) is needed because the underlying per-symbol
 * statistic is far heavier-tailed than Costas's bounded phase ratio; the
 * verify counts mirror Costas's declare-fast/drop-reluctant shape. */
#define SYMSYNC_LOCK_DEFAULT_UP 0.45
#define SYMSYNC_LOCK_DEFAULT_DOWN 0.40
#define SYMSYNC_LOCK_DEFAULT_N_UP 8u
#define SYMSYNC_LOCK_DEFAULT_N_DOWN 32u

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
  s->nvar_ema         = 1.0; /* pessimistic seed: consistency derives to 0 */
  s->lock_metric      = 0.0;
  lockdet_reset (&s->lock); /* drop the lock; keep the configured rule */
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
  lockdet_init (&s->lock, SYMSYNC_LOCK_DEFAULT_UP, SYMSYNC_LOCK_DEFAULT_DOWN,
                SYMSYNC_LOCK_DEFAULT_N_UP, SYMSYNC_LOCK_DEFAULT_N_DOWN);
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
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_freq < 0 || id_rate < 0 || id_lock < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  state->tlm.id_e      = id_e;
  state->tlm.id_freq   = id_freq;
  state->tlm.id_rate   = id_rate;
  state->tlm.id_lock   = id_lock;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
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
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_metric);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
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

double
symsync_get_lock_metric (const symsync_state_t *state)
{
  return state->lock_metric;
}

int
symsync_get_locked (const symsync_state_t *state)
{
  return state->lock.locked;
}

void
symsync_configure_lock (symsync_state_t *state, double up_thresh,
                        double down_thresh, uint32_t n_up, uint32_t n_down)
{
  lockdet_configure (&state->lock, up_thresh, down_thresh, n_up, n_down);
}
