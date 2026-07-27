/*
 * mpsk_receiver_core.c — the M-PSK receiver, and the loops both receiver
 * types share.
 *
 * Two things live here. The first half is mpsk_rx_loops_t: the carrier loop,
 * the handover and the demapper, plus lifecycle for the timing loop embedded
 * from ratesync. It has no front end of its own, which is exactly why the
 * complex- and real-input receivers can share it verbatim — the real-input
 * twin links this core and supplies only its own front end. The second half is
 * MpskReceiver itself: a matched DDC, those loops, and the block API.
 *
 * The per-output work is the force-inlined mpsk_rx_take_output() in
 * mpsk_rx_loops.h, and the design arguments (why the carrier loop runs at two
 * rates, why the symbol rather than the filter input is rotated) live there.
 */
#include "mpsk_receiver/mpsk_receiver_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * The loops, front-end agnostic
 * ================================================================== */

/* Arm AGC in front of the M-th-power discriminator. The discriminator's gain
 * goes as |z|^m, so the loop gain is only amplitude-invariant if the sample
 * feeding it sits at unit average power. Same constants and the same reasoning
 * as carrier_nda's own arm (carrier_nda_core.h): the AGC must stay ~100x
 * slower than the carrier loop so it tracks the overall signal level and never
 * the carrier dynamics or the within-symbol pulse envelope, and the 10 dB
 * square clip bounds the peak constructive-ISI samples that would otherwise
 * dominate the |z|^m weighting. It runs once per terminal output, which is
 * this loop's update rate, so no decimation is wanted. */
static void
seed_agc (mpsk_rx_loops_t *l)
{
  l->car_agc.ref_db             = CARRIER_NDA_AGC_REF_DB;
  l->car_agc.loop_bw            = MPSK_RX_AGC_BW;
  l->car_agc.alpha              = CARRIER_NDA_AGC_ALPHA;
  l->car_agc.decim              = AGC_DECIM_DEFAULT;
  l->car_agc.clip_db            = CARRIER_NDA_AGC_CLIP_DB;
  l->car_agc.gain_db            = 0.0;
  l->car_agc.p_avg              = 1.0; /* 10^(ref_db/10) for ref_db = 0 */
  l->car_agc.g_last             = 1.0;
  l->car_agc.gain_update_period = AGC_DECIM_DEFAULT;
  l->car_agc.gain_phase         = 0;
  l->car_agc.clip_lin = (float)agc_exp10_ (l->car_agc.clip_db * 0.05);
  l->agc_seeded       = 0; /* counts the strobes averaged to set the level;
                              see mpsk_rx_disc's seeding block */
}

/* Configure the carrier PI loop. Both discriminators fire once per recovered
   symbol, so the update period is one symbol and bn_carrier is normalised to
   the symbol rate — the same convention the timing loop uses, which is what
   makes one setting mean the same thing at every input rate. The loop filter
   then outputs a phase command per symbol; freq_scale turns that into the
   cycles per LO sample the front end's control port wants (rad -> cycles, then
   spread over the lo_sps samples a symbol spans). */
static void
config_carrier (mpsk_rx_loops_t *l)
{
  /* bn_carrier keeps its meaning — normalised to the SYMBOL rate — whatever
     tap the caller picked, so one setting means the same loop at every tap.
     What the tap changes is how often that loop is updated, which is the
     filter's `t` (its update period, here measured in symbols). A tap that
     updates m_out or lo_sps times per symbol therefore does NOT silently
     widen the loop; it widens the range of frequency error the discriminator
     can still SEE, and improves the stability margin at any given bn, which
     is what lets a caller then raise bn_carrier on purpose. */
  double upd = mpsk_rx_updates_per_symbol (l);
  loop_filter_init (&l->car_lf, l->bn_carrier, l->zeta, 1.0 / upd);
  l->freq_scale = CARRIER_NDA_INV_2PI / l->lo_sps;
}

