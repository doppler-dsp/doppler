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

/* Configure the carrier PI loop. Both discriminators fire once per recovered
   symbol, so the update period is one symbol and bn_carrier is normalised to
   the symbol rate — the same convention the timing loop uses, which is what
   makes one setting mean the same thing at every input rate. The loop filter
   then outputs a phase command per symbol; freq_scale turns that into the
   cycles per LO sample the front end's control port wants (rad -> cycles, then
   spread over the lo_sps samples a symbol spans). */
void
mpsk_rx_config_carrier (mpsk_rx_loops_t *l)
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
  l->freq_scale = CARRIER_NDA_INV_2PI * upd / l->lo_sps;
}

void
mpsk_rx_loops_init (mpsk_rx_loops_t *l, int m, double sps, double lo_sps,
                    size_t m_out, double bn_carrier, double zeta,
                    double bn_timing, double bn_agc_ratio, int ted,
                    int acq_to_track, double lock_thresh, int differential,
                    int nda_tap)
{
  l->m      = m;
  l->sps    = sps;
  l->lo_sps = lo_sps;
  /* Overwritten by any front end that publishes a real bank rate; this
     default only keeps mpsk_rx_updates_per_symbol() non-zero. */
  l->mf_in_sps    = lo_sps;
  l->m_out        = m_out;
  l->bn_carrier   = bn_carrier;
  l->bn_agc_ratio = bn_agc_ratio;
  l->zeta         = zeta;
  l->nda_tap      = nda_tap;
  /* Only the strobe tap reads an output the timing loop had to nominate; the
     other two are timing-independent by construction. */
  l->tap_timed = (nda_tap == MPSK_RX_NDA_TAP_STROBE);

  l->acq_to_track = acq_to_track ? 1 : 0;
  l->differential = differential ? 1 : 0;

  /* The NDA M-th-power loop's stable points are the 0-grid (z^m = +1), but
     the QPSK constellation sits on the pi/4-offset grid, so an unrotated
     strobe would land every symbol exactly on a decision boundary. */
  l->sym_rot = (float complex) (cos (mpsk_phi0 (m)) + sin (mpsk_phi0 (m)) * I);

  memset (&l->tlm, 0, sizeof l->tlm);

  ratesync_loop_init (&l->timing, sps, m_out, bn_timing, zeta, ted);

  /* Two-way handover rule on the carrier lock EMA: declare fast, drop
     reluctantly — level + time hysteresis so metric wobble at the threshold
     cannot chatter the discriminator choice. */
  lockdet_init (&l->handover, lock_thresh, MPSK_RX_HANDOVER_DOWN * lock_thresh,
                MPSK_RX_HANDOVER_N_UP, MPSK_RX_HANDOVER_N_DOWN);
  lockdet_init (&l->car_lock, lock_thresh, MPSK_RX_HANDOVER_DOWN * lock_thresh,
                MPSK_RX_HANDOVER_N_UP, MPSK_RX_HANDOVER_N_DOWN);

  mpsk_rx_config_carrier (l);
  mpsk_rx_loops_reset (l);
}

void
mpsk_rx_loops_reset (mpsk_rx_loops_t *l)
{
  ratesync_loop_reset (&l->timing);
  loop_filter_reset (&l->car_lf);
  l->freq_ctrl     = 0.0;
  l->car_error     = 0.0;
  l->lock          = 0.0;
  l->tracking      = 0;
  l->sym_count     = 0;
  l->lock_time     = -1;
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
  /* Receiver convention: the front end holds the conjugate. */
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_nco, -l->freq_ctrl);
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
  /* The command that actually drives the LO -- integ + kp*e, not the
     integrator alone. That sum is the frequency the receiver is APPLYING and
     is what a consumer watching a Doppler profile wants; `car.freq` is the
     integrator, i.e. the frequency MEMORY that survives a handover, and on a
     ramp the two differ by exactly the proportional term. Publishing only the
     integrator made a correctly-tracking loop look like it was lagging. */
  (void)snprintf (name, sizeof (name), "%s.car.nco", p);
  int id_nco = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_lock < 0 || id_tracking < 0 || id_e < 0 || id_freq < 0 || id_nco < 0
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
  l->tlm.id_nco      = id_nco;
  l->tlm.id_locked   = id_locked;
  l->tlm.ctx         = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* ── Serializable state — the loops ────────────────────────────────────────
 *
 * Scalars, then two self-validating children (timing loop, carrier loop
 * filter). `freq_scale` and `nda_tap` are pure config — restored by the
 * owner's create() and never packed.
 *
 * There used to be a third child, the Costas arm's boxcar, packed
 * unconditionally even though only one tap filled it. gh-768 removed the arm,
 * so the choice it forced (a fixed layout carrying an unused child, versus a
 * layout that changed shape with the tap) no longer exists. */

