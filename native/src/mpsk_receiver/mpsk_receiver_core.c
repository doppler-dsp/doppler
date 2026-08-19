/*
 * mpsk_receiver_core.c — the M-PSK receiver: one object, two front ends.
 *
 * Two things live here. The first half is mpsk_rx_loops_t: the carrier loop,
 * the lock indicator and the demapper, plus lifecycle for the timing loop
 * embedded
 * from ratesync. It has no front end of its own, which is exactly why the
 * complex- and real-input faces share it verbatim. The second half is
 * MpskReceiver itself: a matched DDC *or* a matched DDCR, those loops, and the
 * block API.
 *
 * The `real` tag is read on cold paths ONLY — create, destroy, reset,
 * telemetry, the frequency accessors and the state triplet. The hot path has
 * two entry points, so the front end is a compile-time fact inside the sample
 * loop (docs/design/mpsk.md §12).
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
                    double lock_thresh, int differential)
{
  l->m      = m;
  l->sps    = sps;
  l->lo_sps = lo_sps;
  /* Overwritten by any front end that publishes a real bank rate; this
     default only keeps mpsk_rx_updates_per_symbol() non-zero. */
  l->m_out        = m_out;
  l->bn_carrier   = bn_carrier;
  l->bn_agc_ratio = bn_agc_ratio;
  l->zeta         = zeta;
  /* Only the strobe tap reads an output the timing loop had to nominate; the
     other two are timing-independent by construction. */

  l->differential = differential ? 1 : 0;

  /* The NDA M-th-power loop's stable points are the 0-grid (z^m = +1), but
     the QPSK constellation sits on the pi/4-offset grid, so an unrotated
     strobe would land every symbol exactly on a decision boundary. */
  l->sym_rot = (float complex) (cos (mpsk_phi0 (m)) + sin (mpsk_phi0 (m)) * I);

  memset (&l->tlm, 0, sizeof l->tlm);

  ratesync_loop_init (&l->timing, sps, m_out, bn_timing, zeta, ted);

  /* Carrier lock indicator on the lock EMA: declare fast, drop reluctantly
     — level + time hysteresis so metric wobble at the threshold cannot
     chatter the reading a caller sizes its measurement window from. */
  lockdet_init (&l->car_lock, lock_thresh, MPSK_RX_LOCK_DOWN * lock_thresh,
                MPSK_RX_LOCK_N_UP, MPSK_RX_LOCK_N_DOWN);

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
  l->sym_count     = 0;
  l->lock_time     = -1;
  l->have_prev_idx = 0;
  l->prev_idx      = 0;
  lockdet_reset (&l->car_lock);
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
mpsk_rx_tlm_flush (const mpsk_rx_loops_t *l, float complex y)
{
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_lock, l->lock);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_e, l->car_error);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_freq, mpsk_rx_freq_est (l));
  /* Receiver convention: the front end holds the conjugate. */
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_nco, -l->freq_ctrl);
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_locked, (double)l->car_lock.locked);
  /* The symbol this flush is ABOUT. Every probe here fires once per
     recovered symbol, so the pair lands on the same sample index as the loop
     state that produced it and a reader rebuilds the constellation by zipping
     them -- no separate stream, and no way for the two to disagree. */
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_sym_i, (double)crealf (y));
  dp_tlm_emit (l->tlm.ctx, l->tlm.id_sym_q, (double)cimagf (y));
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
  (void)snprintf (name, sizeof (name), "%s.car.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  /* The command that actually drives the LO -- integ + kp*e, not the
     integrator alone. That sum is the frequency the receiver is APPLYING and
     is what a consumer watching a Doppler profile wants; `car.freq` is the
     integrator, i.e. the frequency MEMORY the loop carries, and on a
     ramp the two differ by exactly the proportional term. Publishing only the
     integrator made a correctly-tracking loop look like it was lagging. */
  (void)snprintf (name, sizeof (name), "%s.car.nco", p);
  int id_nco = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.car.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.sym.i", p);
  int id_sym_i = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.sym.q", p);
  int id_sym_q = dp_tlm_probe (tlm, name, decim);
  if (id_lock < 0 || id_e < 0 || id_freq < 0 || id_nco < 0 || id_locked < 0
      || id_sym_i < 0 || id_sym_q < 0)
    return DP_ERR_INVALID;
  /* Forward to the timing loop under "<prefix>.sync"; if it fails the whole
     attach fails, so nothing is left half-armed. */
  (void)snprintf (name, sizeof (name), "%s.sync", p);
  int rc = ratesync_loop_set_telemetry (&l->timing, tlm, name, decim);
  if (rc != DP_OK)
    return rc;
  l->tlm.id_sym_i  = id_sym_i;
  l->tlm.id_sym_q  = id_sym_q;
  l->tlm.id_lock   = id_lock;
  l->tlm.id_e      = id_e;
  l->tlm.id_freq   = id_freq;
  l->tlm.id_nco    = id_nco;
  l->tlm.id_locked = id_locked;
  l->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

/* ── Serializable state — the loops ────────────────────────────────────────
 *
 * Scalars, then two self-validating children (timing loop, carrier loop
 * filter). `freq_scale` is pure config — restored by the
 * owner's create() and never packed.
 *
 * There used to be a third child, the Costas arm's boxcar, packed
 * unconditionally even though only one tap filled it. gh-768 removed the arm,
 * so the choice it forced (a fixed layout carrying an unused child, versus a
 * layout that changed shape with the tap) no longer exists. */

/* freq_ctrl, car_error, lock */
#define DP_MRX_DOUBLES 3
/* sym_count, have_prev_idx, prev_idx, lock_time */
#define DP_MRX_U64S 4

size_t
mpsk_rx_loops_state_bytes (const mpsk_rx_loops_t *l)
{
  return sizeof (dp_state_hdr_t) + DP_MRX_DOUBLES * sizeof (double)
         + DP_MRX_U64S * sizeof (uint64_t)
         + 2 * sizeof (uint32_t) /* car_lock cnt/locked */
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
  dp_w_u64 (&w, (uint64_t)(l->have_prev_idx ? 1u : 0u));
  dp_w_u64 (&w, (uint64_t)l->prev_idx);
  /* -1 (never locked) round-trips as a two's-complement u64. */
  dp_w_u64 (&w, (uint64_t)l->lock_time);
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
  l->have_prev_idx = (flags & 1u) ? 1 : 0;

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
 * MpskReceiver — one front end (complex or real) plus those loops
 * ================================================================== */

/* The cascade both front ends own, reached without knowing which one it is.
   Every accessor that only wants the RateConverter goes through here, so the
   tag is read in one place rather than copied into each of them. */
static RateConverter_state_t *
mpsk_rx_fe_rc (const mpsk_receiver_state_t *s)
{
  return s->real ? s->fe.r->rc : s->fe.c->rc;
}

/* One construction body. `real` decides three things and nothing else: which
   bound `sps` (and therefore a derived `m_out`) must clear, which front end is
   built and with what tuning law, and what the LO's own rate is. Every
   derivation, every validator and the whole loop setup is shared — which is
   the point of the collapse, because a rule that exists twice is a rule free
   to drift (docs/design/mpsk.md §12.1). */
static mpsk_receiver_state_t *
mpsk_rx_create_impl (int real, int m, double sps, size_t m_out, int pulse,
                     double rrc_beta, int rrc_span, double bn_carrier,
                     double zeta, double bn_timing, double lock_thresh,
                     double init_norm_freq, int differential,
                     size_t num_phases, int agc, double bn_agc_ratio)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (pulse != MPSK_RX_PULSE_IANDD && pulse != MPSK_RX_PULSE_RRC)
    return NULL;

  /* Derive what is not a design axis (doppler#644). Zero asks for the
     object's own answer; every validator below previously REJECTED zero, so
     no working call site can be relying on it. The derivation runs BEFORE the
     validation so a derived value is checked by the same guards a supplied
     one is — a rule that produced an invalid answer must still be caught.

     The ONE difference between the faces is the rate handed to the m_out
     rule: the real cascade sits behind the R2C halfband and therefore sees
     `sps/2`, strictly. That is how design/mpsk.md §8's second rule falls out
     of the shared one instead of being a second rule to keep in step. */
  if (m_out == 0u)
    m_out = real ? mpsk_rx_derive_m_out (0.5 * sps, 1) /* sps > 2*m_out */
                 : mpsk_rx_derive_m_out (sps, 0);      /* sps >= m_out  */
  if (zeta == 0.0)
    zeta = MPSK_RX_ZETA_DEFAULT;
  if (num_phases == 0u)
    num_phases = MPSK_RX_NUM_PHASES_DEFAULT;
  if (lock_thresh == 0.0)
    lock_thresh = MPSK_RX_LOCK_THRESH_DEFAULT;
  if (bn_agc_ratio == 0.0)
    bn_agc_ratio = MPSK_RX_AGC_RATIO_DEFAULT;
  /* Written as !(x >= y) so a NaN parameter is rejected, not accepted. The
     real face's bound is 2*m_out and strict: the cascade behind the R2C
     halfband runs at twice the overall rate, and Ddcr requires that below
     0.5. */
  const int sps_ok
      = real ? (sps > 2.0 * (double)m_out) : (sps >= (double)m_out);
  if (m_out < 2u || m_out > (size_t)RATESYNC_MAX_M || (m_out & 1u) != 0u
      || !sps_ok || !(rrc_beta >= 0.0) || !(rrc_beta <= 1.0) || rrc_span < 1
      || !(bn_carrier >= 0.0) || !(bn_timing >= 0.0) || !(zeta > 0.0)
      || num_phases < 2u
      || (num_phases & (num_phases - 1u)) != 0u
      /* An AGC at or above the bandwidth of a loop it feeds corrects the
         excursions that loop is producing; the two then integrate against
         each other. The invariant is structural rather than advisory. */
      || !(bn_agc_ratio > 0.0) || !(bn_agc_ratio < 1.0))
    return NULL;

  mpsk_receiver_state_t *rx = calloc (1, sizeof (*rx));
  if (!rx)
    return NULL;
  rx->real = real ? 1 : 0;

  /* Both LOs use the conjugate convention of the old carrier_nda NCO: a
     carrier at +f is brought to DC by tuning the LO to -f. init_norm_freq
     keeps its caller-facing meaning (the carrier offset to remove) on both
     faces, so the sign flips exactly here and nowhere else.

     Ddcr's law carries two more terms, and they are its contract rather than
     a guess (ddcr_core.h): its LO runs at the INTERMEDIATE rate (fs_in/2) and
     the R2C halfband has an fs/4 shift baked in, so tuning a real tone at
     input-normalised f_c to DC is norm_freq = -(2*f_c + 0.5). Differentiating
     that gives the 0.5 the readbacks below carry — a deviation of d at the
     intermediate rate is d/2 in input-normalised terms. */
  if (real)
    rx->fe.r = ddcr_create_matched (
        -(2.0 * init_norm_freq + 0.5), (double)m_out / sps, pulse, rrc_beta,
        (size_t)rrc_span, (double)m_out, num_phases);
  else
    rx->fe.c = ddc_create_matched (-init_norm_freq, (double)m_out / sps, pulse,
                                   rrc_beta, (size_t)rrc_span, (double)m_out,
                                   num_phases);
  if (!(real ? (void *)rx->fe.r : (void *)rx->fe.c))
    {
      free (rx);
      return NULL;
    }
  rx->centre_freq = init_norm_freq;

  /* A complex front end mixes at the input rate, so the LO sees `sps` samples
     per symbol; the real one's halfband decimates 2:1 first, so its LO sees
     `sps/2`. That is the whole reason mpsk_rx_loops_init() takes lo_sps
     separately from sps rather than assuming they are equal. */
  mpsk_rx_loops_init (&rx->l, m, sps, real ? 0.5 * sps : sps, m_out,
                      bn_carrier, zeta, bn_timing, bn_agc_ratio,
                      RATESYNC_TED_GARDNER, lock_thresh, differential);
  ratesync_loop_bind_cascade (&rx->l.timing, mpsk_rx_fe_rc (rx));

  /* The front end levels itself so the TED's construct-time slope means what
     it says. A zero loop bandwidth leaves nothing to be slower than, so the
     derived AGC bandwidth is zero and enable_agc declines -- the receiver is
     then simply un-levelled, which is the honest reading of bn = 0.

     The wedge goes in the same place on both faces: inside the cascade,
     before the terminal matched stage. Behind the halfband on the real face,
     the AGC therefore sees the analytic signal at the intermediate rate,
     which is also where the noise has already been filtered -- so the level
     it sets does not depend on how far the front end oversamples. */
  if (agc)
    (void)RateConverter_enable_agc (
        mpsk_rx_fe_rc (rx),
        mpsk_rx_agc_bn (bn_carrier, bn_timing, bn_agc_ratio),
        MPSK_RX_AGC_ALPHA);
  return rx;
}

mpsk_receiver_state_t *
mpsk_receiver_create (int m, double sps, size_t m_out, int pulse,
                      double rrc_beta, int rrc_span, double bn_carrier,
                      double zeta, double bn_timing, double lock_thresh,
                      double init_norm_freq, int differential,
                      size_t num_phases, int agc, double bn_agc_ratio)
{
  return mpsk_rx_create_impl (0, m, sps, m_out, pulse, rrc_beta, rrc_span,
                              bn_carrier, zeta, bn_timing, lock_thresh,
                              init_norm_freq, differential, num_phases, agc,
                              bn_agc_ratio);
}

mpsk_receiver_state_t *
mpsk_receiver_create_real (int m, double sps, size_t m_out, int pulse,
                           double rrc_beta, int rrc_span, double bn_carrier,
                           double zeta, double bn_timing, double lock_thresh,
                           double init_norm_freq, int differential,
                           size_t num_phases, int agc, double bn_agc_ratio)
{
  return mpsk_rx_create_impl (1, m, sps, m_out, pulse, rrc_beta, rrc_span,
                              bn_carrier, zeta, bn_timing, lock_thresh,
                              init_norm_freq, differential, num_phases, agc,
                              bn_agc_ratio);
}

double
mpsk_receiver_get_agc_gain_db (const mpsk_receiver_state_t *state)
{
  return RateConverter_agc_gain_db (mpsk_rx_fe_rc (state));
}

/* The Hz face. A pure delegate -- but the conversion it performs is the
   whole reason it exists, so it is written out
   rather than inlined into the call: `sps` is a RATIO the caller should never
   have had to compute, and the LO centre is a normalised frequency they
   should never have had to express. Both are derived here, once, from units
   somebody reading a capture actually has.

   The two refusals are worth their lines. A non-positive rate makes `sps`
   undefined or negative, and a carrier outside Nyquist is a mis-stated
   capture rather than a tuning request -- returning NULL for either says so
   at construction instead of at the first strobe that lands nowhere. */
mpsk_receiver_state_t *
mpsk_receiver_create_bpsk (double sample_rate_hz, double symbol_rate_hz,
                           double carrier_freq_hz, int pulse, double rrc_beta,
                           int rrc_span, double bn_carrier, double bn_timing,
                           int differential, int agc)
{
  if (!(sample_rate_hz > 0.0) || !(symbol_rate_hz > 0.0))
    return NULL;
  if (fabs (carrier_freq_hz) >= 0.5 * sample_rate_hz)
    return NULL;
  return mpsk_receiver_create (
      2,                                          /* m -- the type says it */
      sample_rate_hz / symbol_rate_hz, 0u,        /* m_out        -> derived */
      pulse, rrc_beta, rrc_span, bn_carrier, 0.0, /* zeta  -> derived */
      bn_timing, 0.0,                             /* lock_thresh -> derived */
      carrier_freq_hz / sample_rate_hz, differential, 0u, /* num_phases */
      agc, 0.0); /* bn_agc_ratio -> derived */
}

void
mpsk_receiver_destroy (mpsk_receiver_state_t *state)
{
  if (!state)
    return;
  if (state->real)
    ddcr_destroy (state->fe.r);
  else
    ddc_destroy (state->fe.c);
  free (state);
}

void
mpsk_receiver_reset (mpsk_receiver_state_t *state)
{
  if (state->real)
    ddcr_reset (state->fe.r);
  else
    ddc_reset (state->fe.c);
  mpsk_rx_loops_reset (&state->l);
}

/* ── the block API: one body per verb, two input types ─────────────────────
 *
 * `real` arrives as a LITERAL from each entry point below, so both the dtype
 * branch here and the front-end call inside step_ted() fold away — the same
 * specialisation the `ted` parameter uses. Written once rather than copied
 * per face because two copies of this loop is exactly the drift the collapse
 * removes: the previous real-input twin's steps() had already grown its own
 * telemetry hoist, its own capacity guard and its own comment about both. */
JM_FORCEINLINE static int
mpsk_rx_step_at (mpsk_receiver_state_t *s, const void *x, size_t i,
                 float complex *y, int real)
{
  return real ? mpsk_receiver_step_real_ted (s, ((const float *)x)[i], y,
                                             RATESYNC_TED_GARDNER)
              : mpsk_receiver_step_ted (s, ((const float complex *)x)[i], y,
                                        RATESYNC_TED_GARDNER);
}

JM_FORCEINLINE static size_t
mpsk_rx_steps_impl (mpsk_receiver_state_t *state, const void *x, size_t x_len,
                    float complex *out, size_t max_out, int real)
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
          if (mpsk_rx_step_at (state, x, i, &y, real) && emitted < max_out)
            out[emitted++] = y;
        }
    }
  else
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float complex y;
          if (mpsk_rx_step_at (state, x, i, &y, real))
            {
              if (emitted < max_out)
                out[emitted++] = y;
              mpsk_rx_tlm_flush (&state->l, y);
            }
        }
    }
  return emitted;
}