void
mpsk_rx_loops_init (mpsk_rx_loops_t *l, int m, double sps, double lo_sps,
                    size_t m_out, double bn_carrier, double zeta,
                    double bn_timing, int ted, int acq_to_track,
                    double lock_thresh, size_t warmup_syms, int differential,
                    int nda_tap)
{
  l->m          = m;
  l->sps        = sps;
  l->lo_sps     = lo_sps;
  l->m_out      = m_out;
  l->bn_carrier = bn_carrier;
  l->zeta       = zeta;
  l->nda_tap    = nda_tap;
  /* Only the strobe tap reads an output the timing loop had to nominate; the
     other two are timing-independent by construction. */
  l->tap_timed = (nda_tap == MPSK_RX_NDA_TAP_STROBE);

  /* Free-running arm for the LO tap: a half-symbol boxcar at the LO's rate,
     clamped to what the ring can hold. Unit gain — the AGC downstream is what
     sets the discriminator's operating level. */
  {
    double win = lo_sps / (double)MPSK_RX_ARM_DIV;
    size_t len = (size_t)(win < 1.0 ? 1.0 : win);
    if (len > (size_t)BOXCAR_MAX_LEN)
      len = (size_t)BOXCAR_MAX_LEN;
    boxcar_init (&l->arm, len, 1.0);
  }

  l->acq_to_track = acq_to_track ? 1 : 0;
  l->warmup_syms  = warmup_syms;
  l->differential = differential ? 1 : 0;

  /* The NDA M-th-power loop's stable points are the 0-grid (z^m = +1), but
     the QPSK constellation sits on the pi/4-offset grid, so an unrotated
     strobe would land every symbol exactly on a decision boundary. */
  l->sym_rot = (float complex) (cos (mpsk_phi0 (m)) + sin (mpsk_phi0 (m)) * I);

  memset (&l->tlm, 0, sizeof l->tlm);
  memset (&l->car_agc.tlm, 0, sizeof l->car_agc.tlm);

  ratesync_loop_init (&l->timing, sps, m_out, bn_timing, zeta, ted);

  /* Two-way handover rule on the carrier lock EMA: declare fast, drop
     reluctantly — level + time hysteresis so metric wobble at the threshold
     cannot chatter the discriminator choice. */
  lockdet_init (&l->handover, lock_thresh, MPSK_RX_HANDOVER_DOWN * lock_thresh,
                MPSK_RX_HANDOVER_N_UP, MPSK_RX_HANDOVER_N_DOWN);
  lockdet_init (&l->car_lock, lock_thresh, MPSK_RX_HANDOVER_DOWN * lock_thresh,
                MPSK_RX_HANDOVER_N_UP, MPSK_RX_HANDOVER_N_DOWN);

  config_carrier (l);
  mpsk_rx_loops_reset (l);
}

void
mpsk_rx_loops_reset (mpsk_rx_loops_t *l)
{
  ratesync_loop_reset (&l->timing);
  loop_filter_reset (&l->car_lf);
  seed_agc (l);
  l->freq_ctrl     = 0.0;
  l->car_error     = 0.0;
  l->lock          = 0.0;
  l->tracking      = 0;
  l->sym_count     = 0;
  l->have_prev_idx = 0;
  l->prev_idx      = 0;
  lockdet_reset (&l->handover);
  lockdet_reset (&l->car_lock);
}

void
mpsk_rx_configure_lock (mpsk_rx_loops_t *l, double up_thresh,
                        double down_thresh, uint32_t n_up, uint32_t n_down)
{
  lockdet_configure (&l->handover, up_thresh, down_thresh, n_up, n_down);
}

double
mpsk_rx_freq_est (const mpsk_rx_loops_t *l)
{
  /* The INTEGRATOR is the frequency memory (loop_filter_core.h: "kp*e is the
     instantaneous phase nudge"), so the estimate excludes the proportional
     term. Reported in the receiver's convention — positive means the carrier
     sits above the centre the LO is tuned to. */
  return l->car_lf.integ * l->freq_scale;
}