/* freq_ctrl, car_error, lock */
#define DP_MRX_DOUBLES 3
/* sym_count, tracking|have_prev_idx, prev_idx, lock_time */
#define DP_MRX_U64S 4

size_t
mpsk_rx_loops_state_bytes (const mpsk_rx_loops_t *l)
{
  return sizeof (dp_state_hdr_t) + DP_MRX_DOUBLES * sizeof (double)
         + DP_MRX_U64S * sizeof (uint64_t)
         + 4 * sizeof (uint32_t) /* handover + car_lock cnt/locked */
         + ratesync_loop_state_bytes (&l->timing)
         + loop_filter_state_bytes (&l->car_lf);
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
  dp_w_u64 (
      &w, (uint64_t)((l->tracking ? 1u : 0u) | (l->have_prev_idx ? 2u : 0u)));
  dp_w_u64 (&w, (uint64_t)l->prev_idx);
  /* -1 (never locked) round-trips as a two's-complement u64. */
  dp_w_u64 (&w, (uint64_t)l->lock_time);
  dp_w_u32 (&w, l->handover.cnt);
  dp_w_u32 (&w, (uint32_t)l->handover.locked);
  dp_w_u32 (&w, l->car_lock.cnt);
  dp_w_u32 (&w, (uint32_t)l->car_lock.locked);

  char *p = (char *)blob + w.off;
  ratesync_loop_get_state (&l->timing, p);
  p += ratesync_loop_state_bytes (&l->timing);
  loop_filter_get_state (&l->car_lf, p);
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
  l->lock_time     = (int64_t)dp_r_u64 (&r);
  l->tracking      = (flags & 1u) ? 1 : 0;
  l->have_prev_idx = (flags & 2u) ? 1 : 0;

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
  return DP_OK;
}

/* ==================================================================
 * MpskReceiver — the complex-input front end plus those loops
 * ================================================================== */

mpsk_receiver_state_t *
mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                      double rrc_beta, int rrc_span, double bn_carrier,
                      double zeta, double bn_timing, int acq_to_track,
                      double lock_thresh, double init_norm_freq,
                      int differential, size_t num_phases, int nda_tap,
                      int agc, double bn_agc_ratio)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (pulse != MPSK_RX_PULSE_IANDD && pulse != MPSK_RX_PULSE_RRC)
    return NULL;

  /* Derive what is not a design axis (doppler#644). Zero asks for the
     object's own answer; every validator below previously REJECTED zero, so
     no working call site can be relying on it. The derivation runs BEFORE the
     validation so a derived value is checked by the same guards a supplied
     one is — a rule that produced an invalid answer must still be caught. */
  if (m_out == 0u)
    m_out = mpsk_rx_derive_m_out (sps, 0); /* sps >= m_out */
  if (zeta == 0.0)
    zeta = MPSK_RX_ZETA_DEFAULT;
  if (num_phases == 0u)
    num_phases = MPSK_RX_NUM_PHASES_DEFAULT;
  if (lock_thresh == 0.0)
    lock_thresh = MPSK_RX_LOCK_THRESH_DEFAULT;
  if (bn_agc_ratio == 0.0)
    bn_agc_ratio = MPSK_RX_AGC_RATIO_DEFAULT;
  /* Written as !(x >= y) so a NaN parameter is rejected, not accepted. */
  if (m_out < 2u || m_out > (size_t)RATESYNC_MAX_M || (m_out & 1u) != 0u
      || !(sps >= (double)m_out) || !(rrc_beta >= 0.0) || !(rrc_beta <= 1.0)
      || rrc_span < 1 || !(bn_carrier >= 0.0) || !(bn_timing >= 0.0)
      || !(zeta > 0.0) || num_phases < 2u
      || (num_phases & (num_phases - 1u)) != 0u
      || nda_tap < MPSK_RX_NDA_TAP_STROBE
      || nda_tap > MPSK_RX_NDA_TAP_MF_IN
      /* An AGC at or above the bandwidth of a loop it feeds corrects the
         excursions that loop is producing; the two then integrate against
         each other. The invariant is structural rather than advisory. */
      || !(bn_agc_ratio > 0.0) || !(bn_agc_ratio < 1.0))
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
                      bn_agc_ratio, RATESYNC_TED_GARDNER, acq_to_track,
                      lock_thresh, differential, nda_tap);
  ratesync_loop_bind_cascade (&rx->l.timing, rx->fe->rc);
  /* The pre-terminal tap's update rate is the cascade's own bank rate. It is
     a planner outcome, so it is READ from the cascade that planned it rather
     than re-derived here — re-deriving would be a second copy of the plan,
     free to drift from the one the filters were actually built on.

     It arrives too LATE for mpsk_rx_loops_init(), which has already run
     config_carrier() against the `lo_sps` placeholder — so the carrier filter
     must be re-sized now that the real rate is known. Skipping this is not a
     tuning nicety: MF_IN would keep gains designed for `lo_sps` updates per
     symbol while actually updating `bank_sps` times, i.e. ki too small by
     (lo_sps/bank_sps)^2 — 1.7e7 at Fs/Rs = 10000, an integrator that never
     moves. The loop then reads a perfect 0 Hz error at 0 Hz offset and
     acquires nothing at any other, which is exactly as wrong as it sounds.
     native/validation/rx_nda_tap.c is the gate. `integ` survives
     loop_filter_init() by contract, and every other tap re-derives the same
     gains it already had. */
  rx->l.mf_in_sps = ddc_get_bank_sps (rx->fe);
  mpsk_rx_config_carrier (&rx->l);

  /* The front end levels itself so the TED's construct-time slope means what
     it says. A zero loop bandwidth leaves nothing to be slower than, so the
     derived AGC bandwidth is zero and enable_agc declines -- the receiver is
     then simply un-levelled, which is the honest reading of bn = 0. */
  if (agc)
    (void)RateConverter_enable_agc (
        rx->fe->rc, mpsk_rx_agc_bn (bn_carrier, bn_timing, bn_agc_ratio),
        MPSK_RX_AGC_ALPHA);
  return rx;
}

