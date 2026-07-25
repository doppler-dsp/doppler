/*
 * ratesync_core.c — RRC matched filter fused with symbol-timing recovery.
 *
 * The object owns two `resamp` children built from RRC banks that differ
 * only by a half-symbol displacement, both run at rate = 1/sps through the
 * streaming control port. See ratesync_core.h for why the roles are pinned
 * that way rather than by an even/odd output parity.
 */
#include "ratesync/ratesync_core.h"

#include "wfm/wfm_dsp.h" /* wfm_rrc_h — the RRC formula's one home */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lock-detector defaults.
 *
 * lock_signal here is the SAME statistic symsync block-averages --
 * 2*(|on-time|^2 - |mid|^2)/(|on-time|^2 + |mid|^2), formed from an on-time
 * strobe and a transition-gate strobe half a symbol earlier -- so symsync's
 * empirically validated operating point transfers directly: avgs = 133,
 * threshold = 0.311 (its (pfa=1e-3, pd=0.9) sizing at rolloff 0.35 /
 * esno_min 10 dB), n_up = 1, n_down = 8.
 *
 * The (pfa, pd) SIZING entry point (symsync_configure_lock) is deliberately
 * NOT mirrored here: its constants were calibrated against symsync's own
 * geometry by Monte Carlo, and re-exposing the formula for a different
 * front end without repeating that validation would be asserting a
 * calibration nobody measured. ratesync_configure_lock_raw() exposes every
 * knob for a caller that sizes its own, and a validated sizing entry point
 * can be added once the same Monte Carlo is run against this object. */
#define RATESYNC_LOCK_DEFAULT_AVGS 133u
#define RATESYNC_LOCK_DEFAULT_THRESH 0.311
#define RATESYNC_LOCK_DEFAULT_N_UP 1u
#define RATESYNC_LOCK_DEFAULT_N_DOWN 8u

double
ratesync_pulse_h (int pulse, double t, double beta)
{
  if (pulse == RATESYNC_PULSE_IANDD)
    /* Unit rectangle over one symbol — the matched filter for a rectangular
       symbol, and exactly what an integrate-and-dump computes. Half-open at
       the trailing edge so two adjacent symbols never both claim t = ±0.5. */
    return (t >= -0.5 && t < 0.5) ? 1.0 : 0.0;
  return wfm_rrc_h (t, beta); /* the canonical RRC, one home */
}

double
ratesync_pulse_support (int pulse, size_t span)
{
  /* The rectangle is one symbol wide whatever `span` says. */
  return pulse == RATESYNC_PULSE_IANDD ? 0.5 : (double)span;
}

size_t
ratesync_bank_ntaps (int pulse, double sps, size_t span)
{
  double support = ratesync_pulse_support (pulse, span);
  return (size_t)ceil ((2.0 * support + 0.5) * sps) + 1u;
}

void
ratesync_bank (int pulse, double beta, double sps, size_t span,
               size_t num_phases, size_t num_taps, double offset_sym,
               float *bank)
{
  double support = ratesync_pulse_support (pulse, span);
  for (size_t p = 0; p < num_phases; p++)
    {
      /* Arm p moves the sampling instant by p/num_phases of an output
         period, in the SAME direction crossing an emission boundary moves
         it — the accumulator is the only timing authority and this is its
         fractional read-out (see the header). */
      double arm = (double)p / (double)num_phases;
      for (size_t t = 0; t < num_taps; t++)
        {
          /* tap t multiplies x[n-t]: the delay line is newest-first. */
          double ts = -(double)t / sps + support + offset_sym + arm;
          bank[p * num_taps + t] = (float)ratesync_pulse_h (pulse, ts, beta);
        }
    }
}

