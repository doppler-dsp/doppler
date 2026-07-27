/*
 * ratesync_core.c — symbol-timing recovery closed around a matched-filter
 * rate cascade.
 *
 * The object owns one `RateConverter` whose terminal stage carries the pulse;
 * everything here is lifecycle, configuration and serialization. The per-input
 * work is the force-inlined ratesync_step_ted() in the header, and the design
 * arguments (why the TED normaliser is the energy SUM, why the loop stays open
 * until the cascade is primed, why the T/2 parity does not matter) live there
 * too.
 */
#include "ratesync/ratesync_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Lock-detector defaults.
 *
 * lock_signal here is the SAME statistic symsync block-averages --
 * 2*(|on|^2 - |mid|^2)/(|on|^2 + |mid|^2), formed from an on-time strobe and a
 * transition-gate strobe half a symbol earlier -- so symsync's empirically
 * validated operating point transfers directly: avgs = 133, threshold = 0.311
 * (its (pfa=1e-3, pd=0.9) sizing at rolloff 0.35 / esno_min 10 dB), n_up = 1,
 * n_down = 8.
 *
 * The (pfa, pd) SIZING entry point (symsync_configure_lock) is deliberately
 * NOT mirrored: its constants were calibrated against symsync's own geometry
 * by Monte Carlo, and re-exposing the formula for a different front end
 * without repeating that validation would be asserting a calibration nobody
 * measured. ratesync_configure_lock_raw() exposes every knob for a caller that
 * sizes its own. */
#define RATESYNC_LOCK_DEFAULT_AVGS 133u
#define RATESYNC_LOCK_DEFAULT_THRESH 0.311
#define RATESYNC_LOCK_DEFAULT_N_UP 1u
#define RATESYNC_LOCK_DEFAULT_N_DOWN 8u

/* ------------------------------------------------------------------
 * The timing loop on its own — no cascade, no filter.
 * ------------------------------------------------------------------ */

void
ratesync_loop_init (ratesync_loop_t *l, double sps, size_t m, double bn,
                    double zeta, int ted)
{
  l->sps        = sps;
  l->m          = m;
  l->bn         = bn;
  l->zeta       = zeta;
  l->ted        = ted;
  l->term_rate  = 0.0;
  l->prime_taps = 0;
  l->term       = NULL;
  /* Loop update period is one symbol: the TED fires once per on-time strobe,
     and bn is normalised to the symbol rate. */
  loop_filter_init (&l->lf, bn, zeta, 1.0);
  l->avgs = RATESYNC_LOCK_DEFAULT_AVGS;
  lockdet_init (&l->lock, RATESYNC_LOCK_DEFAULT_THRESH,
                RATESYNC_LOCK_DEFAULT_THRESH, RATESYNC_LOCK_DEFAULT_N_UP,
                RATESYNC_LOCK_DEFAULT_N_DOWN);
  l->tlm.ctx = NULL;
  ratesync_loop_reset (l);
}

void
ratesync_loop_set_cascade (ratesync_loop_t *l, double term_rate,
                           size_t prime_taps)
{
  l->term_rate  = term_rate;
  l->prime_taps = prime_taps;
  l->prime_left = prime_taps + 1u;
  /* Geometry given by hand: there is no stage to read `mu` from, and keeping
     a pointer bound by an earlier cascade would report another object's
     phase. The probe reports 0 instead. */
  l->term = NULL;
}

/* Describe the cascade's terminal stage to the loop: the rate `ctrl` is
   referenced to, and the tap count that sets the prime length. Read once at
   create so the hot path never walks the cascade. */
void
ratesync_loop_bind_cascade (ratesync_loop_t             *l,
                            const RateConverter_state_t *rc)
{
  size_t                ntaps     = 0;
  double                term_rate = 0.0;
  const resamp_state_t *term      = NULL;
  int                   last      = rc->n_stages - 1;
  if (last >= 0 && rc->stage_types[last] == RC_STAGE_RESAMP)
    {
      term      = (const resamp_state_t *)rc->stage_ptrs[last];
      ntaps     = resamp_get_num_taps (term);
      term_rate = resamp_get_rate (term);
    }
  ratesync_loop_set_cascade (l, term_rate, ntaps);
  /* AFTER set_cascade, which clears it: keep the stage itself for the `mu`
     probe. The loop steers this accumulator, so its phase is the loop's own
     output, but the cascade does not report it any other way. */
  l->term = term;
}