void
mpsk_rx_set_freq_est (mpsk_rx_loops_t *l, double val)
{
  l->car_lf.integ = l->freq_scale > 0.0 ? val / l->freq_scale : 0.0;
  l->freq_ctrl    = -val;
}

int
mpsk_rx_symbol_to_bits (mpsk_rx_loops_t *l, float complex y, uint8_t *bits)
{
  float complex ahat;
  unsigned      label = mpsk_slice (y, l->m, &ahat);
  unsigned      out_label;
  if (l->differential)
    {
      unsigned idx     = mpsk_gray_decode (label & (unsigned)(l->m - 1));
      unsigned prev    = l->have_prev_idx ? l->prev_idx : 0u;
      unsigned diff    = (idx + (unsigned)l->m - prev) % (unsigned)l->m;
      l->prev_idx      = idx;
      l->have_prev_idx = 1;
      out_label        = mpsk_gray_encode (diff);
    }
  else
    out_label = label;
  int bps = mpsk_bps (l->m);
  for (int b = 0; b < bps; b++)
    bits[b] = (uint8_t)((out_label >> b) & 1u);
  return bps;
}

void
mpsk_rx_tlm_flush (const mpsk_rx_loops_t *l)
{
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_lock, l->lock);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_tracking, (double)l->tracking);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_e, l->car_error);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_freq, mpsk_rx_freq_est (l));
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_locked, (double)l->car_lock.locked);
  ratesync_loop_tlm_flush (&l->timing);
}

int
mpsk_rx_set_telemetry (mpsk_rx_loops_t *l, dp_tlm_t *tlm, const char *prefix,
                       uint32_t decim)
{
  if (!tlm) /* detach the receiver AND the timing loop */
    {
      l->tlm.ctx = NULL;
      (void)ratesync_loop_set_telemetry (&l->timing, NULL, prefix, decim);
      return DP_OK;
    }
  const char *p = prefix ? prefix : "rx";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.tracking", p);
  int id_tracking = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_lock < 0 || id_tracking < 0 || id_e < 0 || id_freq < 0
      || id_locked < 0)
    return DP_ERR_INVALID;
  /* Forward to the timing loop under "<prefix>.sync"; if it fails the whole
     attach fails, so nothing is left half-armed. */
  (void)snprintf (name, sizeof (name), "%s.sync", p);
  int rc = ratesync_loop_set_telemetry (&l->timing, tlm, name, decim);
  if (rc != DP_OK)
    return rc;
  l->tlm.id_lock     = id_lock;
  l->tlm.id_tracking = id_tracking;
  l->tlm.id_e        = id_e;
  l->tlm.id_freq     = id_freq;
  l->tlm.id_locked   = id_locked;
  l->tlm.ctx         = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* ── Serializable state — the loops ────────────────────────────────────────
 *
 * Scalars, then four self-validating children (timing loop, carrier loop
 * filter, arm AGC, arm boxcar). `freq_scale` and `nda_tap` are pure config —
 * restored by the owner's create() and never packed. The arm rides along
 * unconditionally even though only MPSK_RX_NDA_TAP_LO_ARM fills it: a blob
 * layout that changed shape with the tap would be one more thing to get
 * wrong, and boxcar_state_bytes is a few hundred bytes once. */

/* freq_ctrl, car_error, lock */
#define _MRX_DOUBLES 3
/* sym_count, tracking|have_prev_idx, prev_idx */
#define _MRX_U64S 3

size_t
mpsk_rx_loops_state_bytes (const mpsk_rx_loops_t *l)
{
  return sizeof (dp_state_hdr_t) + _MRX_DOUBLES * sizeof (double)
         + _MRX_U64S * sizeof (uint64_t)
         + 4 * sizeof (uint32_t) /* handover + car_lock cnt/locked */
         + ratesync_loop_state_bytes (&l->timing)
         + loop_filter_state_bytes (&l->car_lf) + agc_state_bytes (&l->car_agc)
         + boxcar_state_bytes (&l->arm);
}

