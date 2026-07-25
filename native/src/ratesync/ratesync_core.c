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

/* Clear everything that carries across inputs; config is untouched. */
static void
seed (ratesync_state_t *s)
{
  s->ctrl       = 0.0;
  s->last_error = 0.0;
  s->pwr_avg    = 0.0;
  s->pwr_seeded = 0;
  s->rate_est   = s->sps;
  s->have_prev  = 0;
  s->out_count  = 0;
  s->ring_n     = 0;
  s->prev_on    = 0.0f;
  memset (s->ring, 0, sizeof (s->ring));
  s->lock_sum   = 0.0;
  s->lock_count = 0;
  s->lock_stat  = 0.0;
  lockdet_reset (&s->lock);

  /* Terminal-stage OUTPUTS to discard while the cascade's delay lines fill.
     Counting outputs rather than strobes is what makes this rate-independent:
     the bank spans num_taps of its own input samples, and since the terminal
     stage never interpolates here (rate = m/sps <= 1) it cannot emit more
     than num_taps outputs while those inputs arrive. Discarding num_taps
     outputs therefore always covers the fill, whatever m and sps are.
     Steering during the fill is meaningless — the eye statistic swings over
     its whole +-2 range there — and costs an acquisition. */
  size_t ntaps = 0;
  int    last  = s->mf->n_stages - 1;
  if (last >= 0 && s->mf->stage_types[last] == RC_STAGE_RESAMP)
    ntaps = resamp_get_num_taps (
        (const resamp_state_t *)s->mf->stage_ptrs[last]);
  s->prime_left = ntaps + 1u;
}

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

  s->sps        = sps;
  s->pulse      = pulse;
  s->beta       = beta;
  s->span       = span;
  s->m          = m;
  s->num_phases = num_phases;
  s->bn         = bn;
  s->zeta       = zeta;
  s->ted        = ted;

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

  /* Loop update period is one symbol: the TED fires once per on-time strobe,
     and bn is normalised to the symbol rate. */
  loop_filter_init (&s->lf, bn, zeta, 1.0);
  s->avgs = RATESYNC_LOCK_DEFAULT_AVGS;
  lockdet_init (&s->lock, RATESYNC_LOCK_DEFAULT_THRESH,
                RATESYNC_LOCK_DEFAULT_THRESH, RATESYNC_LOCK_DEFAULT_N_UP,
                RATESYNC_LOCK_DEFAULT_N_DOWN);
  seed (s);
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
  loop_filter_reset (&state->lf);
  seed (state);
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
  if (state->ted == RATESYNC_TED_DTTL)
    {
      for (size_t i = 0; i < x_len && n < max_out; i++)
        if (ratesync_step_ted (state, x[i], &out[n], RATESYNC_TED_DTTL))
          {
            n++;
            if (state->tlm.ctx)
              ratesync_tlm_flush (state);
          }
    }
  else
    {
      for (size_t i = 0; i < x_len && n < max_out; i++)
        if (ratesync_step_ted (state, x[i], &out[n], RATESYNC_TED_GARDNER))
          {
            n++;
            if (state->tlm.ctx)
              ratesync_tlm_flush (state);
          }
    }
  return n;
}

void
ratesync_configure (ratesync_state_t *state, double bn, double zeta)
{
  if (!(bn >= 0.0) || !(zeta > 0.0))
    return;
  state->bn   = bn;
  state->zeta = zeta;
  /* loop_filter_init does not touch integ, so a retune preserves the lock. */
  loop_filter_init (&state->lf, bn, zeta, 1.0);
}

double
ratesync_get_bn (const ratesync_state_t *state)
{
  return state->bn;
}

void
ratesync_set_bn (ratesync_state_t *state, double val)
{
  ratesync_configure (state, val, state->zeta);
}

double
ratesync_get_timing_error (const ratesync_state_t *state)
{
  return state->last_error;
}

double
ratesync_get_rate (const ratesync_state_t *state)
{
  return state->rate_est;
}

double
ratesync_get_ctrl (const ratesync_state_t *state)
{
  return state->ctrl;
}

double
ratesync_get_lock_stat (const ratesync_state_t *state)
{
  return state->lock_stat;
}

int
ratesync_get_locked (const ratesync_state_t *state)
{
  return state->lock.locked;
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
  state->avgs = avgs < 1u ? 1u : avgs;
  lockdet_init (&state->lock, up_thresh, down_thresh, n_up, n_down);
  /* Drop the in-flight block and the decision: the next call must be made
     from looks gathered entirely under the new geometry. */
  state->lock_sum   = 0.0;
  state->lock_count = 0;
  state->lock_stat  = 0.0;
  lockdet_reset (&state->lock);
}