void
ratesync_loop_reset (ratesync_loop_t *l)
{
  loop_filter_reset (&l->lf);
  l->ctrl       = 0.0;
  l->last_error = 0.0;
  l->pwr_avg    = 0.0;
  l->pwr_seeded = 0;
  l->rate_est   = l->sps;
  l->have_prev  = 0;
  l->out_count  = 0;
  l->ring_n     = 0;
  l->prev_on    = 0.0f;
  memset (l->ring, 0, sizeof (l->ring));
  l->lock_sum   = 0.0;
  l->lock_count = 0;
  l->lock_stat  = 0.0;
  lockdet_reset (&l->lock);
  /* Terminal-stage OUTPUTS to discard while the cascade's delay lines fill.
     Counting outputs rather than strobes is what makes this rate-independent:
     the bank spans num_taps of its own input samples, and since the terminal
     stage never interpolates here (rate = m/sps <= 1) it cannot emit more
     than num_taps outputs while those inputs arrive. Discarding num_taps
     outputs therefore always covers the fill, whatever m and sps are.
     Steering during the fill is meaningless — the eye statistic swings over
     its whole +-2 range there — and costs an acquisition. */
  l->prime_left = l->prime_taps + 1u;
}

void
ratesync_loop_configure (ratesync_loop_t *l, double bn, double zeta)
{
  if (!(bn >= 0.0) || !(zeta > 0.0))
    return;
  l->bn   = bn;
  l->zeta = zeta;
  /* loop_filter_init does not touch integ, so a retune preserves the lock. */
  loop_filter_init (&l->lf, bn, zeta, 1.0);
}

void
ratesync_loop_configure_lock_raw (ratesync_loop_t *l, size_t avgs,
                                  double up_thresh, double down_thresh,
                                  uint32_t n_up, uint32_t n_down)
{
  l->avgs = avgs < 1u ? 1u : avgs;
  lockdet_init (&l->lock, up_thresh, down_thresh, n_up, n_down);
  /* Drop the in-flight block and the decision: the next call must be made
     from looks gathered entirely under the new geometry. */
  l->lock_sum   = 0.0;
  l->lock_count = 0;
  l->lock_stat  = 0.0;
  lockdet_reset (&l->lock);
}

void
ratesync_loop_tlm_flush (const ratesync_loop_t *l)
{
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_e, l->last_error);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_ctrl, l->ctrl);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_rate, l->rate_est);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_lock, l->lock_stat);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_locked, (double)l->lock.locked);
  /* The sampling phase this steering produced. 0 when the geometry was bound
     by hand and there is no stage to read (see ratesync_loop_t::term). */
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_mu,
               l->term ? resamp_get_ctrl_acc (l->term) : 0.0);
}

int
ratesync_loop_set_telemetry (ratesync_loop_t *l, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      l->tlm.ctx = NULL;
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
  (void)snprintf (name, sizeof (name), "%s.mu", p);
  int id_mu = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_ctrl < 0 || id_rate < 0 || id_lock < 0 || id_locked < 0
      || id_mu < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  l->tlm.id_e      = id_e;
  l->tlm.id_ctrl   = id_ctrl;
  l->tlm.id_rate   = id_rate;
  l->tlm.id_lock   = id_lock;
  l->tlm.id_locked = id_locked;
  l->tlm.id_mu     = id_mu;
  l->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* ── Serializable state — the loop's running scalars, then its child ────────
 *
 * The strobe ring and its counters are running state too — a resumed instance
 * that forgot which output was on-time would restart the parity search, and
 * the prime countdown must not re-arm on a stream that is already flowing. */

/* Scalars packed between the envelope and the child. */
#define _RS_DOUBLES                                                           \
  6                /* ctrl,last_error,pwr_avg,rate_est,lock_sum,lock_stat     \
                    */
#define _RS_U64S 4 /* pwr_seeded|have_prev, prime_left, out_count, ring_n */

size_t
ratesync_loop_state_bytes (const ratesync_loop_t *l)
{
  return sizeof (dp_state_hdr_t) + _RS_DOUBLES * sizeof (double)
         + _RS_U64S * sizeof (uint64_t) + sizeof (uint64_t) /* lock_count */
         + (RATESYNC_MAX_M / 2 + 1 + 1)
               * sizeof (float complex) /* ring+prev */
         + sizeof (uint32_t) * 2        /* lockdet cnt, locked */
         + loop_filter_state_bytes (&l->lf);
}

void
ratesync_loop_get_state (const ratesync_loop_t *l, void *blob)
{
  const size_t total = ratesync_loop_state_bytes (l);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, RATESYNC_LOOP_STATE_MAGIC, RATESYNC_LOOP_STATE_VERSION, total);
  dp_w_f64 (&w, l->ctrl);
  dp_w_f64 (&w, l->last_error);
  dp_w_f64 (&w, l->pwr_avg);
  dp_w_f64 (&w, l->rate_est);
  dp_w_f64 (&w, l->lock_sum);
  dp_w_f64 (&w, l->lock_stat);
  dp_w_u64 (&w,
            (uint64_t)((l->pwr_seeded ? 1u : 0u) | (l->have_prev ? 2u : 0u)));
  dp_w_u64 (&w, (uint64_t)l->prime_left);
  dp_w_u64 (&w, (uint64_t)l->out_count);
  dp_w_u64 (&w, (uint64_t)l->ring_n);
  dp_w_u64 (&w, (uint64_t)l->lock_count);
  dp_w_bytes (&w, l->ring, sizeof (l->ring));
  dp_w_bytes (&w, &l->prev_on, sizeof (l->prev_on));
  dp_w_u32 (&w, l->lock.cnt);
  dp_w_u32 (&w, (uint32_t)l->lock.locked);
  loop_filter_get_state (&l->lf, (char *)blob + w.off);
}