void
mpsk_rx_loops_get_state (const mpsk_rx_loops_t *l, void *blob)
{
  const size_t total = mpsk_rx_loops_state_bytes (l);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, MPSK_RX_LOOPS_STATE_MAGIC, MPSK_RX_LOOPS_STATE_VERSION, total);
  dp_w_f64 (&w, l->freq_ctrl);
  dp_w_f64 (&w, l->car_error);
  dp_w_f64 (&w, l->lock);
  dp_w_u64 (&w, (uint64_t)l->sym_count);
  /* agc_seeded is a COUNTER (0..MPSK_RX_AGC_SEED_SAMPS), not a flag: it rides
     in bits 8..15 so the blob size stays put. See the v3 note on
     MPSK_RX_LOOPS_STATE_VERSION. */
  dp_w_u64 (&w,
            (uint64_t)((l->tracking ? 1u : 0u) | (l->have_prev_idx ? 2u : 0u))
                | (((uint64_t)l->agc_seeded & 0xFFu) << 8));
  dp_w_u64 (&w, (uint64_t)l->prev_idx);
  dp_w_u32 (&w, l->handover.cnt);
  dp_w_u32 (&w, (uint32_t)l->handover.locked);
  dp_w_u32 (&w, l->car_lock.cnt);
  dp_w_u32 (&w, (uint32_t)l->car_lock.locked);

  char *p = (char *)blob + w.off;
  ratesync_loop_get_state (&l->timing, p);
  p += ratesync_loop_state_bytes (&l->timing);
  loop_filter_get_state (&l->car_lf, p);
  p += loop_filter_state_bytes (&l->car_lf);
  agc_get_state (&l->car_agc, p);
  p += agc_state_bytes (&l->car_agc);
  boxcar_get_state (&l->arm, p);
}

int
mpsk_rx_loops_set_state (mpsk_rx_loops_t *l, const void *blob)
{
  const size_t total = mpsk_rx_loops_state_bytes (l);
  int          rc = dp_state_validate (blob, total, MPSK_RX_LOOPS_STATE_MAGIC,
                                       MPSK_RX_LOOPS_STATE_VERSION);
  if (rc != DP_OK)
    return rc;

  dp_reader_t r    = dp_reader_init (blob, total);
  r.off            = sizeof (dp_state_hdr_t);
  l->freq_ctrl     = dp_r_f64 (&r);
  l->car_error     = dp_r_f64 (&r);
  l->lock          = dp_r_f64 (&r);
  l->sym_count     = (size_t)dp_r_u64 (&r);
  uint64_t flags   = dp_r_u64 (&r);
  l->prev_idx      = (unsigned)dp_r_u64 (&r);
  l->tracking      = (flags & 1u) ? 1 : 0;
  l->have_prev_idx = (flags & 2u) ? 1 : 0;
  l->agc_seeded    = (int)((flags >> 8) & 0xFFu);

  l->handover.cnt    = dp_r_u32 (&r);
  l->handover.locked = (int)dp_r_u32 (&r);
  l->car_lock.cnt    = dp_r_u32 (&r);
  l->car_lock.locked = (int)dp_r_u32 (&r);

  const char *p = (const char *)blob + r.off;
  rc            = ratesync_loop_set_state (&l->timing, p);
  if (rc != DP_OK)
    return rc;
  p += ratesync_loop_state_bytes (&l->timing);
  rc = loop_filter_set_state (&l->car_lf, p);
  if (rc != DP_OK)
    return rc;
  p += loop_filter_state_bytes (&l->car_lf);
  rc = agc_set_state (&l->car_agc, p);
  if (rc != DP_OK)
    return rc;
  p += agc_state_bytes (&l->car_agc);
  return boxcar_set_state (&l->arm, p);
}

/* ==================================================================
 * MpskReceiver — the complex-input front end plus those loops
 * ================================================================== */

