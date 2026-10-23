#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/util/util_core.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Defined below, beside the refine chain that uses it. */
static dp_carrier_acq_state_t *
adr_new_carrier_acq (const dp_async_dsss_receiver_state_t *s,
                     double                                target_rate);

/* The tracker's symbol period in Dll partials: `segments` partials a code
   period, chip_rate / code_len periods a second, symbol_rate symbols. One
   formula for the build site that sets the period and the checks that
   vouch for it. */
static double
adr_partials_per_symbol (size_t segments, double chip_rate, size_t code_len,
                         double symbol_rate)
{
  return (double)segments * chip_rate / ((double)code_len * symbol_rate);
}

/* Whether a tracker chain at `segments` partials and `sps` samples a symbol
   can be built. Its Dll's arguments are inside dp_dll_params_ok(). sps >= 2,
   since sps = 1 carries no m_out (dsss_receiver_core.c, gh-782). The symbol
   is no faster than a chip, and at most 2^20 partials long at ANY segments:
   that keeps the chain's resampling ratio, sps over the partials, in
   [sps / 2^20, sps * code_len / segments], where a tiny symbol rate had
   underflowed it to 0 and a huge one sized the scratch at ~4e12 samples, both
   aborting. Under 2 partials the tracker keeps per-partial looks; any longer
   period the Dll's aid takes. create() and configure_chain_raw() both refuse
   outside it, so the chain built mid-stream under dp_xnn never meets a
   refusal (doppler#2103). */
static int
adr_track_chain_ok (size_t code_len, double chip_rate, double symbol_rate,
                    size_t segments, size_t sps)
{
  const double partials
      = adr_partials_per_symbol (segments, chip_rate, code_len, symbol_rate);
  return sps >= 2
         && dp_dll_params_ok (code_len, 0.0, ASYNC_DSSS_RX_DLL_BN,
                              ASYNC_DSSS_RX_DLL_ZETA,
                              ASYNC_DSSS_RX_DLL_SPACING, segments)
         && symbol_rate <= chip_rate && partials <= DLL_AID_MAX_PERIOD
         && (segments < 2 || partials < 2.0
             || (dp_dll_symbol_period_ok (segments, partials)
                 /* set_symbol_period() also refuses a bandwidth whose gains
                    overflow at the period's interval: the same check here,
                    so the hand-off's set can never be refused (#2103). */
                 && dp_loop_filter_params_ok (ASYNC_DSSS_RX_DLL_BN,
                                              ASYNC_DSSS_RX_DLL_ZETA,
                                              partials / (double)segments)));
}

/* MpskReceiver's terminal outputs per symbol (`m_out`) for a chain at
   `sps`: the one rule, mpsk_rx_derive_m_out() (mpsk_rx_loops.h), on the
   complex face's inclusive bound, as DsssReceiver and MpskReceiver itself
   derive it. Even, in [2, 8], since create() refuses sps < 2. */
static int
adr_m_out (size_t sps)
{
  return (int)mpsk_rx_derive_m_out ((double)sps, 0);
}

/* Reset the symbol-lock detector's RUNNING state (config -- alpha, lockdet
 * thresholds/counts -- is set once in create()). Called on every fresh
 * track-chain build so each pass starts unlocked. */
static void
adr_reset_lock (dp_async_dsss_receiver_state_t *s)
{
  s->lock_num    = 0.0;
  s->lock_den    = 0.0;
  s->lock_metric = 0.0;
  dp_lockdet_reset (&s->sym_lockdet);
}

/* The code phase the live chain starts from: the seed's, advanced by the
 * clock dilation the Doppler implies over the refine (samples_consumed_refine
 * samples). Off when carrier_freq_hz == 0: no carrier, no dilation. The one
 * formula the hand-off uses, for the refined value and the seed fallback. */
static double
adr_handover_phase (const dp_async_dsss_receiver_state_t *state,
                    double doppler_hz_est, uint64_t samples_consumed_refine)
{
  double phase = state->seed_chip_phase;
  if (state->carrier_freq_hz > 0.0)
    {
      double dilation = doppler_hz_est / state->carrier_freq_hz;
      double chips_elapsed
          = (double)samples_consumed_refine / (double)state->spc;
      phase = dp_fmod_pos (phase + dilation * chips_elapsed,
                           (double)state->code_len);
    }
  return phase;
}

/* Every state transition goes through here so the two running clocks stay
 * honest: `state_samples` counts from the entry, and the release clock
 * (`both_down_samples`) only ever runs inside tracking. */
static void
adr_enter (dp_async_dsss_receiver_state_t *s, int state)
{
  s->state         = state;
  s->state_samples = 0;
  /* Lost KEEPS the release clock and goes on counting it (the status
   * record's both_down_samples: "in lost it keeps counting -- samples since
   * the flags dropped"); every other entry restarts it. */
  if (state != ASYNC_DSSS_RX_LOST)
    s->both_down_samples = 0;
}

/* Allocate a fresh refine-stage chain (frozen carrier + collection Dll +
 * RateConverter + CarrierAcquisition, plus their scratch buffers) from
 * the given hand-off phase/frequency, without touching `s`'s existing
 * children -- the fail-safe half of the "allocate everything first"
 * regrid discipline (dsss_receiver_core.c's own dsss_rx_build_chain is the
 * precedent). `s->refine_segments` is assumed already set (fixed for the
 * object's lifetime -- see dp_async_dsss_receiver_create()). The sub-object
 * allocations are small and internal, so they are trusted (the house
 * "no error handling for impossible scenarios" rule) -- this build cannot
 * fail. */
static void
adr_build_refine_chain (
    dp_async_dsss_receiver_state_t *s, double chip_phase,
    double doppler_hz_est, dp_costas_state_t *car_frozen_out,
    dp_dll_state_t **refine_dll_out, dp_RateConverter_state_t **refine_rc_out,
    dp_carrier_acq_state_t **ca_out, float _Complex **dll_out_buf_out,
    size_t *dll_out_cap_out, float _Complex **rc_out_buf_out,
    size_t *rc_out_cap_out)
{
  /* Frozen carrier: costas_update() is never called on this instance --
   * the direct C equivalent of Python's freeze_carrier=True. bn/tsamps
   * below are inert placeholders; only costas_wipeoff()'s phase accumulator
   * is ever touched. bn_fll = 0: no FLL anywhere in this object (see the
   * ASYNC_DSSS_RX_BN_CARRIER comment). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  dp_costas_init (car_frozen_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
                  doppler_hz_est / front_end_rate, s->tsamps, 0.0);

  dp_dll_state_t *refine_dll = dp_xnn (
      dp_dll_create (s->code, s->code_len, s->spc, chip_phase,
                     ASYNC_DSSS_RX_DLL_BN, ASYNC_DSSS_RX_DLL_ZETA,
                     ASYNC_DSSS_RX_REFINE_DLL_SPACING, s->refine_segments));

  /* Carrier->code aiding on the collection Dll too: without it the coupled
     code-rate Doppler drifts the code phase over the frozen refine prefix
     (~55 chips at a 50 kHz offset / 2.5 GHz over ~0.9 s), degrading the very
     despread stream CarrierAcquisition estimates from. Seeded off the coarse
     handoff Doppler (the refined value isn't known yet); the ~1 kHz coarse
     residual leaves only a small aiding error. Off when carrier_freq_hz==0. */
  if (s->carrier_freq_hz > 0.0)
    dp_dll_set_rate_aid (refine_dll, doppler_hz_est / s->carrier_freq_hz);

  double partial_rate
      = s->chip_rate * (double)s->refine_segments / (double)s->code_len;
  double target_rate = (double)s->refine_samples_per_symbol * s->symbol_rate;

  dp_RateConverter_state_t *refine_rc
      = dp_xnn (dp_RateConverter_create (target_rate / partial_rate, 0));

  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  dp_carrier_acq_state_t *ca = adr_new_carrier_acq (s, target_rate);

  float _Complex *dll_out_buf
      = dp_xmalloc (s->refine_segments * sizeof (*dll_out_buf));

  size_t rc_out_cap
      = (size_t)((double)s->refine_segments * refine_rc->rate) + 64;
  float _Complex *rc_out_buf = dp_xmalloc (rc_out_cap * sizeof (*rc_out_buf));

  *refine_dll_out  = refine_dll;
  *refine_rc_out   = refine_rc;
  *ca_out          = ca;
  *dll_out_buf_out = dll_out_buf;
  *dll_out_cap_out = s->refine_segments;
  *rc_out_buf_out  = rc_out_buf;
  *rc_out_cap_out  = rc_out_cap;
}

static void
adr_free_refine_chain (dp_async_dsss_receiver_state_t *s)
{
  dp_carrier_acq_destroy (s->ca);
  dp_RateConverter_destroy (s->refine_rc);
  dp_dll_destroy (s->refine_dll);
  free (s->refine_dll_out_buf);
  free (s->refine_rc_out_buf);
  s->ca                 = NULL;
  s->refine_rc          = NULL;
  s->refine_dll         = NULL;
  s->refine_dll_out_buf = NULL;
  s->refine_rc_out_buf  = NULL;
}

/* Build a fresh refine chain and, only on success, swap it in for `s`'s
 * current one -- the "allocate everything first" half of the regrid
 * discipline applied at the call site. Also resets refine_samples_fed
 * and the shared carry buffer (fresh chain: no leftover partial-period
 * tail). Cannot fail (the sub-object allocations are trusted). */
static void
adr_rebuild_refine_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                          double doppler_hz_est)
{
  dp_costas_state_t         car_frozen;
  dp_dll_state_t           *refine_dll  = NULL;
  dp_RateConverter_state_t *refine_rc   = NULL;
  dp_carrier_acq_state_t   *ca          = NULL;
  float _Complex           *dll_out_buf = NULL;
  size_t                    dll_out_cap = 0;
  float _Complex           *rc_out_buf  = NULL;
  size_t                    rc_out_cap  = 0;

  adr_build_refine_chain (s, chip_phase, doppler_hz_est, &car_frozen,
                          &refine_dll, &refine_rc, &ca, &dll_out_buf,
                          &dll_out_cap, &rc_out_buf, &rc_out_cap);

  adr_free_refine_chain (s);
  s->car_frozen         = car_frozen;
  s->refine_dll         = refine_dll;
  s->refine_rc          = refine_rc;
  s->ca                 = ca;
  s->refine_dll_out_buf = dll_out_buf;
  s->refine_dll_out_cap = dll_out_cap;
  s->refine_rc_out_buf  = rc_out_buf;
  s->refine_rc_out_cap  = rc_out_cap;
  s->refine_samples_fed = 0;
  s->refine_periods     = 0;
  s->car_carry_len      = 0;
}

/* Allocate a fresh live-tracking chain (Dll/RateConverter/MpskReceiver +
 * the pre-despread carrier loop), mirroring dsss_receiver_core.c's own
 * dsss_rx_build_chain: dp_costas_init()'s tsamps is one whole code period, and
 * costas_update() is called once per period from a non-data-aided
 * (squaring) combine of that period's emitted coherent-I&D partials (see
 * adr_track_period()) -- not once per partial (see this function's own
 * comment on dp_costas_init() below for why). */