int
ratesync_set_telemetry (ratesync_state_t *state, dp_tlm_t *tlm,
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
  state->tlm.id_e      = id_e;
  state->tlm.id_ctrl   = id_ctrl;
  state->tlm.id_rate   = id_rate;
  state->tlm.id_lock   = id_lock;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* ── Serializable state — this object's scalars, then the children ──────────
 *
 * A composition: envelope, our own running state, then the cascade's and the
 * loop filter's self-validating sub-blobs. The strobe ring and its counters
 * are running state too — a resumed instance that forgot which output was
 * on-time would restart the parity search, and the prime countdown must not
 * re-arm on a stream that is already flowing. */

/* Scalars packed between the envelope and the children. */
#define _RS_DOUBLES                                                           \
  6                /* ctrl,last_error,pwr_avg,rate_est,lock_sum,lock_stat     \
                    */
#define _RS_U64S 4 /* pwr_seeded|have_prev, prime_left, out_count, ring_n */

size_t
ratesync_state_bytes (const ratesync_state_t *state)
{
  return sizeof (dp_state_hdr_t) + _RS_DOUBLES * sizeof (double)
         + _RS_U64S * sizeof (uint64_t) + sizeof (uint64_t) /* lock_count */
         + (RATESYNC_MAX_M / 2 + 1 + 1)
               * sizeof (float complex) /* ring+prev */
         + sizeof (uint32_t) * 2        /* lockdet cnt, locked */
         + RateConverter_state_bytes (state->mf)
         + loop_filter_state_bytes (&state->lf);
}

void
ratesync_get_state (const ratesync_state_t *state, void *blob)
{
  const size_t total = ratesync_state_bytes (state);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, RATESYNC_STATE_MAGIC, RATESYNC_STATE_VERSION, total);
  dp_w_f64 (&w, state->ctrl);
  dp_w_f64 (&w, state->last_error);
  dp_w_f64 (&w, state->pwr_avg);
  dp_w_f64 (&w, state->rate_est);
  dp_w_f64 (&w, state->lock_sum);
  dp_w_f64 (&w, state->lock_stat);
  dp_w_u64 (&w, (uint64_t)((state->pwr_seeded ? 1u : 0u)
                           | (state->have_prev ? 2u : 0u)));
  dp_w_u64 (&w, (uint64_t)state->prime_left);
  dp_w_u64 (&w, (uint64_t)state->out_count);
  dp_w_u64 (&w, (uint64_t)state->ring_n);
  dp_w_u64 (&w, (uint64_t)state->lock_count);
  dp_w_bytes (&w, state->ring, sizeof (state->ring));
  dp_w_bytes (&w, &state->prev_on, sizeof (state->prev_on));
  dp_w_u32 (&w, state->lock.cnt);
  dp_w_u32 (&w, (uint32_t)state->lock.locked);

  char *p = (char *)blob + w.off;
  RateConverter_get_state (state->mf, p);
  p += RateConverter_state_bytes (state->mf);
  loop_filter_get_state (&state->lf, p);
}

int
ratesync_set_state (ratesync_state_t *state, const void *blob)
{
  const size_t total = ratesync_state_bytes (state);
  int          rc    = dp_state_validate (blob, total, RATESYNC_STATE_MAGIC,
                                          RATESYNC_STATE_VERSION);
  if (rc != DP_OK)
    return rc;

  dp_reader_t r     = dp_reader_init (blob, total);
  r.off             = sizeof (dp_state_hdr_t);
  state->ctrl       = dp_r_f64 (&r);
  state->last_error = dp_r_f64 (&r);
  state->pwr_avg    = dp_r_f64 (&r);
  state->rate_est   = dp_r_f64 (&r);
  state->lock_sum   = dp_r_f64 (&r);
  state->lock_stat  = dp_r_f64 (&r);
  uint64_t flags    = dp_r_u64 (&r);
  state->pwr_seeded = (flags & 1u) ? 1 : 0;
  state->have_prev  = (flags & 2u) ? 1 : 0;
  state->prime_left = (size_t)dp_r_u64 (&r);
  state->out_count  = (size_t)dp_r_u64 (&r);
  state->ring_n     = (size_t)dp_r_u64 (&r);
  state->lock_count = (size_t)dp_r_u64 (&r);
  dp_r_bytes (&r, state->ring, sizeof (state->ring));
  dp_r_bytes (&r, &state->prev_on, sizeof (state->prev_on));
  state->lock.cnt    = dp_r_u32 (&r);
  state->lock.locked = (int)dp_r_u32 (&r);

  const char *p = (const char *)blob + r.off;
  rc            = RateConverter_set_state (state->mf, p);
  if (rc != DP_OK)
    return rc;
  p += RateConverter_state_bytes (state->mf);
  return loop_filter_set_state (&state->lf, p);
}