mpsk_receiver_state_t *
mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                      double rrc_beta, int rrc_span, double bn_carrier,
                      double zeta, double bn_timing, int acq_to_track,
                      double lock_thresh, double init_norm_freq,
                      size_t warmup_syms, int differential, size_t num_phases,
                      int nda_tap)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (pulse != MPSK_RX_PULSE_IANDD && pulse != MPSK_RX_PULSE_RRC)
    return NULL;
  /* Written as !(x >= y) so a NaN parameter is rejected, not accepted. */
  if (m_out < 2u || m_out > (size_t)RATESYNC_MAX_M || (m_out & 1u) != 0u
      || !(sps >= (double)m_out) || !(rrc_beta >= 0.0) || !(rrc_beta <= 1.0)
      || rrc_span < 1 || !(bn_carrier >= 0.0) || !(bn_timing >= 0.0)
      || !(zeta > 0.0) || num_phases < 2u
      || (num_phases & (num_phases - 1u)) != 0u
      || nda_tap < MPSK_RX_NDA_TAP_STROBE || nda_tap > MPSK_RX_NDA_TAP_LO_ARM)
    return NULL;

  mpsk_receiver_state_t *rx = calloc (1, sizeof (*rx));
  if (!rx)
    return NULL;

  /* The DDC's LO is the conjugate convention of the old carrier_nda NCO: a
     carrier at +f is brought to DC by tuning the LO to -f. init_norm_freq
     keeps its caller-facing meaning (the carrier offset to remove), so the
     sign flips exactly here and nowhere else. */
  rx->fe = ddc_create_matched (-init_norm_freq, (double)m_out / sps, pulse,
                               rrc_beta, (size_t)rrc_span, (double)m_out,
                               num_phases);
  if (!rx->fe)
    {
      free (rx);
      return NULL;
    }
  rx->centre_freq = init_norm_freq;

  /* A complex front end mixes at the input rate, so the LO sees `sps` samples
     per symbol. (The real-input twin's halfband decimates first, which is why
     lo_sps is a parameter rather than an assumption.) */
  mpsk_rx_loops_init (&rx->l, m, sps, sps, m_out, bn_carrier, zeta, bn_timing,
                      RATESYNC_TED_GARDNER, acq_to_track, lock_thresh,
                      warmup_syms, differential, nda_tap);
  ratesync_loop_bind_cascade (&rx->l.timing, rx->fe->rc);
  return rx;
}

void
mpsk_receiver_destroy (mpsk_receiver_state_t *state)
{
  if (!state)
    return;
  ddc_destroy (state->fe);
  free (state);
}

void
mpsk_receiver_reset (mpsk_receiver_state_t *state)
{
  ddc_reset (state->fe);
  mpsk_rx_loops_reset (&state->l);
}

size_t
mpsk_receiver_steps_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0; /* sps >= m_out >= 2, so symbols <= inputs */
}

size_t
mpsk_receiver_steps (mpsk_receiver_state_t *state, const float complex *x,
                     size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  /* Telemetry hoisted to loop entry (attach is setup-time only): the detached
     loop carries no call site, so the compiler keeps the hot loop state in
     registers — an extern call per iteration forces everything to memory
     (measured ~20% slower detached on the previous engine). */
  if (!state->l.tlm.ctx)
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex y;
          if (mpsk_receiver_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER)
              && emitted < max_out)
            out[emitted++] = y;
        }
    }
  else
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex y;
          if (mpsk_receiver_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER))
            {
              if (emitted < max_out)
                out[emitted++] = y;
              mpsk_rx_tlm_flush (&state->l);
            }
        }
    }
  return emitted;
}

size_t
mpsk_receiver_bits_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0;
}