static void
adr_build_track_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                       double doppler_hz_est, size_t segments, size_t sps,
                       int n, dp_costas_state_t *car_out,
                       dp_dll_state_t           **dll_out,
                       dp_RateConverter_state_t **rc_out,
                       dp_mpsk_receiver_state_t **rx_out)
{
  double partial_rate = s->chip_rate * (double)segments / (double)s->code_len;
  double target_rate  = (double)sps * s->symbol_rate;

  dp_dll_state_t *dll = dp_xnn (dp_dll_create (
      s->code, s->code_len, s->spc, chip_phase, ASYNC_DSSS_RX_DLL_BN,
      ASYNC_DSSS_RX_DLL_ZETA, ASYNC_DSSS_RX_DLL_SPACING, segments));
  /* The lock detector's looks, sized for THIS operating point rather than
     left at the DLL's default 20 partials: the symbol period in partials is
     known from configuration, so the detector's looks are the symbol-scale
     max-power windows (dp_dll_set_symbol_period, docs/design/
     async-dsss-receiver.md §3.7), and n_looks is det_n_noncoh over the
     window's coherent length at the design C/N0. Measured before this: at
     Es/N0 5.7 dB the default detector read "unlocked" 96% of the time on a
     loop that never lost the code (§12.3). A period under 2 partials (a
     very fast data clock) keeps per-partial looks and only sizes them. */
  if (segments > 1)
    {
      double partials_per_symbol = adr_partials_per_symbol (
          segments, s->chip_rate, s->code_len, s->symbol_rate);
      /* Under 2 partials the tracker keeps per-partial looks; any longer
         period adr_track_chain_ok() vouched for, at create() and in
         configure_chain_raw(), so a refusal here is a defect, not a
         configuration. */
      if (partials_per_symbol >= 2.0
          && dp_dll_set_symbol_period (dll, partials_per_symbol) != DP_OK)
        abort ();
      size_t win  = dp_dll_get_symbol_window (dll);
      size_t look = (win ? win : 1) * (s->tsamps / segments);
      double amp
          = dp_det_cn0_to_snr (s->cn0_dbhz, s->chip_rate * (double)s->spc);
      int nl = dp_det_n_noncoh (amp, (int)look, 0.99, 1e-3, 4000);
      if (nl >= 1)
        {
          (void)dp_dll_configure_lock (dll, 1e-3, (size_t)nl, 0.0);
          /* The drop count from the miss probability the sizing bought
             (1 - 0.99) at a 1e-6 per-decision false-drop budget -- three,
             against configure_lock's fixed two. */
          (void)dp_dll_set_lock_verify (
              dll, dll->lock.n_up, (uint32_t)dp_det_verify_count (0.01, 1e-6));
        }
    }

  /* Carrier->code aiding: the code-rate Doppler is coupled to the carrier
     offset through the same v/c, so feed the refined carrier estimate into
     the code NCO's rate. The discriminator alone cannot pull in that
     constant rate offset at low SNR (~24.6 chips/s at 20 kHz / 2.5 GHz), so
     unaided the code loop walks off even with a correct carrier. Off when
     carrier_freq_hz == 0 (a pure baseband offset with no clock dilation). */
  if (s->carrier_freq_hz > 0.0)
    dp_dll_set_rate_aid (dll, doppler_hz_est / s->carrier_freq_hz);

  dp_RateConverter_state_t *rc
      = dp_xnn (dp_RateConverter_create (target_rate / partial_rate, 0));

  /* MpskReceiver's own carrier loop is seeded at 0, NOT doppler_hz_est
   * again -- same reasoning as dsss_receiver_core.c's own
   * dsss_rx_build_chain(): the pre-despread Costas loop below already removes
   * the FULL physical Doppler, so only a small residual should reach
   * MpskReceiver. Re-seeding it with the full doppler_hz_est (evaluated at
   * target_rate=sps*symbol_rate, a much smaller rate than the front end)
   * double-counts and can alias past Nyquist at large offsets, far
   * outside MpskReceiver's own carrier_nda pull-in range. */
  /* ASYNC_DSSS_RX_DEMOD_BN_TIMING 0.005, matching dsss_receiver_core.c's
     own dsss_rx_build_chain -- see the measurement table there. Short
     version: the rebuild's timing
     loop steers RateSync's accumulator rather than a Farrow interpolator, 0.01
     is too wide for that on a despread stream, and the value is chosen on
     steady-state EVM rather than BER (BPSK's BER saturates first, and the
     BER-optimal 0.001 is ~8 dB worse in EVM). */
  /* `n` is inside dp_ratesync_m_ok(): create() derives it
     (mpsk_rx_derive_m_out) and configure_chain_raw() refuses outside it, so
     this create is never refused on `n` (doppler#2421). */
  dp_mpsk_receiver_state_t *rx = dp_xnn (dp_mpsk_receiver_create (
      s->m, (double)sps, (size_t)n, MPSK_RX_PULSE_IANDD,
      ASYNC_DSSS_RX_DEMOD_RRC_BETA, ASYNC_DSSS_RX_DEMOD_RRC_SPAN,
      ASYNC_DSSS_RX_DEMOD_BN_CARRIER, ASYNC_DSSS_RX_DEMOD_ZETA,
      ASYNC_DSSS_RX_DEMOD_BN_TIMING, ASYNC_DSSS_RX_DEMOD_LOCK_THRESH, 0.0,
      s->differential, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO));

  /* Per-CODE-PERIOD cadence (tsamps = one whole period), matching
   * dsss_receiver_core.c's own mechanism -- NOT once per dp_dll_steps()
   * partial. The carrier discriminator combines the whole period's coherent-
   * I&D partials (a non-data-aided squaring combine, adr_track_period()) into
   * one full-period error; a per-partial cadence would shrink each update's
   * integration by `segments`, too weak/noisy to steer a clean carrier
   * estimate. Pure PLL, no FLL (bn_fll = 0 below). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  /* bn_fll = 0: pure PLL, no FLL (see ASYNC_DSSS_RX_BN_CARRIER's comment --
     the FLL cross-product is too noisy on the despread-symbol input and
     drives the carrier wander that causes the residual slips). */
  dp_costas_init (car_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
                  doppler_hz_est / front_end_rate, s->tsamps, 0.0);

  *dll_out = dll;
  *rc_out  = rc;
  *rx_out  = rx;
}

static void
adr_free_track_chain (dp_async_dsss_receiver_state_t *s)
{
  dp_mpsk_receiver_destroy (s->rx);
  dp_RateConverter_destroy (s->rc);
  dp_dll_destroy (s->dll);
  s->rx  = NULL;
  s->rc  = NULL;
  s->dll = NULL;
}

/* Attach the stored telemetry request (tlm, tlm_prefix, tlm_decim) to every
 * loop the receiver holds right now. set_telemetry calls it, and so does
 * each track-chain rebuild: a seed destroys the Dll, the resampler and the
 * symbol receiver and builds new ones, which would otherwise come up
 * detached. Every name is joined before the first registration, so an
 * overlong prefix is refused with the registry untouched; a child that
 * cannot register fails the whole attach. car_held is attached beside car
 * under the same names, so the same ids: the hold restores car from
 * car_held on entering a coast, and refreshes car_held only while both
 * flags are up, so an attach made with one flag down would otherwise be
 * undone by the next coast -- and the detach clears both, or that restore
 * would hand the carrier a context the caller may have destroyed. The
 * refine stage's chain is not instrumented. */
static int
adr_tlm_attach (dp_async_dsss_receiver_state_t *s)
{
  dp_tlm_t   *t = s->tlm;
  const char *p = s->tlm_prefix;
  uint32_t    d = s->tlm_decim;
  char        car[DP_TLM_NAME_MAX], code[DP_TLM_NAME_MAX], rc[DP_TLM_NAME_MAX],
      demod[DP_TLM_NAME_MAX], acq[DP_TLM_NAME_MAX], sym_lock[DP_TLM_NAME_MAX],
      sym_locked[DP_TLM_NAME_MAX], st[DP_TLM_NAME_MAX],
      handover[DP_TLM_NAME_MAX], refused[DP_TLM_NAME_MAX];
  if (dp_tlm_name_join (car, p, "car") != DP_OK
      || dp_tlm_name_join (code, p, "code") != DP_OK
      || dp_tlm_name_join (rc, p, "rc") != DP_OK
      || dp_tlm_name_join (demod, p, "demod") != DP_OK
      || dp_tlm_name_join (acq, p, "acq") != DP_OK
      || dp_tlm_name_join (sym_lock, p, "sym.lock") != DP_OK
      || dp_tlm_name_join (sym_locked, p, "sym.locked") != DP_OK
      || dp_tlm_name_join (st, p, "state") != DP_OK
      || dp_tlm_name_join (handover, p, "handover_hz") != DP_OK
      || dp_tlm_name_join (refused, p, "refused") != DP_OK)
    return DP_ERR_INVALID;
  int r = dp_costas_set_telemetry (&s->car, t, car, d);
  if (r == DP_OK)
    r = dp_costas_set_telemetry (&s->car_held, t, car, d);
  if (r == DP_OK)
    r = dp_dll_set_telemetry (s->dll, t, code, d);
  if (r == DP_OK)
    r = dp_RateConverter_set_telemetry (s->rc, t, rc, d);
  if (r == DP_OK)
    r = dp_mpsk_receiver_set_telemetry (s->rx, t, demod, d);
  if (r == DP_OK && s->acq)
    r = dp_acq_set_telemetry (s->acq, t, acq, d);
  /* The receiver's own decision, the one its users act on: the symbol-lock
     metric and the lockdet's flag, once per emitted symbol. */
  if (r == DP_OK)
    {
      s->tlm_sym_lock   = dp_tlm_probe (t, sym_lock, d);
      s->tlm_sym_locked = dp_tlm_probe (t, sym_locked, d);
      if (s->tlm_sym_lock < 0 || s->tlm_sym_locked < 0)
        r = DP_ERR_INVALID;
    }
  /* Where the receiver is in its lifecycle, the Doppler its chain was built
     from, and its refused hits: once per steps() call, after it
     (dp_async_dsss_receiver_steps()). The rebuilds re-run this attach, and a
     registered name returns its own id, so these stay one series each. */
  if (r == DP_OK)
    {
      s->tlm_state    = dp_tlm_probe (t, st, d);
      s->tlm_handover = dp_tlm_probe (t, handover, d);
      s->tlm_refused  = dp_tlm_probe (t, refused, d);
      if (s->tlm_state < 0 || s->tlm_handover < 0 || s->tlm_refused < 0)
        r = DP_ERR_INVALID;
    }
  return r;
}

/* Detach every loop. The request itself is the caller's to clear. */
static void
adr_tlm_detach (dp_async_dsss_receiver_state_t *s)
{
  const char *p = s->tlm_prefix;
  (void)dp_costas_set_telemetry (&s->car, NULL, p, 1);
  (void)dp_costas_set_telemetry (&s->car_held, NULL, p, 1);
  (void)dp_dll_set_telemetry (s->dll, NULL, p, 1);
  (void)dp_RateConverter_set_telemetry (s->rc, NULL, p, 1);
  (void)dp_mpsk_receiver_set_telemetry (s->rx, NULL, p, 1);
  if (s->acq)
    (void)dp_acq_set_telemetry (s->acq, NULL, p, 1);
  s->tlm_sym_lock = s->tlm_sym_locked = -1;
  s->tlm_state = s->tlm_handover = s->tlm_refused = -1;
}