ratesync_state_t *
ratesync_create (double sps, int pulse, double beta, size_t span,
                 size_t num_phases, double bn, double zeta, int ted)
{
  /* Written as !(x >= y) so a NaN parameter is rejected, not accepted. */
  if (!(sps >= 1.0) || !(beta >= 0.0) || !(beta <= 1.0) || span < 1
      || num_phases < 2u || (num_phases & (num_phases - 1u)) != 0u
      || !(bn >= 0.0) || !(zeta > 0.0)
      || (pulse != RATESYNC_PULSE_IANDD && pulse != RATESYNC_PULSE_RRC))
    return NULL;

  ratesync_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  size_t ntaps = ratesync_bank_ntaps (pulse, sps, span);
  float *b_on  = malloc (num_phases * ntaps * sizeof (float));
  float *b_mid = malloc (num_phases * ntaps * sizeof (float));
  if (!b_on || !b_mid)
    {
      free (b_on);
      free (b_mid);
      free (s);
      return NULL;
    }

  ratesync_bank (pulse, beta, sps, span, num_phases, ntaps, 0.0, b_on);
  ratesync_bank (pulse, beta, sps, span, num_phases, ntaps, 0.5, b_mid);

  /* ONE common scale for both banks: the TED and the lock statistic compare
     |on| against |mid| directly, so normalising each bank independently
     would bake a gain difference into the comparison. */
  double e0 = 0.0;
  for (size_t t = 0; t < ntaps; t++)
    e0 += (double)b_on[t] * (double)b_on[t];
  float g = (float)(e0 > 0.0 ? 1.0 / sqrt (e0) : 1.0);
  for (size_t i = 0; i < num_phases * ntaps; i++)
    {
      b_on[i] *= g;
      b_mid[i] *= g;
    }

  double rate = 1.0 / sps;
  s->mf_on    = resamp_create_custom (num_phases, ntaps, b_on, rate);
  s->mf_mid   = resamp_create_custom (num_phases, ntaps, b_mid, rate);
  free (b_on);
  free (b_mid);
  if (!s->mf_on || !s->mf_mid)
    {
      ratesync_destroy (s);
      return NULL;
    }

  s->sps        = sps;
  s->pulse      = pulse;
  s->beta       = beta;
  s->span       = span;
  s->num_phases = num_phases;
  s->num_taps   = ntaps;
  s->bn         = bn;
  s->zeta       = zeta;
  s->ted
      = (ted == RATESYNC_TED_DTTL) ? RATESYNC_TED_DTTL : RATESYNC_TED_GARDNER;
  s->rate_est = sps;
  s->pwr_avg  = 1.0;
  s->avgs     = RATESYNC_LOCK_DEFAULT_AVGS;

  loop_filter_init (&s->lf, bn, zeta, 1.0); /* one update per SYMBOL */
  lockdet_init (&s->lock, RATESYNC_LOCK_DEFAULT_THRESH,
                RATESYNC_LOCK_DEFAULT_THRESH, RATESYNC_LOCK_DEFAULT_N_UP,
                RATESYNC_LOCK_DEFAULT_N_DOWN);
  return s;
}

void
ratesync_destroy (ratesync_state_t *s)
{
  if (!s)
    return;
  resamp_destroy (s->mf_on);
  resamp_destroy (s->mf_mid);
  free (s);
}

void
ratesync_reset (ratesync_state_t *s)
{
  resamp_reset (s->mf_on);
  resamp_reset (s->mf_mid);
  loop_filter_reset (&s->lf);
  lockdet_reset (&s->lock);
  s->ctrl       = 0.0;
  s->last_error = 0.0;
  s->pwr_avg    = 1.0;
  s->rate_est   = s->sps;
  s->have_on    = 0;
  s->prev_on    = 0.0f;
  s->lock_sum   = 0.0;
  s->lock_count = 0;
  s->lock_stat  = 0.0;
}

size_t
ratesync_steps_max_out (ratesync_state_t *s)
{
  (void)s;
  return 0;
}

size_t
ratesync_steps (ratesync_state_t *s, const float complex *x, size_t x_len,
                float complex *out, size_t max_out)
{
  size_t        emitted = 0;
  float complex y;
  /* The TED selection is hoisted out of the hot loop so the force-inlined
     body constant-folds the detector branch away (symsync measured ~30%
     for the same specialisation). */
  if (s->ted == RATESYNC_TED_DTTL)
    {
      for (size_t i = 0; i < x_len && emitted < max_out; i++)
        if (ratesync_step_ted (s, x[i], &y, RATESYNC_TED_DTTL))
          {
            out[emitted++] = y;
            if (s->tlm.ctx)
              ratesync_tlm_flush (s);
          }
    }
  else
    {
      for (size_t i = 0; i < x_len && emitted < max_out; i++)
        if (ratesync_step_ted (s, x[i], &y, RATESYNC_TED_GARDNER))
          {
            out[emitted++] = y;
            if (s->tlm.ctx)
              ratesync_tlm_flush (s);
          }
    }
  return emitted;
}

/* ------------------------------------------------------------------ */
/* Properties / configuration                                          */
/* ------------------------------------------------------------------ */

void
ratesync_configure (ratesync_state_t *s, double bn, double zeta)
{
  if (!(bn >= 0.0) || !(zeta > 0.0))
    return;
  s->bn   = bn;
  s->zeta = zeta;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* preserves the integrator */
}

double
ratesync_get_bn (const ratesync_state_t *s)
{
  return s->bn;
}

void
ratesync_set_bn (ratesync_state_t *s, double val)
{
  ratesync_configure (s, val, s->zeta);
}

double
ratesync_get_timing_error (const ratesync_state_t *s)
{
  return s->last_error;
}

double
ratesync_get_rate (const ratesync_state_t *s)
{
  return s->rate_est;
}

double
ratesync_get_ctrl (const ratesync_state_t *s)
{
  return s->ctrl;
}

double
ratesync_get_lock_stat (const ratesync_state_t *s)
{
  return s->lock_stat;
}

int
ratesync_get_locked (const ratesync_state_t *s)
{
  return s->lock.locked;
}

