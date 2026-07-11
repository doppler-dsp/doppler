#include "carrier_nda/carrier_nda_core.h"

#include <stdlib.h>
#include <string.h>

/* Default carrier lock-detector rule (see carrier_nda_configure_lock's
 * header doc). up_thresh/down_thresh (0.5/0.4) start from MpskReceiver's
 * own pre-existing acquisition<->tracking handover thresholds
 * (MPSK_RX_HANDOVER_DOWN in mpsk_receiver_core.c), which steps a lockdet
 * on this exact `lock` statistic. n_up was NOT safe to inherit alongside
 * them, though: MpskReceiver's handover verify count (8) assumes
 * consecutive looks compound independently (p^n_up), but `lock` is a
 * fast per-sample EMA (CARRIER_NDA_LOCK_ALPHA, ~20-sample memory) --
 * consecutive samples are highly autocorrelated, so that assumption does
 * not hold (the same independence violation flagged, and designed
 * around, for the DLL's lock statistic and Dll's tumbling vs. sliding
 * window discussion). Direct Monte Carlo against the real object (noise-
 * only, no carrier) confirmed the risk empirically before shipping:
 *   n_up=8  (MpskReceiver's value): 4/30 trials falsely locked
 *   n_up=16:                        7/100 trials falsely locked
 *   n_up=32:                        1/100 trials falsely locked
 *   n_up=64:                        0/300 trials falsely locked
 * n_up=64 is the shipped default (real carrier still locks reliably,
 * 20/20 trials at ~15 dB) -- 64 samples is a small fraction of a
 * typical acquisition window, so this buys reliability at negligible
 * latency cost. */
#define CARRIER_NDA_LOCK_DEFAULT_UP 0.5
#define CARRIER_NDA_LOCK_DEFAULT_DOWN 0.4
#define CARRIER_NDA_LOCK_DEFAULT_N_UP 64u
#define CARRIER_NDA_LOCK_DEFAULT_N_DOWN 32u

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

/* Seed the loop integrator so the per-sample frequency estimate (lf.integ,
 * rad/sample) matches the requested carrier offset, and point the NCO at the
 * same frequency — de-rotation is correct from the first sample, before any
 * update runs. */
static void
seed (carrier_nda_state_t *s, double init_norm_freq)
{
  lo_init (&s->nco, init_norm_freq); /* centre freq lives in nco.phase_inc */
  s->lf.integ = 0.0;                 /* loop filter integrates the correction
                                        from zero; the NCO control port adds it
                                        on top of the centre each sample.      */
  s->ctl_cyc = 0.0;
  /* The boxcar arm starts at unit gain (the AGC drives it from here);
   * boxcar_init clears its whole fixed ring, so the pointer-free POD snapshot
   * is deterministic regardless of allocation — MpskReceiver embeds this by
   * value, with no calloc. */
  boxcar_init (&s->arm, s->arm_len, 1.0);
  s->lock       = 0.0;
  s->last_error = 0.0;
  lockdet_reset (&s->lockdet); /* drop the lock; keep the configured rule */
  /* Embed the log-domain AGC by value (no agc_create): config + reset its loop
   * memory to the post-create condition (gain 0 dB, p_avg pre-seeded to the
   * unit reference power so the first on-target dump produces no transient).
   */
  s->agc.ref_db  = CARRIER_NDA_AGC_REF_DB;
  s->agc.loop_bw = CARRIER_NDA_AGC_BW_RATIO * s->bn;
  s->agc.alpha   = CARRIER_NDA_AGC_ALPHA;
  s->agc.decim   = AGC_DECIM_DEFAULT;
  s->agc.clip_db = CARRIER_NDA_AGC_CLIP_DB;
  s->agc.gain_db = 0.0;
  s->agc.p_avg   = 1.0; /* 10^(ref_db/10) for ref_db = 0 */
  s->agc.g_last  = 1.0;
  /* Decimate the AGC's loop-filter command: agc_step applies the gain and
   * folds power every sample, but refreshes the dB command (the exp10/log10)
   * once per AGC_DECIM_DEFAULT samples — amortising the transcendentals on
   * this sample-rate carrier loop. The AGC is ~100x slower than the carrier
   * loop, so an 8-sample gain hold is negligible. gain_phase/clip_lin are set
   * by the agc_reset-equivalent below. */
  s->agc.gain_update_period = AGC_DECIM_DEFAULT;
  s->agc.gain_phase         = 0;
  s->agc.clip_lin           = (float)agc_exp10_ (s->agc.clip_db * 0.05);
}