/* Size the track chain's per-period scratch for the chain just built. One
 * output per input sample bounds the Dll; the resampler's output is that
 * times its rate plus its own margin. Grow-only, so a regrid to a lower
 * rate leaves the larger buffer in place rather than churning. */
static void
adr_size_track_scratch (dp_async_dsss_receiver_state_t *s)
{
  size_t need_dll = s->tsamps;
  size_t need_rc  = (size_t)((double)s->tsamps * s->rc->rate) + 64;
  if (s->track_dll_out_cap < need_dll)
    {
      free (s->track_dll_out_buf);
      s->track_dll_out_buf
          = dp_xmalloc (need_dll * sizeof *s->track_dll_out_buf);
      s->track_dll_out_cap = need_dll;
    }
  if (s->track_rc_out_cap < need_rc)
    {
      free (s->track_rc_out_buf);
      s->track_rc_out_buf = dp_xmalloc (need_rc * sizeof *s->track_rc_out_buf);
      s->track_rc_out_cap = need_rc;
    }
}

static void
adr_rebuild_track_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                         double doppler_hz_est, size_t segments, size_t sps,
                         int n)
{
  dp_costas_state_t         car;
  dp_dll_state_t           *dll = NULL;
  dp_RateConverter_state_t *rc  = NULL;
  dp_mpsk_receiver_state_t *rx  = NULL;
  adr_build_track_chain (s, chip_phase, doppler_hz_est, segments, sps, n, &car,
                         &dll, &rc, &rx);
  adr_free_track_chain (s);
  s->had_lock      = 0; /* a fresh chain pulls in before it may coast */
  s->car_coasting  = 0;
  s->car           = car;
  s->car_held      = car;
  s->dll           = dll;
  s->rc            = rc;
  s->rx            = rx;
  s->segments      = segments;
  s->sps           = sps;
  s->n             = n;
  s->car_carry_len = 0;
  adr_size_track_scratch (s);
  adr_reset_lock (s); /* fresh symbol-lock per pass */
  /* The rebuilt loops report under the names the old ones did, so the
     attachment -- and every probe id -- survives the seed. Same names, so
     nothing new registers and this cannot fill the table. */
  if (s->tlm)
    (void)adr_tlm_attach (s);
}

/* The residual-carrier estimator on a despread stream at `target_rate`
 * samples per second -- the refine chain's. The resolution is
 * refine_samples_per_symbol * symbol_rate / refine_n_fft, the design SNR the
 * derated C/N0's, the dwell floored at refine_min_blocks (#1265,
 * docs/design/async-dsss-receiver-measurements.md §12.16: det_n_noncoh sized
 * it for detection at the derated C/N0, two blocks at 45 dB-Hz with the
 * shipped margin, while the estimate's noise the chain must pull in from is
 * 210 Hz there and 77 at seven), bounded by the give-up cap. */
static dp_carrier_acq_state_t *
adr_new_carrier_acq (const dp_async_dsss_receiver_state_t *s,
                     double                                target_rate)
{
  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  double effective_cn0_dbhz = s->cn0_dbhz - s->refine_design_margin_db;
  double design_snr    = dp_det_cn0_to_snr (effective_cn0_dbhz, target_rate);
  double resolution_hz = (double)s->refine_samples_per_symbol * s->symbol_rate
                         / (double)s->refine_n_fft;
  dp_carrier_acq_state_t *ca = dp_xnn (dp_carrier_acq_create (
      target_rate, s->symbol_rate, resolution_hz, s->refine_zero_pad,
      DP_PSD_WINDOW_HANN, 0.0f, NULL, 0, s->pfa, s->pd, design_snr,
      s->refine_sequential, s->refine_max_n_blocks));
  if (ca->dwell_target < s->refine_min_blocks)
    ca->dwell_target = s->refine_min_blocks;
  if (ca->dwell_target > ca->max_n_blocks)
    ca->dwell_target = ca->max_n_blocks;
  return ca;
}

/* One frozen-carrier-wiped code period through refine_dll -> refine_rc ->
 * CarrierAcquisition. `period` must be exactly s->tsamps samples.
 * dp_carrier_acq_steps() is documented as a no-op once ready (or the
 * give-up cap) is reached, so calling it past that point is harmless.
 *
 * `refine_dll` MUST oversample the epoch: with asynchronous data the
 * residual carrier rides a ~symbol_rate-wide data-modulated spectrum, and
 * CarrierAcquisition's PSDMF can only locate its centre if that spectrum is
 * sampled above its Nyquist. `dp_dll_lookback_segments(refine_max_error_db)`
 * splits each epoch into `refine_segments` coherent integrate-and-dump
 * windows -- each window is phase-coherent internally (what the per-block
 * FFT consumes), and the several windows per epoch raise the despread rate
 * to `refine_segments * epoch_rate`, comfortably above the data bandwidth
 * before refine_rc feeds the estimator. The degenerate `segments=1` (one
 * dump per epoch, ~epoch_rate ~= 1 sample/symbol here) undersamples that
 * spectrum: any nonzero coarse-handoff residual aliases, and the estimate
 * collapses to near-DC. See objects/async_dsss_receiver.toml's
 * refine_max_error_db comment. */
static void
adr_refine_period (dp_async_dsss_receiver_state_t *s,
                   const float _Complex           *period)
{
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car_frozen, period[i]);

  size_t n_dll = dp_dll_steps (s->refine_dll, s->car_wiped_buf, s->tsamps,
                               s->refine_dll_out_buf, s->refine_dll_out_cap);
  if (n_dll == 0)
    return;

  size_t n_rc
      = dp_RateConverter_execute (s->refine_rc, s->refine_dll_out_buf, n_dll,
                                  s->refine_rc_out_buf, s->refine_rc_out_cap);
  if (n_rc == 0)
    return;

  dp_carrier_acq_steps (s->ca, s->refine_rc_out_buf, n_rc);
}

/* The refine has decided: the carrier estimate is ready, or it has given
 * up at its block cap. The one test both the period loop below and
 * steps() ask, so the loop stops exactly where steps() hands over. */
static int
adr_refine_decided (const dp_async_dsss_receiver_state_t *s)
{
  const dp_carrier_acq_state_t *ca = s->ca;
  size_t cap = ca->sequential ? ca->max_n_blocks : ca->dwell_target;
  return ca->ready || ca->n_blocks >= cap;
}

/* Refine-stage entry point: buffer raw samples into whole `tsamps`-sample
 * periods (the shared car_carry_buf -- refine and track never run
 * concurrently) and run adr_refine_period() on each complete one, up to
 * the period the refine decides in. The rest of the call is the tracking
 * chain's: steps() feeds it the tail, so folding it here too would pay for
 * those samples twice (#2395). */
static void
adr_process_refine (dp_async_dsss_receiver_state_t *s, const float _Complex *x,
                    size_t x_len)
{
  size_t pos = 0;

  if (s->car_carry_len > 0)
    {
      size_t need = s->tsamps - s->car_carry_len;
      size_t take = (need <= x_len) ? need : x_len;
      memcpy (s->car_carry_buf + s->car_carry_len, x, take * sizeof (*x));
      s->car_carry_len += take;
      pos = take;
      if (s->car_carry_len < s->tsamps)
        return;
      adr_refine_period (s, s->car_carry_buf);
      s->refine_periods++;
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len && !adr_refine_decided (s))
    {
      adr_refine_period (s, x + pos);
      s->refine_periods++;
      pos += s->tsamps;
    }

  /* Decided: the rest of x is the tracking chain's tail, which steps()
     feeds it, and the track rebuild clears this buffer. Buffering it here
     would also overrun car_carry_buf, which holds at most one period: only
     a loop that consumed every whole period leaves less than one. */
  if (adr_refine_decided (s))
    {
      s->car_carry_len = 0;
      return;
    }
  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;
}

/* One carrier-wiped code period through dll -> ONE costas_update() call per
 * period, driven by a NON-DATA-AIDED (squaring) carrier discriminator over
 * that period's emitted coherent-I&D partials. A code period spans ~0.9 data
 * symbols at SPEC's async ratio, so a data transition lands inside nearly
 * every period; the earlier decision-directed combine (sign-align each
 * partial by its own real part, then sum) was corrupted by exactly those
 * transitions -- direct measurement showed a +/-57deg discriminator that
 * averaged to zero, so the loop never tracked and the whole carrier burden
 * silently fell to the post-despread MpskReceiver loop (which then held the
 * Type-II ramp phase error). Squaring removes the +/-1 BPSK data with NO
 * per-partial sign decision (dll_out[i]^2 = A^2 exp(j2phi) regardless of the
 * data bit); a partial straddling a transition merely loses amplitude, not
 * phase. This is the SAME non-data-aided principle the downstream
 * MpskReceiver carrier loop already uses, applied PRE-despread on the
 * transition-free windows so despreading stays coherent (no residual carrier
 * rotating within the correlation) -- the "unfreeze and track loop 1" step
 * of coarse -> freeze -> refine -> track. The dll_out partials are the actual
 * despread symbol stream this object also hands to RateConverter/mpsk_
 * receiver, so this squaring lives here in the consumer, not in dll_core.c.
 * Writes emitted partials into dll_out (capacity max_out); returns count. */