void
ratesync_configure_lock_raw (ratesync_state_t *s, size_t avgs,
                             double up_thresh, double down_thresh,
                             uint32_t n_up, uint32_t n_down)
{
  s->avgs = avgs < 1u ? 1u : avgs;
  lockdet_init (&s->lock, up_thresh, down_thresh, n_up, n_down);
  /* Drop the in-flight block so the next decision uses only looks gathered
     under the new geometry. */
  s->lock_sum   = 0.0;
  s->lock_count = 0;
  s->lock_stat  = 0.0;
  lockdet_reset (&s->lock);
}

/* ------------------------------------------------------------------ */
/* Telemetry                                                           */
/* ------------------------------------------------------------------ */

int
ratesync_set_telemetry (ratesync_state_t *s, dp_tlm_t *tlm, const char *prefix,
                        uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      s->tlm.ctx = NULL;
      return DP_OK;
    }
  const char *p = prefix ? prefix : "sync";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.ctrl", p);
  int id_ctrl = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.rate", p);
  int id_rate = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_ctrl < 0 || id_rate < 0 || id_lock < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  s->tlm.id_e      = id_e;
  s->tlm.id_ctrl   = id_ctrl;
  s->tlm.id_rate   = id_rate;
  s->tlm.id_lock   = id_lock;
  s->tlm.id_locked = id_locked;
  s->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
ratesync_tlm_flush (const ratesync_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_ctrl, s->ctrl);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_rate, s->rate_est);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_stat);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
}

/* ------------------------------------------------------------------ */
/* Serializable state — a composition (see dp_state.h)                 */
/* ------------------------------------------------------------------ */

size_t
ratesync_state_bytes (const ratesync_state_t *s)
{
  /* Must match ratesync_get_state() exactly: an over-count leaves
     uninitialised tail bytes in every blob (they resume fine but compare
     unequal, which is what the Python state matrix asserts on). */
  return sizeof (dp_state_hdr_t)
         + 6 * sizeof (double)    /* ctrl, last_error, pwr_avg, rate_est,
                                     lock_stat, lock_sum                   */
         + 2 * sizeof (uint64_t)  /* have_on, lock_count                   */
         + sizeof (float complex) /* prev_on                               */
         + lockdet_state_bytes (&s->lock) + loop_filter_state_bytes (&s->lf)
         + resamp_state_bytes (s->mf_on) + resamp_state_bytes (s->mf_mid);
}

void
ratesync_get_state (const ratesync_state_t *s, void *blob)
{
  DP_GET_OPEN (RATESYNC_STATE_MAGIC, RATESYNC_STATE_VERSION,
               ratesync_state_bytes (s));
  dp_w_f64 (&_w, s->ctrl);
  dp_w_f64 (&_w, s->last_error);
  dp_w_f64 (&_w, s->pwr_avg);
  dp_w_f64 (&_w, s->rate_est);
  dp_w_f64 (&_w, s->lock_stat);
  dp_w_f64 (&_w, s->lock_sum);
  dp_w_u64 (&_w, (uint64_t)s->have_on);
  dp_w_u64 (&_w, (uint64_t)s->lock_count);
  dp_w_cf32 (&_w, &s->prev_on, 1);
  /* Children: each writes its own self-validating envelope. */
  size_t n = lockdet_state_bytes (&s->lock);
  lockdet_get_state (&s->lock, dp_w_reserve (&_w, n));
  n = loop_filter_state_bytes (&s->lf);
  loop_filter_get_state (&s->lf, dp_w_reserve (&_w, n));
  n = resamp_state_bytes (s->mf_on);
  resamp_get_state (s->mf_on, dp_w_reserve (&_w, n));
  n = resamp_state_bytes (s->mf_mid);
  resamp_get_state (s->mf_mid, dp_w_reserve (&_w, n));
}

int
ratesync_set_state (ratesync_state_t *s, const void *blob)
{
  DP_SET_OPEN (RATESYNC_STATE_MAGIC, RATESYNC_STATE_VERSION,
               ratesync_state_bytes (s));
  s->ctrl       = dp_r_f64 (&_r);
  s->last_error = dp_r_f64 (&_r);
  s->pwr_avg    = dp_r_f64 (&_r);
  s->rate_est   = dp_r_f64 (&_r);
  s->lock_stat  = dp_r_f64 (&_r);
  s->lock_sum   = dp_r_f64 (&_r);
  s->have_on    = (int)dp_r_u64 (&_r);
  s->lock_count = (size_t)dp_r_u64 (&_r);
  dp_r_cf32 (&_r, &s->prev_on, 1);

  size_t n  = lockdet_state_bytes (&s->lock);
  int    rc = lockdet_set_state (&s->lock, dp_r_reserve (&_r, n));
  if (rc != DP_OK)
    return rc;
  n  = loop_filter_state_bytes (&s->lf);
  rc = loop_filter_set_state (&s->lf, dp_r_reserve (&_r, n));
  if (rc != DP_OK)
    return rc;
  n  = resamp_state_bytes (s->mf_on);
  rc = resamp_set_state (s->mf_on, dp_r_reserve (&_r, n));
  if (rc != DP_OK)
    return rc;
  n = resamp_state_bytes (s->mf_mid);
  return resamp_set_state (s->mf_mid, dp_r_reserve (&_r, n));
}