int
ratesync_loop_set_state (ratesync_loop_t *l, const void *blob)
{
  const size_t total = ratesync_loop_state_bytes (l);
  int          rc = dp_state_validate (blob, total, RATESYNC_LOOP_STATE_MAGIC,
                                       RATESYNC_LOOP_STATE_VERSION);
  if (rc != DP_OK)
    return rc;

  dp_reader_t r  = dp_reader_init (blob, total);
  r.off          = sizeof (dp_state_hdr_t);
  l->ctrl        = dp_r_f64 (&r);
  l->last_error  = dp_r_f64 (&r);
  l->pwr_avg     = dp_r_f64 (&r);
  l->rate_est    = dp_r_f64 (&r);
  l->lock_sum    = dp_r_f64 (&r);
  l->lock_stat   = dp_r_f64 (&r);
  uint64_t flags = dp_r_u64 (&r);
  l->pwr_seeded  = (flags & 1u) ? 1 : 0;
  l->have_prev   = (flags & 2u) ? 1 : 0;
  l->prime_left  = (size_t)dp_r_u64 (&r);
  l->out_count   = (size_t)dp_r_u64 (&r);
  l->ring_n      = (size_t)dp_r_u64 (&r);
  l->lock_count  = (size_t)dp_r_u64 (&r);
  dp_r_bytes (&r, l->ring, sizeof (l->ring));
  dp_r_bytes (&r, &l->prev_on, sizeof (l->prev_on));
  l->lock.cnt    = dp_r_u32 (&r);
  l->lock.locked = (int)dp_r_u32 (&r);
  return loop_filter_set_state (&l->lf, (const char *)blob + r.off);
}

/* ------------------------------------------------------------------
 * RateSync — the cascade, plus the loop above
 * ------------------------------------------------------------------ */

ratesync_state_t *
ratesync_create (double sps, int pulse, double beta, size_t span, size_t m,
                 size_t num_phases, double bn, double zeta, int ted)
{
  /* Written as !(x >= y) so a NaN parameter is rejected, not accepted. */
  if (!(beta >= 0.0) || !(beta <= 1.0) || span < 1 || m < 2u
      || m > (size_t)RATESYNC_MAX_M || (m & 1u) != 0u || num_phases < 2u
      || (num_phases & (num_phases - 1u)) != 0u || !(bn >= 0.0)
      || !(zeta > 0.0)
      || (pulse != RATESYNC_PULSE_IANDD && pulse != RATESYNC_PULSE_RRC)
      || (ted != RATESYNC_TED_GARDNER && ted != RATESYNC_TED_DTTL))
    return NULL;
  /* rate = m/sps must not exceed 1: the terminal stage is a decimator here,
     and asking it to interpolate would let one input emit several strobes,
     which the single-symbol step() contract cannot express. */
  if (!(sps >= (double)m))
    return NULL;

  ratesync_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->pulse      = pulse;
  s->beta       = beta;
  s->span       = span;
  s->num_phases = num_phases;

  /* compensate = 1 unconditionally: on this path the CIC droop compensator
     folds into the terminal bank (six taps per arm, no extra stage, no extra
     pass) and is worth ~28 dB of EVM whenever the plan contains a CIC. There
     is no configuration under which paying for it is wrong. */
  s->mf = RateConverter_create_matched ((double)m / sps, 1, pulse, beta, span,
                                        (double)m, num_phases);
  if (!s->mf)
    {
      free (s);
      return NULL;
    }

  ratesync_loop_init (&s->loop, sps, m, bn, zeta, ted);
  ratesync_loop_bind_cascade (&s->loop, s->mf);
  return s;
}