static size_t
adr_track_period (dp_async_dsss_receiver_state_t *s,
                  const float _Complex *period, float _Complex *dll_out,
                  size_t max_out)
{
  /* The coast (#1271). Once locked in this stint, BOTH flags down means
     the emitter is gone or faded, and both loops hold what they settled
     on with both flags up, their lock detectors still looking; one flag
     down is a degrade and the loops run. Left running on noise, a
     departed receiver's Dll free-runs at whatever its filter holds
     (measured: up to 90 chips per second), sweeps through every live
     emitter's code phase and can capture one that crosses slowly enough,
     its code flag then flickering on a neighbour and restarting the
     release clock for as long as it follows it. A genuine return lands
     on the held replica, lights a flag, and both loops run again on it;
     a neighbour that lights the code flag as it passes steers the loops
     for the blip and no more -- the hold point is marked with both flags
     up (dp_dll_hold_here, car_held), so what a blip steers is dropped on
     re-entry (measured: a neighbour 2 kHz off crossing at 4 chips per
     second, 8-10 blips, no follow). Holding on ONE flag down was tried
     and measured wrong: two live emitters 475 Hz apart crossing at a chip
     per second degrade both receivers' symbol flags, and a code loop held
     through that cannot re-centre on its own emitter afterwards -- both
     flags then stay down for the rest of the watch, a false loss. Run,
     it rides the crossing out (code lock held in 5 of 6 trials). A
     neighbour within about a kilohertz crossing at a chip or two a second
     can lock the carrier, and to a receiver on its own that IS a return --
     the parallel controller, which knows that emitter is held, is where it
     is told apart (#1275). Before the first lock the loops must run: that is
     the pull-in. */
  const int code_up = dp_dll_get_locked (s->dll),
            sym_up  = s->sym_lockdet.locked;
  if (code_up && sym_up)
    {
      s->had_lock = 1;
      s->car_held = s->car;
      dp_dll_hold_here (s->dll);
    }
  const int car_coast = s->had_lock && !code_up && !sym_up;
  if (car_coast && !s->car_coasting)
    s->car = s->car_held;
  s->car_coasting = car_coast;
  dp_dll_set_coast (s->dll, s->had_lock && !code_up && !sym_up);
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car, period[i]);
  size_t n_out
      = dp_dll_steps (s->dll, s->car_wiped_buf, s->tsamps, dll_out, max_out);
  if (n_out > 0 && !car_coast)
    {
      /* NON-DATA-AIDED carrier discriminator on the despread coherent-I&D
       * windows. Each emitted partial is dll_out[i] = A * d * exp(j*phi),
       * d = +/-1 BPSK data, phi = residual carrier phase. Squaring removes
       * the data (d^2 = 1) with NO per-partial sign DECISION -- the failure
       * mode of the old decision-directed sum, which at SPEC's async ratio
       * (~0.9 symbols/code period) straddles a data transition almost every
       * period, giving a garbage +/-57deg discriminator that averages to
       * zero so the loop never tracks. The dp_dll_steps() windows are short
       * (dp_dll_lookback_segments oversampling, the same transition-free I&D
       * the refine PSDMF consumes): a window entirely within one symbol
       * squares cleanly; one straddling a transition merely loses amplitude,
       * not phase -- exactly why NDA is transition-robust where the sign-wipe
       * is not. Sum the squares and halve the angle for the carrier phase (mod
       * pi, the harmless BPSK sign ambiguity). Feed it to the existing Costas
       * loop via a unit phasor (Re = cos(phi) >= 0, so its own data- wipe
       * never flips it, and its discriminator reads sin(phi)). This is the
       * same NDA principle the downstream MpskReceiver carrier loop uses,
       * applied PRE-despread so despreading stays coherent (no residual
       * carrier rotating within the correlation) instead of leaving the whole
       * carrier burden to the post-despread loop. */
      float _Complex sq = 0.0f;
      for (size_t i = 0; i < n_out; i++)
        sq += dll_out[i] * dll_out[i];
      double phi       = 0.5 * atan2 (cimag (sq), creal (sq));
      float _Complex P = (float)cos (phi) + (float)sin (phi) * I;
      costas_update (&s->car, P);
      /* costas_update() emits nothing: the composing caller flushes
         the loop's probes once per update (costas_core.h). */
      if (s->car.tlm.ctx)
        dp_costas_tlm_flush (&s->car);
      /* Continuous carrier->code aiding: the pre-despread Costas tracks the
         FULL carrier offset (including the 500 Hz/s ramp), so refresh the
         code NCO's rate bias from it every period. This keeps the initial
         build-time seed (dp_dll_set_rate_aid in adr_build_track_chain) fresh
         as the ramp drifts, and applies at the next period boundary
         (dll_set_rate_ aid only stores the field, never clobbers phase_inc).
         Off when carrier_freq_hz == 0. */
      if (s->carrier_freq_hz > 0.0)
        dp_dll_set_rate_aid (s->dll, dp_costas_get_norm_freq (&s->car)
                                         * (s->chip_rate * (double)s->spc)
                                         / s->carrier_freq_hz);
    }
  return n_out;
}

/* One carrier-wiped code period through the live chain: the Dll's
 * partials, resampled to MpskReceiver's rate, demodulated into `out`. Every
 * stage streams (state carries across calls), so running the chain a period
 * at a time is the same computation as a block at a time -- and it is what
 * lets the scratch be sized once per build instead of per push (#1192).
 * Returns the symbols written. */
static size_t
adr_track_period_chain (dp_async_dsss_receiver_state_t *s,
                        const float _Complex *period, float _Complex *out,
                        size_t max_out)
{
  size_t n_dll = adr_track_period (s, period, s->track_dll_out_buf,
                                   s->track_dll_out_cap);
  size_t n_rc
      = dp_RateConverter_execute (s->rc, s->track_dll_out_buf, n_dll,
                                  s->track_rc_out_buf, s->track_rc_out_cap);
  return dp_mpsk_receiver_steps (s->rx, s->track_rc_out_buf, n_rc, out,
                                 max_out);
}

static size_t
adr_track_chain (dp_async_dsss_receiver_state_t *s, const float _Complex *x,
                 size_t x_len, float _Complex *out, size_t max_out)
{
  size_t emitted = 0;
  size_t pos     = 0;

  if (s->car_carry_len > 0)
    {
      size_t need = s->tsamps - s->car_carry_len;
      size_t take = (need <= x_len) ? need : x_len;
      memcpy (s->car_carry_buf + s->car_carry_len, x, take * sizeof (*x));
      s->car_carry_len += take;
      pos = take;
      if (s->car_carry_len < s->tsamps)
        return 0;
      emitted += adr_track_period_chain (s, s->car_carry_buf, out + emitted,
                                         max_out - emitted);
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      emitted += adr_track_period_chain (s, x + pos, out + emitted,
                                         max_out - emitted);
      pos += s->tsamps;
    }

  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;

  /* Symbol-lock detector: per emitted symbol, integrate the BPSK phase-lock
   * signal (I^2-Q^2)/(I^2+Q^2) = cos(2*phi) as a POWER-WEIGHTED (i.e. SNR-
   * weighted) running mean -- separate EMAs of the numerator and the total
   * power, so a high-|symbol| (high-SNR) symbol dominates and a
   * noise/transient one contributes little -- then step the hysteretic lockdet
   * on the ratio. dwell = 1/lock_alpha >= LOCK_DWELL. See the
   * ASYNC_DSSS_RX_LOCK_* defines. */
  for (size_t i = 0; i < emitted; i++)
    {
      double re = (double)crealf (out[i]);
      double im = (double)cimagf (out[i]);
      s->lock_num
          = dp_ema_step (s->lock_num, re * re - im * im, s->lock_alpha);
      s->lock_den
          = dp_ema_step (s->lock_den, re * re + im * im, s->lock_alpha);
      s->lock_metric = (s->lock_den > 0.0) ? s->lock_num / s->lock_den : 0.0;
      (void)dp_lockdet_step (&s->sym_lockdet, s->lock_metric);
      if (s->tlm && s->tlm_sym_lock >= 0)
        {
          dp_tlm_emit (s->tlm, s->tlm_sym_lock, s->lock_metric);
          dp_tlm_emit (s->tlm, s->tlm_sym_locked,
                       (double)s->sym_lockdet.locked);
        }
    }
  return emitted;
}

/* Restart the search: drain the engine, and owe its next dwell a start on
 * the block grid (adr_align_skip()). Every path back to a fresh search
 * comes through here, so no dwell starts off the grid. */
static void
adr_restart_search (dp_async_dsss_receiver_state_t *s)
{
  dp_acq_reset (s->acq);
  s->align_pending = true;
}

/* Of an @p x_len-sample tail ending at samples_in, how many to discard so
 * the engine's next sample is a multiple of one block, `coherent_bins *
 * code_len * spc`, counted from create. All of them while the boundary is
 * past the tail; the debt is paid when it is reached. */
static size_t
adr_align_skip (dp_async_dsss_receiver_state_t *s, size_t x_len)
{
  const uint64_t block
      = (uint64_t)s->acq->coherent_bins * s->code_len * s->spc;
  const uint64_t pos  = s->samples_in - x_len;
  const uint64_t owed = (block - pos % block) % block;
  if (owed > (uint64_t)x_len)
    return x_len;
  s->align_pending = false;
  return (size_t)owed;
}

/* One signal's code phase, within a chip, circularly over the code: the
 * zone a tracked signal holds and a peak's neighbouring cells share
 * (design async-dsss-parallel.md §3; keyed on code phase alone, never
 * carrier, async-dsss-receiver-measurements.md §12.14). */
static int
adr_same_code_phase (double a, double b, size_t code_len)
{
  double       d  = fabs (a - b);
  const double cl = (double)code_len;
  if (d > cl / 2.0)
    d = cl - d;
  return d <= 1.0;
}

/* A dwell's peak list, strongest first, to the one this receiver takes.
 * Three rules, in order, then the survivors are counted to `rank`:
 *
 * 1. In the zone of a stronger peak (a neighbouring cell, or the same
 *    emitter in a neighbouring tile): that emitter, not a second one.
 * 2. Within reach of the stronger emitters' sidelobes: one emitter's list
 *    also holds its code's Doppler-offset sidelobes, up to dsl below it and
 *    hundreds of chips from its lag, where rule 1 cannot reach (design
 *    acq-surface.md §8). A peak is a second emitter only if its statistic
 *    exceeds dsl times the stronger emitters' bound by more than the
 *    detector's own gate. Noise riding on a sidelobe lifts it by no more
 *    than the noise's own magnitude, so a sidelobe passes only when that
 *    noise alone would have cleared the gate: the false-alarm rate the
 *    search already runs at. The bound (adr_sidelobe_bound()) is the
 *    strongest emitter's statistic, plus, on a deep block, every stronger
 *    emitter that can put a sidelobe on this peak's Doppler row.
 * 3. In an excluded zone: another receiver's signal. It is still an
 *    emitter, so it still counts in rule 2's bound.
 *
 * 1 and the hand-off in @p pick, or 0 when the list holds no peak at this
 * rank. */
/* Rule 2's bound for a peak at @p f_hz: what the stronger emitters'
 * sidelobes can reach there, as a statistic (doppler#2554).
 *
 * An emitter's Doppler-offset sidelobes land on its own Doppler row in
 * every other tile, f + k f_E: the slow-time transform resolves the same
 * phase advance per epoch in each tile (design acq-surface.md §8). On a
 * deep block (D > 1) over a code-only window those cells are fixed, so two
 * emitters on one row (modulo f_E) can stack their sidelobes on one cell
 * and the bound sums them. "One row" is the candidate's row and both its
 * neighbours: two emitters a few Hz apart quantize to ADJACENT rows, and
 * a cell their sidelobes stack on sits on one parent's row and a row from
 * the other (doppler#2556, review 5630995725), so half a row each side
 * dropped that parent. Summed over every emitter regardless of row, the
 * bound narrowed the near-far range with each emitter on the air. Off the row,
 * an emitter's sidelobe is down by the slow-time leakage as well, so the
 * strongest one bounds it.
 *
 * On a one-epoch search (D = 1) a native row is a whole tile, so rows
 * cannot be told apart, and the sum would read every emitter as stacked.
 * Measured there, two equal emitters on one carrier did not stack past the
 * gate under the strongest alone, while the sum left a true emitter 10 dB
 * below the strongest of six untaken (both on doppler#2554), so the
 * strongest alone bounds it. */
static double
adr_sidelobe_bound (const dp_async_dsss_receiver_state_t *s, double f_hz,
                    const double *stat, const double *freq, size_t n)
{
  double       strongest = 0.0, on_row = 0.0;
  const double fe    = s->acq->chip_rate / (double)s->code_len;
  const double reach = 1.5 * s->acq->doppler_res_hz;
  for (size_t j = 0; j < n; j++)
    {
      strongest = fmax (strongest, stat[j]);
      double d  = fmod (f_hz - freq[j], fe);
      d         = d < 0.0 ? d + fe : d;
      if (fmin (d, fe - d) <= reach)
        on_row += stat[j];
    }
  return s->acq->coherent_bins > 1 ? fmax (strongest, on_row) : strongest;
}