double
mpsk_receiver_get_agc_gain_db (const mpsk_receiver_state_t *state)
{
  return RateConverter_agc_gain_db (state->fe->rc);
}

/* The continuous flavor. A pure delegate -- every argument it does not take
   is a literal here and nothing else differs, so there is no second
   construction path to keep in step with mpsk_receiver_create(). The zeros
   are requests, not omissions: each one asks create() for the derived answer
   (see its @note), which is why this constructor can be short without being
   opinionated about values it has no business choosing. */
mpsk_receiver_state_t *
mpsk_receiver_create_continuous (int m, double sps, int pulse, double rrc_beta,
                                 int rrc_span, double bn_carrier,
                                 double bn_timing, double init_norm_freq,
                                 int differential)
{
  return mpsk_receiver_create (
      m, sps, 0u, /* m_out        -> derived      */
      pulse, rrc_beta, rrc_span, bn_carrier, 0.0, /* zeta         -> derived */
      bn_timing, 0,                     /* acq_to_track -- NO handover */
      0.0,                              /* lock_thresh  -> derived      */
      init_norm_freq, differential, 0u, /* num_phases   -> derived      */
      MPSK_RX_NDA_TAP_MF_IN,            /* no timing dep.    */
      1,                                /* agc -- load-bearing, not opt */
      0.0);                             /* bn_agc_ratio -> derived      */
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
  return 0; /* one bit per symbol, and symbols <= inputs */
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

int64_t
mpsk_receiver_get_lock_time (const mpsk_receiver_state_t *state)
{
  return state->l.lock_time;
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
  int rc = mpsk_rx_set_telemetry (&state->l, tlm, prefix, decim);
  if (rc != DP_OK)
    return rc;
  /* The front end's AGC under "<prefix>.agc". It is the third loop in this
     receiver and was the only one emitting nothing, which made its settling
     the one thing a caller had to infer rather than read -- see
     mpsk_rx_agc_bn() for why that loop is the slowest of the three and so
     the one that sets how long the receiver takes to become usable. */
  char name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.agc", prefix ? prefix : "rx");
  int rc_agc = ddc_set_telemetry (state->fe, tlm, name, decim);
  if (rc_agc != DP_OK) /* fails whole: undo the loops we just attached */
    {
      (void)mpsk_rx_set_telemetry (&state->l, NULL, prefix, decim);
      return rc_agc;
    }
  return DP_OK;
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

/* The derived-parameter readbacks. design/mpsk.md §8.1: everything derived is
   REPORTED, on the same argument as RateConverter.stages — a caller who can
   read back what was chosen can check it. Without these, `0` would be an
   instruction whose result nobody can see. */
double
mpsk_receiver_get_zeta (const mpsk_receiver_state_t *state)
{
  return state->l.zeta;
}

double
mpsk_receiver_get_bn_agc_ratio (const mpsk_receiver_state_t *state)
{
  return state->l.bn_agc_ratio;
}

double
mpsk_receiver_get_lock_thresh (const mpsk_receiver_state_t *state)
{
  return state->l.handover.up_thresh;
}

size_t
mpsk_receiver_get_num_phases (const mpsk_receiver_state_t *state)
{
  return state->fe->rc->num_phases;
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