JM_FORCEINLINE static size_t
mpsk_rx_bits_impl (mpsk_receiver_state_t *state, const void *x, size_t x_len,
                   uint8_t *out, size_t max_out, int real)
{
  size_t emitted = 0;
  /* Guarded in-loop flush (not the steps() split): this loop already makes a
     per-symbol call, so there is no pristine register-resident fast path. */
  for (size_t i = 0; i < x_len; i++)
    {
      float complex y;
      if (!mpsk_rx_step_at (state, x, i, &y, real))
        continue;
      if (state->l.tlm.ctx)
        mpsk_rx_tlm_flush (&state->l, y);
      uint8_t bits[3];
      int     nb = mpsk_rx_symbol_to_bits (&state->l, y, bits);
      for (int b = 0; b < nb && emitted < max_out; b++)
        out[emitted++] = bits[b];
    }
  return emitted;
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
  return mpsk_rx_steps_impl (state, x, x_len, out, max_out, 0);
}

size_t
mpsk_receiver_steps_real_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0; /* sps > 2*m_out, so symbols <= inputs */
}

size_t
mpsk_receiver_steps_real (mpsk_receiver_state_t *state, const float *x,
                          size_t x_len, float complex *out, size_t max_out)
{
  return mpsk_rx_steps_impl (state, x, x_len, out, max_out, 1);
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
  return mpsk_rx_bits_impl (state, x, x_len, out, max_out, 0);
}