static int
adr_pick (const dp_async_dsss_receiver_state_t *s, const acq_result_t *hits,
          size_t n_hits, acq_handoff_t *pick)
{
  const double dsl  = pow (10.0, dp_acq_dsl_db (s->acq) / 20.0);
  const double gate = dp_acq_gate_stat (s->acq);
  double       seen[ASYNC_DSSS_RX_MAX_PEAKS];
  /* The emitters so far, strongest first: statistic and Doppler. */
  double em_stat[ASYNC_DSSS_RX_MAX_PEAKS], em_f[ASYNC_DSSS_RX_MAX_PEAKS];
  size_t n_seen = 0, n_kept = 0, n_em = 0;
  for (size_t i = 0; i < n_hits; i++)
    {
      acq_handoff_t ho;
      dp_acq_build_handoff (s->acq, &hits[i], s->code_len, s->spc, &ho);
      int drop = 0;
      for (size_t j = 0; j < n_seen && !drop; j++)
        drop = adr_same_code_phase (ho.chip_phase, seen[j], s->code_len);
      seen[n_seen++] = ho.chip_phase;
      if (drop
          || (double)hits[i].test_stat
                     - dsl
                           * adr_sidelobe_bound (s, ho.doppler_hz_est, em_stat,
                                                 em_f, n_em)
                 <= gate)
        continue;
      em_stat[n_em] = (double)hits[i].test_stat;
      em_f[n_em++]  = ho.doppler_hz_est;
      for (size_t j = 0; j < s->n_excl && !drop; j++)
        drop = adr_same_code_phase (ho.chip_phase, s->excl[j], s->code_len);
      if (drop)
        continue;
      if (n_kept++ == s->rank)
        {
          *pick = ho;
          return 1;
        }
    }
  return 0;
}

dp_async_dsss_receiver_state_t *
dp_async_dsss_receiver_create (
    const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate,
    size_t spc, int m, double cn0_dbhz, double pfa, double pd,
    double doppler_uncertainty, size_t segments, size_t sps, int differential,
    double refine_max_error_db, size_t refine_samples_per_symbol,
    double refine_design_margin_db, size_t refine_n_fft,
    size_t refine_zero_pad, bool refine_sequential, size_t refine_max_n_blocks,
    double carrier_freq_hz, double lost_confirm_s, bool reacquire,
    double code_only_s, double doppler_rate)
{
  /* symbol_rate becomes the carrier estimator's sample rate, which PSD
     refuses unless finite: refused here, at create, because that estimator
     is built under dp_xnn mid-stream (adr_new_carrier_acq), where a NULL
     aborts.  The negated form also refuses a NaN. */
  /* A carrier the code is coupled to is 0 (none) or above half the sample
     rate: the searcher's hand-off and both chains divide a Doppler by it,
     and a tiny one sent that to inf and a seed phase to NaN, which aborted
     under dp_xnn (doppler#2103). */
  if (!code || code_len < 1 || !(chip_rate > 0.0 && isfinite (chip_rate))
      || !(symbol_rate > 0.0 && isfinite (symbol_rate)) || spc < 1
      || (m != 2 && m != 4 && m != 8) || refine_samples_per_symbol < 1
      || refine_n_fft < 1 || refine_zero_pad < 1
      || !dp_acq_carrier_freq_ok (carrier_freq_hz, chip_rate * (double)spc)
      || !(lost_confirm_s >= 0.0))
    return NULL;
  /* Reacquire's release is its only way back to searching, so a release
     that never fires would leave it tracking a signal long gone: refused
     rather than accepted as a silent no-op. Checked in the clock's own
     unit, input samples, because a confirm time that rounds to zero of
     them (1e-9 s) IS the disabled clock (adr_release_clock returns at
     lost_confirm_samples == 0). */
  /* The release clock in input samples: the rule is a time, and the input
   * rate is the one clock every state of this object is fed at. Computed
   * once, here, refused on, and stored below. */
  const uint64_t lost_confirm_samples
      = (uint64_t)llround (lost_confirm_s * chip_rate * (double)spc);
  if (reacquire && lost_confirm_samples == 0)
    return NULL;
  /* The carrier estimator built mid-stream under dp_xnn
     (adr_new_carrier_acq) refuses a pfa or pd outside (0, 1), so they are
     refused here, at create (doppler#2103). */
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0))
    return NULL;
  /* The code-only window in whole epochs, design async-dsss-parallel.md §4:
     floor(W * chip_rate / sf) - 1 epochs fit at ANY chip phase, and under
     two (no window, or less than two epochs of one) is the epoch-by-epoch
     search. The engine refuses a bad doppler_rate itself; a window too
     long to count in epochs is refused here, before the cast. */
  if (!(code_only_s >= 0.0 && isfinite (code_only_s)))
    return NULL;
  const double window_epochs
      = floor (code_only_s * chip_rate / (double)code_len) - 1.0;
  if (!(window_epochs < 0x1p52))
    return NULL;
  const size_t code_only_epochs
      = window_epochs > 1.0 ? (size_t)window_epochs : 1;
  /* Each Dll this receiver builds -- the refine collector and the tracker,
     both created mid-stream under dp_xnn -- meets the Dll's own domain now,
     so a configuration the Dll refuses (a 1-chip code, whose early and late
     taps would coincide) is refused here rather than aborting the process
     mid-stream, and so is a symbol period its aid would refuse
     (adr_track_chain_ok(), doppler#2103). */
  {
    const size_t refine_segments
        = dp_dll_lookback_segments (code_len * spc, refine_max_error_db);
    if (!dp_dll_params_ok (code_len, 0.0, ASYNC_DSSS_RX_DLL_BN,
                           ASYNC_DSSS_RX_DLL_ZETA,
                           ASYNC_DSSS_RX_REFINE_DLL_SPACING, refine_segments)
        || !adr_track_chain_ok (code_len, chip_rate, symbol_rate, segments,
                                sps))
      return NULL;
  }

  dp_async_dsss_receiver_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  obj->tlm_sym_lock = obj->tlm_sym_locked = -1; /* detached */
  obj->tlm_state = obj->tlm_handover = obj->tlm_refused = -1;

  obj->code = dp_xmalloc (code_len);
  memcpy (obj->code, code, code_len);
  obj->code_len = code_len;

  /* The searcher checks its own domain (pfa, pd, C/N0 ...), so its refusal
     is this create's, not an abort under dp_xnn (doppler#2103). */
  obj->acq = dp_acq_create_continuous (
      obj->code, code_len, spc, chip_rate, symbol_rate, cn0_dbhz,
      doppler_uncertainty, pfa, pd, 0 /* noise_mode=mean */, code_only_epochs,
      doppler_rate);
  if (!obj->acq)
    {
      free (obj->code);
      free (obj);
      return NULL;
    }
  /* A physically-coupled carrier moves the code too: the searcher's
     hand-off advances its hit's phase by the drift over half its dwell
     (#1254), and a coherent block aligns its epochs (#1256). The same
     carrier the tracking Dll's aid uses. */
  if (carrier_freq_hz > 0.0)
    (void)dp_acq_set_carrier_freq_hz (obj->acq, carrier_freq_hz);
  /* A dwell's whole peak list, for adr_pick() to rank. Its first entry is
     the one-peak detector's pick, so rank 0 with no exclusions takes what
     max_peaks 1 took. */
  (void)dp_acq_set_max_peaks (obj->acq, ASYNC_DSSS_RX_MAX_PEAKS);
  /* adr_pick() reads the code's Doppler-offset sidelobe on every dwell, and
     the searcher computes it on the first read. Read it here, so that sweep
     is this create's cost and never a stall inside push(). */
  (void)dp_acq_dsl_db (obj->acq);

  obj->spc          = spc;
  obj->m            = m;
  obj->differential = differential;
  obj->chip_rate    = chip_rate;
  obj->symbol_rate  = symbol_rate;
  obj->cn0_dbhz     = cn0_dbhz;
  obj->pfa          = pfa;
  obj->pd           = pd;
  adr_enter (obj, ASYNC_DSSS_RX_SEARCHING);

  obj->lost_confirm_s       = lost_confirm_s;
  obj->reacquire            = reacquire;
  obj->lost_confirm_samples = lost_confirm_samples;
  obj->code_only_s          = code_only_s;
  obj->doppler_rate         = doppler_rate;
  /* Sample 0 is on every block boundary: the first dwell owes nothing. */
  obj->samples_in    = 0;
  obj->align_pending = false;

  obj->refine_max_error_db       = refine_max_error_db;
  obj->refine_samples_per_symbol = refine_samples_per_symbol;
  obj->refine_design_margin_db   = refine_design_margin_db;
  obj->refine_n_fft              = refine_n_fft;
  obj->refine_zero_pad           = refine_zero_pad;
  obj->refine_sequential         = refine_sequential;
  obj->refine_max_n_blocks       = refine_max_n_blocks;
  obj->refine_min_blocks         = ASYNC_DSSS_RX_REFINE_MIN_BLOCKS;
  obj->carrier_freq_hz           = carrier_freq_hz;

  /* tsamps (one code period, samples) is fixed for this object's entire
   * lifetime -- refine_segments is likewise fixed (depends only on
   * tsamps/refine_max_error_db, both construction-time invariants), so
   * both the shared carry scratch and refine_segments are computed ONCE
   * here, never per-rebuild. */
  obj->tsamps = code_len * spc;
  obj->refine_segments
      = dp_dll_lookback_segments (obj->tsamps, refine_max_error_db);
  obj->car_wiped_buf = dp_xmalloc (obj->tsamps * sizeof (*obj->car_wiped_buf));
  obj->car_carry_buf = dp_xmalloc (obj->tsamps * sizeof (*obj->car_carry_buf));
  obj->car_carry_len = 0;

  /* Placeholder chains (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (fixed shape, same rationale
   * dsss_receiver_core.c's own state struct doc comment gives). */
  adr_build_refine_chain (obj, 0.0, 0.0, &obj->car_frozen, &obj->refine_dll,
                          &obj->refine_rc, &obj->ca, &obj->refine_dll_out_buf,
                          &obj->refine_dll_out_cap, &obj->refine_rc_out_buf,
                          &obj->refine_rc_out_cap);
  obj->refine_samples_fed = 0;

  adr_build_track_chain (obj, 0.0, 0.0, segments, sps, adr_m_out (sps),
                         &obj->car, &obj->dll, &obj->rc, &obj->rx);
  obj->segments      = segments;
  obj->sps           = sps;
  obj->n             = adr_m_out (sps);
  obj->car_carry_len = 0; /* both placeholder builds share this buffer */
  adr_size_track_scratch (obj);

  obj->seed_chip_phase     = 0.0;
  obj->seed_doppler_hz_est = 0.0;
  obj->doppler_hz_est      = 0.0;
  obj->cn0_dbhz_est        = 0.0;

  /* Symbol-lock detector config (running state reset per track-chain build,
   * adr_reset_lock()). */
  obj->lock_alpha = 1.0 / (double)ASYNC_DSSS_RX_LOCK_DWELL;
  dp_lockdet_init (&obj->sym_lockdet, ASYNC_DSSS_RX_LOCK_UP,
                   ASYNC_DSSS_RX_LOCK_DOWN, ASYNC_DSSS_RX_LOCK_N_UP,
                   ASYNC_DSSS_RX_LOCK_N_DOWN);
  adr_reset_lock (obj);
  return obj;
}

void
dp_async_dsss_receiver_destroy (dp_async_dsss_receiver_state_t *state)
{
  if (!state)
    return;
  adr_free_track_chain (state);
  adr_free_refine_chain (state);
  free (state->track_dll_out_buf);
  free (state->track_rc_out_buf);
  free (state->car_wiped_buf);
  free (state->car_carry_buf);
  dp_acq_destroy (state->acq);
  free (state->code);
  free (state);
}