size_t
mpsk_receiver_bits (mpsk_receiver_state_t *state, const float complex *x,
                    size_t x_len, uint8_t *out, size_t max_out)
{
  size_t emitted = 0;
  /* Guarded in-loop flush (not the steps() split): this loop already makes a
     per-symbol call, so there is no pristine register-resident fast path. */
  for (size_t i = 0; i < x_len; i++)
    {
      float complex y;
      if (!mpsk_receiver_step_ted (state, x[i], &y, RATESYNC_TED_GARDNER))
        continue;
      if (state->l.tlm.ctx)
        mpsk_rx_tlm_flush (&state->l);
      uint8_t bits[3];
      int     nb = mpsk_rx_symbol_to_bits (&state->l, y, bits);
      for (int b = 0; b < nb && emitted < max_out; b++)
        out[emitted++] = bits[b];
    }
  return emitted;
}

double
mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state)
{
  return state->centre_freq + mpsk_rx_freq_est (&state->l);
}

double
mpsk_receiver_get_nco_freq (const mpsk_receiver_state_t *state)
{
  /* The instantaneous command includes the proportional nudge, and is held in
     the front end's (conjugate) convention — report the receiver's. */
  return state->centre_freq - state->l.freq_ctrl;
}

void
mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val)
{
  state->centre_freq = val;
  ddc_set_norm_freq (state->fe, -val);
  mpsk_rx_set_freq_est (&state->l, 0.0);
}

double
mpsk_receiver_get_lock (const mpsk_receiver_state_t *state)
{
  return state->l.lock;
}

int
mpsk_receiver_get_locked (const mpsk_receiver_state_t *state)
{
  return state->l.car_lock.locked;
}

double
mpsk_receiver_get_last_error (const mpsk_receiver_state_t *state)
{
  return state->l.car_error;
}

void
mpsk_receiver_configure_lock (mpsk_receiver_state_t *state, double up_thresh,
                              double down_thresh, uint32_t n_up,
                              uint32_t n_down)
{
  mpsk_rx_configure_lock (&state->l, up_thresh, down_thresh, n_up, n_down);
}

int
mpsk_receiver_set_telemetry (mpsk_receiver_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim)
{
  return mpsk_rx_set_telemetry (&state->l, tlm, prefix, decim);
}

double
mpsk_receiver_get_timing_rate (const mpsk_receiver_state_t *state)
{
  return state->l.timing.rate_est;
}

int
mpsk_receiver_get_tracking (const mpsk_receiver_state_t *state)
{
  return state->l.tracking;
}

int
mpsk_receiver_get_m (const mpsk_receiver_state_t *state)
{
  return state->l.m;
}

double
mpsk_receiver_get_sps (const mpsk_receiver_state_t *state)
{
  return state->l.sps;
}

size_t
mpsk_receiver_get_m_out (const mpsk_receiver_state_t *state)
{
  return state->l.m_out;
}

int
mpsk_receiver_get_clipped (const mpsk_receiver_state_t *state)
{
  return ddc_get_clipped (state->fe) ? 1 : 0;
}

/* ── Serializable state — two children, no scalars of our own ───────────────
 */

size_t
mpsk_receiver_state_bytes (const mpsk_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + ddc_state_bytes (s->fe)
         + mpsk_rx_loops_state_bytes (&s->l);
}

void
mpsk_receiver_get_state (const mpsk_receiver_state_t *s, void *blob)
{
  const size_t total = mpsk_receiver_state_bytes (s);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, MPSK_RECEIVER_STATE_MAGIC, MPSK_RECEIVER_STATE_VERSION, total);
  char *p = (char *)blob + w.off;
  ddc_get_state (s->fe, p);
  p += ddc_state_bytes (s->fe);
  mpsk_rx_loops_get_state (&s->l, p);
}

int
mpsk_receiver_set_state (mpsk_receiver_state_t *s, const void *blob)
{
  const size_t total = mpsk_receiver_state_bytes (s);
  int          rc = dp_state_validate (blob, total, MPSK_RECEIVER_STATE_MAGIC,
                                       MPSK_RECEIVER_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  const char *p = (const char *)blob + sizeof (dp_state_hdr_t);
  rc            = ddc_set_state (s->fe, p);
  if (rc != DP_OK)
    return rc;
  p += ddc_state_bytes (s->fe);
  return mpsk_rx_loops_set_state (&s->l, p);
}