size_t
mpsk_receiver_bits_real_max_out (mpsk_receiver_state_t *state)
{
  (void)state;
  return 0; /* one bit per symbol, and symbols <= inputs */
}

size_t
mpsk_receiver_bits_real (mpsk_receiver_state_t *state, const float *x,
                         size_t x_len, uint8_t *out, size_t max_out)
{
  return mpsk_rx_bits_impl (state, x, x_len, out, max_out, 1);
}

/* ── the frequency accessors: the rate convention, and nothing else ────────
 *
 * The real face's LO runs at the intermediate rate (fs_in/2), so a deviation
 * of d there is d/2 in the input-normalised cycles/sample every caller-facing
 * frequency on this object is quoted in. That factor is the ONLY thing the
 * tag buys here — the estimate itself is the shared loop's. */
static double
mpsk_rx_lo_to_input (const mpsk_receiver_state_t *s)
{
  return s->real ? 0.5 : 1.0;
}

double
mpsk_receiver_get_norm_freq (const mpsk_receiver_state_t *state)
{
  return state->centre_freq
         + mpsk_rx_lo_to_input (state) * mpsk_rx_freq_est (&state->l);
}

double
mpsk_receiver_get_nco_freq (const mpsk_receiver_state_t *state)
{
  /* The instantaneous command includes the proportional nudge, and is held in
     the front end's (conjugate) convention — report the receiver's. */
  return state->centre_freq - mpsk_rx_lo_to_input (state) * state->l.freq_ctrl;
}