void
dp_async_dsss_receiver_reset (dp_async_dsss_receiver_state_t *state)
{
  adr_restart_search (state);
  /* Best-effort: on OOM, leave the current chains in place rather than
   * signal a failure this void-returning lifecycle function can't report
   * (matches dp_dsss_receiver_reset()'s own contract). */
  adr_rebuild_refine_chain (state, 0.0, 0.0);
  adr_rebuild_track_chain (state, 0.0, 0.0, state->segments, state->sps,
                           state->n);
  adr_enter (state, ASYNC_DSSS_RX_SEARCHING);
  state->seed_chip_phase     = 0.0;
  state->seed_doppler_hz_est = 0.0;
  state->doppler_hz_est      = 0.0;
  state->cn0_dbhz_est        = 0.0;
  state->refused_hits        = 0;
}

int
dp_async_dsss_receiver_set_telemetry (dp_async_dsss_receiver_state_t *state,
                                      dp_tlm_t *tlm, const char *prefix,
                                      uint32_t decim)
{
  if (!tlm)
    {
      adr_tlm_detach (state);
      state->tlm = NULL;
      return DP_OK;
    }
  const char  *p   = prefix ? prefix : "rx";
  const size_t len = strlen (p);
  if (decim == 0 || len >= DP_TLM_NAME_MAX)
    return DP_ERR_INVALID;
  /* A refusal leaves the receiver as it was, as a leaf's does: the dry run
     below moves every loop off whatever context it was on, so keep the
     request it replaces and put it back on any refusal. */
  dp_tlm_t *const old_tlm   = state->tlm;
  const uint32_t  old_decim = state->tlm_decim;
  char            old_prefix[DP_TLM_NAME_MAX];
  memcpy (old_prefix, state->tlm_prefix, sizeof old_prefix);
  memcpy (state->tlm_prefix, p, len + 1);
  state->tlm_decim = decim;
  /* Every name must fit before ANY registers on the caller's context. The
     loops join their own names, so the one test that cannot drift from what
     they will register is to let them: attach to a scratch context first,
     and to the caller's only if that took every probe. Without it a prefix
     that fits the carrier's names and not the demodulator's leaves the
     carrier's in the caller's table, unused (#1897). The scratch then
     knows every name the attach needs, so it also answers whether the
     caller's table has room for the ones it lacks. */
  dp_tlm_t *scratch = dp_tlm_create (1);
  int       fits    = 0;
  if (scratch)
    {
      state->tlm = scratch;
      fits       = adr_tlm_attach (state) == DP_OK
                   && dp_tlm_has_room_for (tlm, scratch);
      adr_tlm_detach (state);
      dp_tlm_destroy (scratch);
    }
  if (fits)
    {
      state->tlm = tlm;
      if (adr_tlm_attach (state) == DP_OK)
        return DP_OK;
      /* Whole or nothing: a half-attached receiver would report some loops
         and not others with no way to tell from the context. */
      adr_tlm_detach (state);
    }
  /* Refused: restore the previous request. Its names are already in its
     context, so re-registering them takes no slot and cannot fail. */
  memcpy (state->tlm_prefix, old_prefix, sizeof old_prefix);
  state->tlm_decim = old_decim;
  state->tlm       = old_tlm;
  if (old_tlm)
    (void)adr_tlm_attach (state);
  return DP_ERR_INVALID;
}

size_t
dp_async_dsss_receiver_steps_max_out (dp_async_dsss_receiver_state_t *state)
{
  (void)state;
  return 0; /* caller falls back to allocating x_len -- always sufficient,
               symbols emitted <= raw samples in. */
}

int
dp_async_dsss_receiver_seed_ok (const dp_async_dsss_receiver_state_t *state,
                                double chip_phase, double doppler_hz_est)
{
  /* A Doppler is reportable only below half the sample rate, which is what
     dp_acq_carrier_freq_ok() relies on to keep doppler / carrier finite; a
     seed near 1e308 sent the hand-off's phase to NaN (doppler#2103). The
     comparisons are written so a NaN fails them. */
  return isfinite (chip_phase) && chip_phase >= 0.0
         && chip_phase < (double)state->code_len
         && fabs (doppler_hz_est)
                < 0.5 * state->chip_rate * (double)state->spc;
}

int
dp_async_dsss_receiver_seed (dp_async_dsss_receiver_state_t *state,
                             double chip_phase, double doppler_hz_est,
                             double cn0_dbhz_est)
{
  /* Only a receiver with nothing to lose takes a seed: a searching one.
   * Lost is deliberately refused too -- reset() releases it, so a stale
   * hold is never silently overwritten. */
  if (state->state != ASYNC_DSSS_RX_SEARCHING)
    return DP_ERR_INVALID;
  if (!dp_async_dsss_receiver_seed_ok (state, chip_phase, doppler_hz_est))
    return DP_ERR_INVALID;

  adr_rebuild_refine_chain (state, chip_phase, doppler_hz_est);

  adr_enter (state, ASYNC_DSSS_RX_REFINING);
  state->seed_chip_phase     = chip_phase;
  state->seed_doppler_hz_est = doppler_hz_est;
  state->doppler_hz_est      = doppler_hz_est;
  state->cn0_dbhz_est        = cn0_dbhz_est;
  return DP_OK;
}

/* The release rule of async-dsss-receiver.md section 10, run once per
 * tracking call: both lock flags down, without a break, for longer than
 * the confirm interval means the emitter is gone. One flag down is a
 * degrade and leaves the clock alone; either flag up restarts it.
 *
 * With reacquire the rule is the parallel design's (async-dsss-parallel.md
 * section 3): symbol lock alone, and the release resets the receiver to
 * searching instead of parking it in lost, so a receiver that is always
 * online finds the next signal without anyone calling reset(). */
static void
adr_release_clock (dp_async_dsss_receiver_state_t *s, size_t x_len)
{
  if (s->lost_confirm_samples == 0)
    return;
  const bool up = s->reacquire
                      ? s->sym_lockdet.locked
                      : (dp_dll_get_locked (s->dll) || s->sym_lockdet.locked);
  if (up)
    {
      s->both_down_samples = 0;
      return;
    }
  s->both_down_samples += x_len;
  if (s->both_down_samples <= s->lost_confirm_samples)
    return;
  if (s->reacquire)
    dp_async_dsss_receiver_reset (s);
  else
    adr_enter (s, ASYNC_DSSS_RX_LOST);
}

/* steps()'s body. A hit, a refused hand-over or a decided refine hands the
 * rest of the call to itself here, so a call may pass through several
 * states; the public steps() below reports once, after all of them. */