/* Configure the carrier PI loop for the current (bn, zeta) and scale its gains
 * by 1/(2*pi) so the loop filter natively outputs the NCO's unit (cycles per
 * sample) from the discriminator's radian phase error. The whole loop then
 * runs in NCO cycles: carrier_nda_steer is a pure loop_filter_step (no
 * per-sample conversion) and lf.integ is the frequency correction in
 * cycles/sample.
 *
 * bn is the loop noise bandwidth in cycles/sample. The moving-average arm
 * emits one sample per input sample, so the loop updates every sample (period
 * t = 1); n only sets the window length, so the gains are n-invariant. Folding
 * the radian->cycle constant into the gains leaves the open-loop product (disc
 * slope x gains x NCO gain) unchanged, so the dynamics are identical. */
static void
config_loop (carrier_nda_state_t *s)
{
  loop_filter_configure (&s->lf, s->bn, s->zeta, 1.0);
  s->lf.kp *= CARRIER_NDA_INV_2PI;
  s->lf.ki *= CARRIER_NDA_INV_2PI;
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
  /* In-place (stack/by-value-embedded) init: start detached — both this
   * loop's attachment and the embedded arm AGC's (seed() configures the
   * AGC field-by-field and deliberately never touches agc.tlm, so a
   * reset keeps a live attachment; that leaves init responsible for the
   * initial zero). carrier_nda_create's calloc gets this for free. */
  memset (&s->tlm, 0, sizeof s->tlm);
  memset (&s->agc.tlm, 0, sizeof s->agc.tlm);
  config_loop (s);
  lockdet_init (&s->lockdet, CARRIER_NDA_LOCK_DEFAULT_UP,
                CARRIER_NDA_LOCK_DEFAULT_DOWN, CARRIER_NDA_LOCK_DEFAULT_N_UP,
                CARRIER_NDA_LOCK_DEFAULT_N_DOWN);
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
  if (sps / (size_t)n > BOXCAR_MAX_LEN)
    return NULL; /* boxcar arm window is a fixed in-struct ring */
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

int
carrier_nda_set_telemetry (carrier_nda_state_t *state, dp_tlm_t *tlm,
                           const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach the loop AND the embedded arm AGC */
    {
      state->tlm.ctx = NULL;
      (void)agc_set_telemetry (&state->agc, NULL, prefix, decim);
      return DP_OK;
    }
  const char *p = prefix ? prefix : "car";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_lock < 0 || id_e < 0 || id_freq < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  /* Forward to the embedded arm AGC under "<prefix>.agc"; if ITS
   * registration fails the whole attach fails and this loop stays
   * detached too (nothing registered above carries state — probe
   * registration is idempotent by name). */
  (void)snprintf (name, sizeof (name), "%s.agc", p);
  int rc = agc_set_telemetry (&state->agc, tlm, name, decim);
  if (rc != DP_OK)
    return rc;
  state->tlm.id_lock   = id_lock;
  state->tlm.id_e      = id_e;
  state->tlm.id_freq   = id_freq;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
carrier_nda_tlm_flush (const carrier_nda_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  /* Tracked carrier = NCO centre + the loop's integrated correction
   * (cycles/sample) — the same reconstruction as get_norm_freq. */
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, s->nco.norm_freq + s->lf.integ);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lockdet.locked);
}