void
mpsk_receiver_set_norm_freq (mpsk_receiver_state_t *state, double val)
{
  state->centre_freq = val;
  if (state->real)
    ddcr_set_norm_freq (state->fe.r, -(2.0 * val + 0.5));
  else
    ddc_set_norm_freq (state->fe.c, -val);
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
  int rc_agc = state->real ? ddcr_set_telemetry (state->fe.r, tlm, name, decim)
                           : ddc_set_telemetry (state->fe.c, tlm, name, decim);
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
  return state->l.car_lock.up_thresh;
}

double
mpsk_receiver_get_lock_drop_thresh (const mpsk_receiver_state_t *state)
{
  return state->l.car_lock.down_thresh;
}

double
mpsk_receiver_get_sync_lock_thresh (const mpsk_receiver_state_t *state)
{
  return state->l.timing.lock.up_thresh;
}

double
mpsk_receiver_get_sync_lock_drop_thresh (const mpsk_receiver_state_t *state)
{
  return state->l.timing.lock.down_thresh;
}

size_t
mpsk_receiver_get_num_phases (const mpsk_receiver_state_t *state)
{
  return mpsk_rx_fe_rc (state)->num_phases;
}

int
mpsk_receiver_get_clipped (const mpsk_receiver_state_t *state)
{
  return (state->real ? ddcr_get_clipped (state->fe.r)
                      : ddc_get_clipped (state->fe.c))
             ? 1
             : 0;
}