static size_t
adr_steps (dp_async_dsss_receiver_state_t *state, const float _Complex *x,
           size_t x_len, float _Complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;
  state->state_samples += x_len;

  if (state->state == ASYNC_DSSS_RX_LOST)
    state->both_down_samples += x_len; /* since the flags dropped */
  if (state->state == ASYNC_DSSS_RX_LOST)
    return 0; /* consumed and discarded: the feeding loop has no case */

  if (state->state == ASYNC_DSSS_RX_SEARCHING)
    {
      /* A restarted search first runs out to the block grid. */
      if (state->align_pending)
        {
          const size_t skip = adr_align_skip (state, x_len);
          x += skip;
          x_len -= skip;
          if (x_len == 0)
            return 0;
        }
      /* The engine's own position and the hit's, one timebase, and the
       * same fail-closed bound -- the rule and why are at
       * dsss_receiver_core.c's steps(), which hands off the same way. */
      const uint64_t before = dp_acq_position (state->acq);
      acq_result_t   hits[ASYNC_DSSS_RX_MAX_PEAKS];
      size_t         n_hits
          = dp_acq_push (state->acq, x, x_len, hits, ASYNC_DSSS_RX_MAX_PEAKS);
      if (n_hits == 0)
        return 0;
      /* With room for a whole dwell's list the push reports ONE dwell, every
         peak sharing its samples_consumed, and may frame on past it to the
         next dwell boundary. The hand-off resumes at the dwell's end; a
         search that goes on resumes where the ENGINE stopped,
         dp_acq_consumed(), so no sample is pushed twice. */
      const uint64_t at = hits[0].samples_consumed - before;
      if (at == 0 || at > (uint64_t)x_len)
        return 0;
      const float _Complex *tail     = x + at;
      const size_t          tail_len = x_len - (size_t)at;
      const size_t          taken    = dp_acq_consumed (state->acq);
      const float _Complex *rest     = x + taken;
      const size_t          rest_len = x_len - taken;

      /* A hit is a seed the object made for itself -- the same path an
       * outside detection takes. The list is filtered first (adr_pick():
       * exclusions, duplicates, rank); a dwell with no peak at this
       * receiver's rank seeds nothing and the search goes on.
       * dp_acq_build_handoff() folds the phase into [0, code_len), but not
       * the Doppler: a hit at or past fs/2 is outside the seed domain. Such
       * a hit is refused as a seed and counted in refused_hits, and the
       * search goes on, as it would after no hit. Either way this call
       * already counted all of x, and the recursion counts the rest again:
       * take the rest back out, so each input sample is counted once. (The
       * accepted branch needs no such step: seed() restarts the count on
       * entry to the refining state.) */
      acq_handoff_t ho;
      if (!adr_pick (state, hits, n_hits, &ho))
        {
          state->state_samples -= rest_len;
          return adr_steps (state, rest, rest_len, out, max_out);
        }
      if (!dp_async_dsss_receiver_seed_ok (state, ho.chip_phase,
                                           ho.doppler_hz_est))
        {
          state->refused_hits++;
          state->state_samples -= rest_len;
          return adr_steps (state, rest, rest_len, out, max_out);
        }
      /* Accepted: the domain was just checked, and the receiver is searching,
         so the seed cannot refuse for either reason. */
      (void)dp_async_dsss_receiver_seed (state, ho.chip_phase,
                                         ho.doppler_hz_est, ho.cn0_dbhz_est);
      return adr_steps (state, tail, tail_len, out, max_out);
    }

  if (state->state == ASYNC_DSSS_RX_REFINING)
    {
      uint64_t before = state->refine_samples_fed;
      state->refine_samples_fed += x_len;
      adr_process_refine (state, x, x_len);

      dp_carrier_acq_state_t *ca = state->ca;
      if (!adr_refine_decided (state))
        return 0; /* still refining, no output yet */

      double refined_doppler_hz_est
          = state->seed_doppler_hz_est
            + (ca->ready ? ca->residual_hz : 0.0); /* give-up: unrefined */
      /* samples_consumed_refine: freq_refine.refine_seed_carrier_acq()'s
       * own elapsed-time formula, ported verbatim -- a resampled chain
       * has no exact raw-sample-to-block correspondence to track through
       * RateConverter's own filter delay; this approximation is already
       * validated to ~tens-of-Hz accuracy at SPEC's own real rate (see
       * the plan doc). Uses `refine_n_fft` (the RAW per-block sample
       * count `dp_carrier_acq_create()` consumes per `n_blocks` increment)
       * -- NOT `ca->nfft` (the zero-padded PSD *transform* length,
       * `next_pow_two(refine_n_fft*refine_zero_pad)`, a different and much
       * larger number). */
      double target_rate
          = (double)state->refine_samples_per_symbol * state->symbol_rate;
      double elapsed_s
          = (double)ca->n_blocks * (double)state->refine_n_fft / target_rate;
      double   front_end_rate = state->chip_rate * (double)state->spc;
      uint64_t samples_consumed_refine
          = (uint64_t)llround (elapsed_s * front_end_rate);

      /* Round UP to a whole number of code periods (tsamps) -- critical,
       * not cosmetic: the live tracking chain below is seeded from
       * `seed_chip_phase`, the code phase at the ORIGINAL handoff (advanced
       * by the clock dilation over the refine, below), not wherever the
       * refine-stage Dll's own tracking drifted to. That reuse is only
       * valid if the live chain's first sample is an EXACT whole number of
       * code periods after the handoff -- one whole period is zero net
       * code-phase advance on the nominal clock, so the phase at any such
       * boundary is the handoff's plus the dilation alone. Python's own
       * e2e_acq_to_despreader.py relies on exactly
       * this (`n_epochs_used`'s own ceiling-to-whole-epoch rounding
       * before slicing `track_rx`); omitting it here was a real bug --
       * confirmed directly: without this rounding, the live chain is
       * seeded with a code phase valid for a DIFFERENT (non-epoch-
       * aligned) stream position than the one it's actually fed,
       * misaligning Dll's correlation from the very first sample
       * (total decode failure, reproduced even at Es/N0=30dB where
       * noise cannot be the explanation). */
      uint64_t n_periods_consumed
          = (samples_consumed_refine + (uint64_t)state->tsamps - 1)
            / (uint64_t)state->tsamps;
      samples_consumed_refine = n_periods_consumed * (uint64_t)state->tsamps;

      /* Only a tail within THIS call's own x can be safely recovered --
       * a prior call's buffer is already gone (the same rule
       * dsss_receiver_core.c's own search->track transition follows).
       * If the estimate implies more was unused than this call
       * provided, there's nothing left to recover; start tracking from
       * zero backlog rather than guess. */
      uint64_t fed_total = before + (uint64_t)x_len;
      uint64_t unused    = (fed_total > samples_consumed_refine)
                               ? (fed_total - samples_consumed_refine)
                               : 0;
      size_t   tail_len  = (unused > (uint64_t)x_len) ? 0 : (size_t)unused;
      const float _Complex *tail = x + (x_len - tail_len);

      /* The code phase the live chain starts from: the seed's, ADVANCED by
       * the clock dilation over the refine. A whole number of periods is
       * zero net advance only on an undilated clock; at 20 ppm the code
       * runs 100 chips/s ahead, 1.2 chips over a 12 ms refine and 5 over
       * 53 ms, and a live Dll seeded 1.5 chips off is outside its pull-in
       * -- nothing downstream locked (doppler#1249). The refine-stage
       * Dll's own tracked phase is NOT the answer: at the floor it wanders
       * by 13 chips over the same 53 ms (measured), where the dilation
       * model from the refined Doppler is within a tenth of a chip. Off
       * when carrier_freq_hz == 0: no carrier, no dilation to model. */
      /* The live chain's Dll is built under dp_xnn, which aborts on a
         non-finite init phase or one outside [0, code_len): a refined value
         failing that takes the failed-refine path, the seed's own Doppler
         and phase (doppler#2103). What survives is then held to the seed
         domain, the same test a hit is (#2461 #122): a refined Doppler at or
         past fs/2 is no Doppler this receiver can report, so the hand-over
         is refused like a hit -- counted in refused_hits, the search reset,
         and the rest of this call searched. */
      double handover_chip_phase = adr_handover_phase (
          state, refined_doppler_hz_est, samples_consumed_refine);
      if (!isfinite (refined_doppler_hz_est) || !isfinite (handover_chip_phase)
          || handover_chip_phase < 0.0
          || handover_chip_phase >= (double)state->code_len)
        {
          refined_doppler_hz_est = state->seed_doppler_hz_est;
          handover_chip_phase    = adr_handover_phase (
              state, refined_doppler_hz_est, samples_consumed_refine);
        }
      if (!dp_async_dsss_receiver_seed_ok (state, handover_chip_phase,
                                           refined_doppler_hz_est))
        {
          /* Refused, as a hit outside the domain is. Not reset(): that
             would zero the refusal count this increments. The tail is the
             search's; entering searching restarts state_samples, so the
             recursion counts it once. */
          state->refused_hits++;
          /* The refused Doppler is not left readable: searching reads the
             estimates as reset() leaves them. */
          state->seed_doppler_hz_est = 0.0;
          state->doppler_hz_est      = 0.0;
          adr_restart_search (state);
          adr_enter (state, ASYNC_DSSS_RX_SEARCHING);
          return adr_steps (state, tail, tail_len, out, max_out);
        }
      adr_rebuild_track_chain (state, handover_chip_phase,
                               refined_doppler_hz_est, state->segments,
                               state->sps, state->n);

      adr_enter (state, ASYNC_DSSS_RX_TRACKING);
      state->doppler_hz_est = refined_doppler_hz_est;

      return adr_steps (state, tail, tail_len, out, max_out);
    }

  size_t n_out = adr_track_chain (state, x, x_len, out, max_out);
  adr_release_clock (state, x_len);
  return n_out;
}

size_t
dp_async_dsss_receiver_steps (dp_async_dsss_receiver_state_t *state,
                              const float _Complex *x, size_t x_len,
                              float _Complex *out, size_t max_out)
{
  /* Counted up front: every tail adr_steps() recurses on ends where this
     call's input ends, so a tail of t samples starts at samples_in - t. */
  state->samples_in += x_len;
  size_t n_out = adr_steps (state, x, x_len, out, max_out);
  /* The lifecycle, once a call and after it, on the block's stamp: where
     the receiver ended up, the Doppler its chain was built from, and the
     hits refused so far. An empty call changed nothing and reports nothing,
     which also keeps every probe within one record per input sample. */
  if (state->tlm && x_len)
    {
      dp_tlm_emit (state->tlm, state->tlm_state, (double)state->state);
      dp_tlm_emit (state->tlm, state->tlm_handover, state->doppler_hz_est);
      dp_tlm_emit (state->tlm, state->tlm_refused,
                   (double)state->refused_hits);
    }
  return n_out;
}

int
dp_async_dsss_receiver_configure_search_raw (
    dp_async_dsss_receiver_state_t *state, size_t doppler_bins,
    size_t n_noncoh)
{
  const int rc
      = dp_acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
  /* An accepted search config drains the engine (dp_acq_reset()), so its
     next dwell starts on the block grid like any restarted search's. */
  if (rc == 0)
    state->align_pending = true;
  return rc;
}

int
dp_async_dsss_receiver_set_threads (dp_async_dsss_receiver_state_t *state,
                                    int                             n)
{
  return dp_acq_set_threads (state->acq, n);
}

int
dp_async_dsss_receiver_set_rank (dp_async_dsss_receiver_state_t *state,
                                 size_t                          rank)
{
  if (rank >= ASYNC_DSSS_RX_MAX_PEAKS)
    return DP_ERR_INVALID;
  state->rank = rank;
  return DP_OK;
}

size_t
dp_async_dsss_receiver_get_rank (const dp_async_dsss_receiver_state_t *state)
{
  return state->rank;
}

int
dp_async_dsss_receiver_set_exclusions (dp_async_dsss_receiver_state_t *state,
                                       const double *phases, size_t phases_len)
{
  if (phases_len > ASYNC_DSSS_RX_MAX_EXCLUSIONS || (phases_len && !phases))
    return DP_ERR_INVALID;
  for (size_t i = 0; i < phases_len; i++)
    if (!isfinite (phases[i]))
      return DP_ERR_INVALID;
  /* Checked whole, then committed: a refusal leaves the list as it was. */
  const double cl = (double)state->code_len;
  for (size_t i = 0; i < phases_len; i++)
    state->excl[i] = dp_fmod_pos (phases[i], cl);
  for (size_t i = phases_len; i < ASYNC_DSSS_RX_MAX_EXCLUSIONS; i++)
    state->excl[i] = 0.0;
  state->n_excl = phases_len;
  return DP_OK;
}

size_t
dp_async_dsss_receiver_get_n_exclusions (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->n_excl;
}

int
dp_async_dsss_receiver_get_threads (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->acq->threads;
}

size_t
dp_async_dsss_receiver_get_coherent_bins (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->acq->coherent_bins;
}

int
dp_async_dsss_receiver_set_refine_min_blocks (
    dp_async_dsss_receiver_state_t *state, size_t n_blocks)
{
  state->refine_min_blocks = n_blocks;
  return DP_OK;
}

void
dp_async_dsss_receiver_configure_lock_raw (
    dp_async_dsss_receiver_state_t *state, double up_thresh,
    double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
    uint32_t n_down)
{
  dp_dll_configure_lock_raw (state->dll, up_thresh, down_thresh, n_looks,
                             alpha, n_up, n_down);
}

/* The whole carrier the live chain holds, Hz: loop 1's (the pre-despread
 * Costas, in cycles per front-end sample) plus what loop 2 -- the
 * post-despread MpskReceiver, at sps * symbol_rate -- has taken up beyond
 * it, its loop-filter integrator. status() reports it and
 * configure_chain_raw() re-seeds a rebuilt chain from it (doppler#1261,
 * #2414): one sum, so the two cannot disagree. */
static double
adr_tracked_doppler_hz (const dp_async_dsss_receiver_state_t *s)
{
  return dp_costas_get_norm_freq (&s->car) * s->chip_rate * (double)s->spc
         + dp_mpsk_receiver_get_norm_freq (s->rx)
               * ((double)s->sps * s->symbol_rate);
}

int
dp_async_dsss_receiver_configure_chain_raw (
    dp_async_dsss_receiver_state_t *state, size_t segments, size_t sps, int n)
{
  /* The new grid's chain is checked as create() checked the first, before
     anything is rebuilt: a period past what the Dll's aid takes, or an `n`
     MpskReceiver refuses, would abort the rebuild under dp_xnn (doppler#2103,
     #2421). A negative `n` is refused before the cast. */
  if (n < 0 || !dp_ratesync_m_ok ((size_t)n)
      || !adr_track_chain_ok (state->code_len, state->chip_rate,
                              state->symbol_rate, segments, sps)
      || (int)(sps % (size_t)n) != 0)
    return -1;

  /* Read before the rebuild, while sps is still the old grid's: the sum
     is loop 1 PLUS loop 2. Loop 2's residual alone (what this read before
     #2414) dropped loop 1's carrier, so a receiver tracking +2.4 kHz was
     rebuilt near 0 Hz and lost the signal. */
  double chip_phase     = dp_dll_get_code_phase (state->dll);
  double doppler_hz_now = adr_tracked_doppler_hz (state);

  adr_rebuild_track_chain (state, chip_phase, doppler_hz_now, segments, sps,
                           n);
  return 0;
}