void
ratesync_destroy (ratesync_state_t *state)
{
  if (!state)
    return;
  RateConverter_destroy (state->mf);
  free (state);
}

void
ratesync_reset (ratesync_state_t *state)
{
  RateConverter_reset (state->mf);
  ratesync_loop_reset (&state->loop);
}

size_t
ratesync_steps_max_out (ratesync_state_t *state)
{
  (void)state;
  return 0; /* sps >= m >= 2, so symbols <= inputs; the caller's length wins */
}

size_t
ratesync_steps (ratesync_state_t *state, const float complex *x, size_t x_len,
                float complex *out, size_t max_out)
{
  size_t n = 0;
  /* Specialise on the configured detector so the branch folds away, exactly
     as symsync_steps does. */
  if (state->loop.ted == RATESYNC_TED_DTTL)
    {
      for (size_t i = 0; i < x_len && n < max_out; i++)
        if (ratesync_step_ted (state, x[i], &out[n], RATESYNC_TED_DTTL))
          {
            n++;
            if (state->loop.tlm.ctx)
              ratesync_loop_tlm_flush (&state->loop);
          }
    }
  else
    {
      for (size_t i = 0; i < x_len && n < max_out; i++)
        if (ratesync_step_ted (state, x[i], &out[n], RATESYNC_TED_GARDNER))
          {
            n++;
            if (state->loop.tlm.ctx)
              ratesync_loop_tlm_flush (&state->loop);
          }
    }
  return n;
}

void
ratesync_configure (ratesync_state_t *state, double bn, double zeta)
{
  ratesync_loop_configure (&state->loop, bn, zeta);
}

double
ratesync_get_bn (const ratesync_state_t *state)
{
  return state->loop.bn;
}

void
ratesync_set_bn (ratesync_state_t *state, double val)
{
  ratesync_configure (state, val, state->loop.zeta);
}

double
ratesync_get_timing_error (const ratesync_state_t *state)
{
  return state->loop.last_error;
}

double
ratesync_get_rate (const ratesync_state_t *state)
{
  return state->loop.rate_est;
}

double
ratesync_get_ctrl (const ratesync_state_t *state)
{
  return state->loop.ctrl;
}

double
ratesync_get_lock_stat (const ratesync_state_t *state)
{
  return state->loop.lock_stat;
}

int
ratesync_get_locked (const ratesync_state_t *state)
{
  return state->loop.lock.locked;
}

int
ratesync_get_clipped (const ratesync_state_t *state)
{
  return RateConverter_get_clipped (state->mf);
}

void
ratesync_configure_lock_raw (ratesync_state_t *state, size_t avgs,
                             double up_thresh, double down_thresh,
                             uint32_t n_up, uint32_t n_down)
{
  ratesync_loop_configure_lock_raw (&state->loop, avgs, up_thresh, down_thresh,
                                    n_up, n_down);
}

int
ratesync_set_telemetry (ratesync_state_t *state, dp_tlm_t *tlm,
                        const char *prefix, uint32_t decim)
{
  return ratesync_loop_set_telemetry (&state->loop, tlm, prefix, decim);
}

/* ── Serializable state — two children, no scalars of our own ───────────────
 *
 * Everything this object carries across inputs now lives in the timing loop,
 * so the blob is the envelope followed by the cascade's and the loop's
 * self-validating sub-blobs. Both children validate their own envelope, so a
 * mismatched cascade or loop is rejected rather than reinterpreted. */

size_t
ratesync_state_bytes (const ratesync_state_t *state)
{
  return sizeof (dp_state_hdr_t) + RateConverter_state_bytes (state->mf)
         + ratesync_loop_state_bytes (&state->loop);
}

void
ratesync_get_state (const ratesync_state_t *state, void *blob)
{
  const size_t total = ratesync_state_bytes (state);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, RATESYNC_STATE_MAGIC, RATESYNC_STATE_VERSION, total);
  char *p = (char *)blob + w.off;
  RateConverter_get_state (state->mf, p);
  p += RateConverter_state_bytes (state->mf);
  ratesync_loop_get_state (&state->loop, p);
}

int
ratesync_set_state (ratesync_state_t *state, const void *blob)
{
  const size_t total = ratesync_state_bytes (state);
  int          rc    = dp_state_validate (blob, total, RATESYNC_STATE_MAGIC,
                                          RATESYNC_STATE_VERSION);
  if (rc != DP_OK)
    return rc;

  const char *p = (const char *)blob + sizeof (dp_state_hdr_t);
  rc            = RateConverter_set_state (state->mf, p);
  if (rc != DP_OK)
    return rc;
  p += RateConverter_state_bytes (state->mf);
  return ratesync_loop_set_state (&state->loop, p);
}