/* Serializable state — pointer-free POD whole-struct snapshot, with the
 * telemetry attachments zeroed in blobs and kept live across restore.
 * Hand-written (not DP_DEFINE_POD_STATE_TLM) because the snapshot spans TWO
 * attachments: this loop's own `tlm` and the by-value-embedded arm AGC's
 * `agc.tlm` — the macro zeroes/preserves a single named member. */
size_t
carrier_nda_state_bytes (const carrier_nda_state_t *s)
{
  (void)s;
  return sizeof (dp_state_hdr_t) + sizeof (carrier_nda_state_t);
}

void
carrier_nda_get_state (const carrier_nda_state_t *s, void *blob)
{
  carrier_nda_state_t c = *s;
  memset (&c.tlm, 0, sizeof c.tlm);
  memset (&c.agc.tlm, 0, sizeof c.agc.tlm);
  dp_writer_t w = dp_writer_init (blob, carrier_nda_state_bytes (s));
  dp_w_hdr (&w, CARRIER_NDA_STATE_MAGIC, CARRIER_NDA_STATE_VERSION,
            carrier_nda_state_bytes (s));
  dp_w_bytes (&w, &c, sizeof c);
}

int
carrier_nda_set_state (carrier_nda_state_t *s, const void *blob)
{
  int rc
      = dp_state_validate (blob, carrier_nda_state_bytes (s),
                           CARRIER_NDA_STATE_MAGIC, CARRIER_NDA_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  carrier_nda_state_t c;
  dp_reader_t         r = dp_reader_init (blob, carrier_nda_state_bytes (s));
  r.off                 = sizeof (dp_state_hdr_t);
  dp_r_bytes (&r, &c, sizeof c);
  c.tlm     = s->tlm; /* keep the live attachments */
  c.agc.tlm = s->agc.tlm;
  *s        = c;
  return DP_OK;
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
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loop contains NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * NCO/arm hot state (measured ~20% slower detached on the symsync
   * loops even though the call never executed). This loop updates every
   * sample, so the attached loop flushes per sample — the per-probe
   * decim set at attach is the throttle. */
  if (!state->tlm.ctx)
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex d = carrier_nda_wipeoff (state, x[i]);
          double        pe, lk;
          if (carrier_nda_arm_step (state, d, &pe, &lk))
            {
              state->lock += CARRIER_NDA_LOCK_ALPHA * (lk - state->lock);
              (void)lockdet_step (&state->lockdet, state->lock);
              carrier_nda_steer (state, pe);
            }
          if (emitted < max_out)
            out[emitted++] = d;
        }
    }
  else
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex d = carrier_nda_wipeoff (state, x[i]);
          double        pe, lk;
          if (carrier_nda_arm_step (state, d, &pe, &lk))
            {
              state->lock += CARRIER_NDA_LOCK_ALPHA * (lk - state->lock);
              (void)lockdet_step (&state->lockdet, state->lock);
              carrier_nda_steer (state, pe);
            }
          if (emitted < max_out)
            out[emitted++] = d;
          carrier_nda_tlm_flush (state);
        }
    }
  return emitted;
}

double
carrier_nda_get_norm_freq (const carrier_nda_state_t *state)
{
  /* Tracked carrier = NCO centre (nco.norm_freq) + the loop's integrated
   * frequency correction (lf.integ, already in cycles/sample — the loop gains
   * carry the rad->cycle scale, see config_loop). */
  return state->nco.norm_freq + state->lf.integ;
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
  config_loop (state);
  /* Keep the arm AGC locked to a fixed fraction of the carrier loop bandwidth
   * so it stays 100x slower than the loop at the new bn (see header). */
  state->agc.loop_bw = CARRIER_NDA_AGC_BW_RATIO * val;
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

void
carrier_nda_configure_lock (carrier_nda_state_t *state, double up_thresh,
                            double down_thresh, uint32_t n_up, uint32_t n_down)
{
  lockdet_configure (&state->lockdet, up_thresh, down_thresh, n_up, n_down);
}

int
carrier_nda_get_locked (const carrier_nda_state_t *state)
{
  return state->lockdet.locked;
}