int
dp_async_dsss_receiver_get_tracking (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_TRACKING;
}
int
dp_async_dsss_receiver_get_refining (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_REFINING;
}
async_dsss_receiver_status_t
dp_async_dsss_receiver_status (const dp_async_dsss_receiver_state_t *s)
{
  /* Where the emitter is NOW. Once tracking (and held where it was in
   * lost) the estimate is the WHOLE carrier: loop 1's, in cycles per
   * front-end sample, plus what loop 2 -- the post-despread MpskReceiver
   * loop, at sps * symbol_rate -- has taken up beyond it, its loop-filter
   * integrator, the same sum configure_chain_raw() re-seeds a rebuilt
   * chain from (doppler#1261). While refining the frozen carrier IS the
   * seed; searching has no emitter. */
  double fs = s->chip_rate * (double)s->spc;
  double doppler_hz;
  switch (s->state)
    {
    case ASYNC_DSSS_RX_TRACKING:
    case ASYNC_DSSS_RX_LOST:
      doppler_hz = adr_tracked_doppler_hz (s);
      break;
    case ASYNC_DSSS_RX_REFINING:
      doppler_hz = dp_costas_get_norm_freq (&s->car_frozen) * fs;
      break;
    default:
      doppler_hz = 0.0;
    }
  async_dsss_receiver_status_t r = {
    .state             = s->state,
    .doppler_hz        = doppler_hz,
    .chip_phase        = dp_async_dsss_receiver_get_chip_phase (s),
    .code_rate         = dp_async_dsss_receiver_get_code_rate (s),
    .cn0_dbhz_est      = s->cn0_dbhz_est,
    .code_locked       = dp_async_dsss_receiver_get_code_locked (s),
    .locked            = dp_async_dsss_receiver_get_locked (s),
    .lock_metric       = s->lock_metric,
    .lock_threshold    = dp_async_dsss_receiver_get_lock_threshold (s),
    .car_last_error    = dp_async_dsss_receiver_get_car_last_error (s),
    .mpsk_last_error   = dp_async_dsss_receiver_get_mpsk_last_error (s),
    .state_samples     = s->state_samples,
    .both_down_samples = s->both_down_samples,
    .refused_hits      = s->refused_hits,
  };
  return r;
}

int
dp_async_dsss_receiver_get_lost (const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_LOST;
}
double
dp_async_dsss_receiver_get_doppler_hz (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->doppler_hz_est;
}
double
dp_async_dsss_receiver_get_cn0_dbhz_est (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}
size_t
dp_async_dsss_receiver_get_segments (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->segments;
}
size_t
dp_async_dsss_receiver_get_sps (const dp_async_dsss_receiver_state_t *state)
{
  return state->sps;
}
int
dp_async_dsss_receiver_get_n (const dp_async_dsss_receiver_state_t *state)
{
  return state->n;
}
double
dp_async_dsss_receiver_get_chip_phase (
    const dp_async_dsss_receiver_state_t *state)
{
  /* Refining has no live Dll yet: the signal it holds is the seed's, at the
     seed's phase advanced by the clock dilation since -- the one formula
     the hand-over starts the live chain from. A controller excludes this
     phase, so it must name the signal, not read 0 (#2525). */
  if (state->state == ASYNC_DSSS_RX_REFINING)
    return adr_handover_phase (state, state->seed_doppler_hz_est,
                               state->refine_samples_fed);
  return dp_dll_get_code_phase (state->dll);
}
double
dp_async_dsss_receiver_get_code_rate (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_dll_get_code_rate (state->dll);
}
double
dp_async_dsss_receiver_get_lock (const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_lock (state->rx);
}
double
dp_async_dsss_receiver_get_norm_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_norm_freq (state->rx);
}

double
dp_async_dsss_receiver_get_nco_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_nco_freq (state->rx);
}

int
dp_async_dsss_receiver_get_locked (const dp_async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.locked;
}

double
dp_async_dsss_receiver_get_lock_metric (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->lock_metric;
}

double
dp_async_dsss_receiver_get_lock_threshold (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.up_thresh;
}

int
dp_async_dsss_receiver_get_code_locked (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_dll_get_locked (state->dll);
}

double
dp_async_dsss_receiver_get_car_last_error (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_costas_get_last_error (&state->car);
}

double
dp_async_dsss_receiver_get_car_nco_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_costas_get_nco_freq (&state->car);
}

double
dp_async_dsss_receiver_get_mpsk_last_error (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_last_error (state->rx);
}

size_t
dp_async_dsss_receiver_state_bytes (const dp_async_dsss_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (async_dsss_receiver_extra_t)
         + dp_acq_state_bytes (s->acq) + dp_costas_state_bytes (&s->car_frozen)
         + dp_dll_state_bytes (s->refine_dll)
         + dp_RateConverter_state_bytes (s->refine_rc)
         + dp_carrier_acq_state_bytes (s->ca) + dp_costas_state_bytes (&s->car)
         + dp_costas_state_bytes (&s->car_held) + dp_dll_state_bytes (s->dll)
         + dp_RateConverter_state_bytes (s->rc)
         + dp_mpsk_receiver_state_bytes (s->rx)
         + s->tsamps * sizeof (float _Complex);
}

void
dp_async_dsss_receiver_get_state (const dp_async_dsss_receiver_state_t *s,
                                  void                                 *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               dp_async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra = {
    .state               = (uint8_t)s->state,
    .seed_chip_phase     = s->seed_chip_phase,
    .seed_doppler_hz_est = s->seed_doppler_hz_est,
    .doppler_hz_est      = s->doppler_hz_est,
    .cn0_dbhz_est        = s->cn0_dbhz_est,
    .segments            = (uint64_t)s->segments,
    .sps                 = (uint64_t)s->sps,
    .n                   = (uint64_t)s->n,
    .refine_segments     = (uint64_t)s->refine_segments,
    .refine_samples_fed  = s->refine_samples_fed,
    .car_carry_len       = (uint64_t)s->car_carry_len,
    .state_samples       = s->state_samples,
    .both_down_samples   = s->both_down_samples,
    .had_lock            = (uint8_t)(s->had_lock != 0),
    .car_coasting        = (uint8_t)(s->car_coasting != 0),
    .lock_num            = s->lock_num,
    .lock_den            = s->lock_den,
    .lock_metric         = s->lock_metric,
    .sym_lockdet         = s->sym_lockdet,
    .refused_hits        = s->refused_hits,
    .refine_min_blocks   = (uint64_t)s->refine_min_blocks,
    .samples_in          = s->samples_in,
    .align_pending       = (uint64_t)(s->align_pending != 0),
    .threads             = (int64_t)s->acq->threads,
    .rank                = (uint64_t)s->rank,
    .n_excl              = (uint64_t)s->n_excl,
  };
  memcpy (extra.excl, s->excl, sizeof extra.excl); /* tail kept zeroed */
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, dp_acq, s->acq);
  DP_W_CHILD (&_w, dp_costas, &s->car_frozen);
  DP_W_CHILD (&_w, dp_dll, s->refine_dll);
  DP_W_CHILD (&_w, dp_RateConverter, s->refine_rc);
  DP_W_CHILD (&_w, dp_carrier_acq, s->ca);
  DP_W_CHILD (&_w, dp_costas, &s->car);
  DP_W_CHILD (&_w, dp_costas, &s->car_held);
  DP_W_CHILD (&_w, dp_dll, s->dll);
  DP_W_CHILD (&_w, dp_RateConverter, s->rc);
  DP_W_CHILD (&_w, dp_mpsk_receiver, s->rx);
  /* The carry's live samples, then zeros: the rest of the buffer is an
     earlier period's or dp_xmalloc's, not state (#2076's sibling). */
  dp_w_cf32 (&_w, s->car_carry_buf, s->car_carry_len);
  dp_w_zeros (&_w, (s->tsamps - s->car_carry_len) * sizeof (float _Complex));
}

int
dp_async_dsss_receiver_set_state (dp_async_dsss_receiver_state_t *s,
                                  const void                     *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               dp_async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n
      || extra.refine_segments != (uint64_t)s->refine_segments
      || extra.car_carry_len >= (uint64_t)s->tsamps
      || extra.state > ASYNC_DSSS_RX_LOST
      || extra.state == ASYNC_DSSS_RX_RESERVED || extra.align_pending > 1
      || extra.threads < 1 || extra.threads > INT_MAX
      || extra.rank >= ASYNC_DSSS_RX_MAX_PEAKS
      || extra.n_excl > ASYNC_DSSS_RX_MAX_EXCLUSIONS)
    return DP_ERR_INVALID;
  /* Only what set_exclusions() can leave: folded phases, a zeroed tail. */
  for (size_t i = 0; i < ASYNC_DSSS_RX_MAX_EXCLUSIONS; i++)
    if (i < extra.n_excl
            ? !(extra.excl[i] >= 0.0 && extra.excl[i] < (double)s->code_len)
            : extra.excl[i] != 0.0)
      return DP_ERR_INVALID;
  /* A reacquire receiver never enters lost (its release resets it to
     searching), so lost is no state it can be in, and nothing would take
     it out: refused, as a state the object cannot reach. */
  if (s->reacquire && extra.state == ASYNC_DSSS_RX_LOST)
    return DP_ERR_INVALID;
  /* The seed fields are only ever what seed() accepted (or the zeros reset
     leaves), so a blob outside the seed domain is forged. Refused here,
     before any child is decoded: a NaN phase reached dp_xnn in the refine
     chain's Dll create (doppler#2103, through set_state). */
  if (!dp_async_dsss_receiver_seed_ok (s, extra.seed_chip_phase,
                                       extra.seed_doppler_hz_est))
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, dp_acq, s->acq);
  DP_R_CHILD (&_r, dp_costas, &s->car_frozen);
  DP_R_CHILD (&_r, dp_dll, s->refine_dll);
  DP_R_CHILD (&_r, dp_RateConverter, s->refine_rc);
  DP_R_CHILD (&_r, dp_carrier_acq, s->ca);
  DP_R_CHILD (&_r, dp_costas, &s->car);
  DP_R_CHILD (&_r, dp_costas, &s->car_held);
  DP_R_CHILD (&_r, dp_dll, s->dll);
  DP_R_CHILD (&_r, dp_RateConverter, s->rc);
  DP_R_CHILD (&_r, dp_mpsk_receiver, s->rx);
  dp_r_cf32 (&_r, s->car_carry_buf, s->tsamps);
  s->state               = extra.state;
  s->seed_chip_phase     = extra.seed_chip_phase;
  s->seed_doppler_hz_est = extra.seed_doppler_hz_est;
  s->doppler_hz_est      = extra.doppler_hz_est;
  s->cn0_dbhz_est        = extra.cn0_dbhz_est;
  s->refine_samples_fed  = extra.refine_samples_fed;
  s->refine_periods      = 0; /* a diagnostic, not in the blob: restarts */
  s->car_carry_len       = (size_t)extra.car_carry_len;
  s->state_samples       = extra.state_samples;
  s->both_down_samples   = extra.both_down_samples;
  s->had_lock            = extra.had_lock;
  s->car_coasting        = extra.car_coasting;
  s->lock_num            = extra.lock_num;
  s->lock_den            = extra.lock_den;
  s->lock_metric         = extra.lock_metric;
  s->sym_lockdet         = extra.sym_lockdet;
  s->refused_hits        = extra.refused_hits;
  s->refine_min_blocks   = (size_t)extra.refine_min_blocks;
  s->samples_in          = extra.samples_in;
  s->rank                = (size_t)extra.rank;
  s->n_excl              = (size_t)extra.n_excl;
  memcpy (s->excl, extra.excl, sizeof s->excl);
  s->align_pending = extra.align_pending != 0;
  /* set_threads()'s value (#2022): the same request the writer's last
     call made, which on a machine with as many cores runs as many. */
  if (extra.threads != (int64_t)s->acq->threads)
    (void)dp_acq_set_threads (s->acq, (int)extra.threads);
  return DP_OK;
}