/* ── Serializable state — two children, no scalars of our own ───────────────
 *
 * One triplet over both faces: the layout is [hdr][front end][loops] either
 * way, and only the front end's own child blob differs in size and content.
 * The `real` tag is config — create() restores it — so it is not packed; what
 * carries it across the wire is the ENVELOPE MAGIC, which is keyed on the
 * face precisely so a complex blob handed to a real receiver is refused by
 * name here rather than reinterpreted or caught three levels down. */

static size_t
mpsk_rx_fe_state_bytes (const mpsk_receiver_state_t *s)
{
  return s->real ? ddcr_state_bytes (s->fe.r) : ddc_state_bytes (s->fe.c);
}

static uint32_t
mpsk_rx_state_magic (const mpsk_receiver_state_t *s)
{
  return s->real ? MPSK_RECEIVER_R_STATE_MAGIC : MPSK_RECEIVER_STATE_MAGIC;
}

static uint16_t
mpsk_rx_state_version (const mpsk_receiver_state_t *s)
{
  return s->real ? MPSK_RECEIVER_R_STATE_VERSION : MPSK_RECEIVER_STATE_VERSION;
}

size_t
mpsk_receiver_state_bytes (const mpsk_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + mpsk_rx_fe_state_bytes (s)
         + mpsk_rx_loops_state_bytes (&s->l);
}

void
mpsk_receiver_get_state (const mpsk_receiver_state_t *s, void *blob)
{
  const size_t total = mpsk_receiver_state_bytes (s);
  dp_writer_t  w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, mpsk_rx_state_magic (s), mpsk_rx_state_version (s), total);
  char *p = (char *)blob + w.off;
  if (s->real)
    ddcr_get_state (s->fe.r, p);
  else
    ddc_get_state (s->fe.c, p);
  p += mpsk_rx_fe_state_bytes (s);
  mpsk_rx_loops_get_state (&s->l, p);
}

int
mpsk_receiver_set_state (mpsk_receiver_state_t *s, const void *blob)
{
  const size_t total = mpsk_receiver_state_bytes (s);
  int          rc    = dp_state_validate (blob, total, mpsk_rx_state_magic (s),
                                          mpsk_rx_state_version (s));
  if (rc != DP_OK)
    return rc;
  const char *p = (const char *)blob + sizeof (dp_state_hdr_t);
  rc = s->real ? ddcr_set_state (s->fe.r, p) : ddc_set_state (s->fe.c, p);
  if (rc != DP_OK)
    return rc;
  p += mpsk_rx_fe_state_bytes (s);
  return mpsk_rx_loops_set_state (&s->l, p);
}
