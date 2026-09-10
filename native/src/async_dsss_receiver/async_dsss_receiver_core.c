#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "detection/detection_core.h"
#include "util/util_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Defined with the cell mode's pull-in below; the refine chain's builder
   above it uses it too (one estimator, two chains). */
static carrier_acq_state_t *
adr_new_carrier_acq (const async_dsss_receiver_state_t *s, double target_rate);

/* MpskReceiver's terminal outputs per symbol (`m_out`), mirroring
 * dsss_receiver_core.c's own adr_derive_m_out() -- see the full rationale
 * there. Short version: this slot used to hold the retired `n` (the NDA
 * arm's dumps per symbol, so dividing sps exactly was the point) and the old
 * divisor rule survived the rename into a parameter that means something
 * else. It must be EVEN in [2, 8] and need not divide sps, so prefer the
 * coherent-bound default and step down only as far as sps allows. */
static int
adr_derive_m_out (size_t sps)
{
  int m = MPSK_RX_M_OUT_DEFAULT;
  while (m > 2 && (size_t)m > sps)
    m -= 2;
  return m;
}

/* Reset the symbol-lock detector's RUNNING state (config -- alpha, lockdet
 * thresholds/counts -- is set once in create()). Called on every fresh
 * track-chain build so each pass starts unlocked. */
static void
adr_reset_lock (async_dsss_receiver_state_t *s)
{
  s->lock_num    = 0.0;
  s->lock_den    = 0.0;
  s->lock_metric = 0.0;
  lockdet_reset (&s->sym_lockdet);
}

/* Every state transition goes through here so the two running clocks stay
 * honest: `state_samples` counts from the entry, and the release clock
 * (`both_down_samples`) only ever runs inside tracking. */
static void
adr_enter (async_dsss_receiver_state_t *s, int state)
{
  s->state         = state;
  s->state_samples = 0;
  /* Lost KEEPS the release clock and goes on counting it (section 11.3:
   * "in lost, since the code flag dropped"); every other entry restarts
   * it. */
  if (state != ASYNC_DSSS_RX_LOST)
    s->both_down_samples = 0;
}

/* Allocate a fresh refine-stage chain (frozen carrier + collection Dll +
 * RateConverter + CarrierAcquisition, plus their scratch buffers) from
 * the given hand-off phase/frequency, without touching `s`'s existing
 * children -- the fail-safe half of the "allocate everything first"
 * regrid discipline (dsss_receiver_core.c's own dsss_rx_build_chain is the
 * precedent). `s->refine_segments` is assumed already set (fixed for the
 * object's lifetime -- see async_dsss_receiver_create()). The sub-object
 * allocations are small and internal, so they are trusted (the house
 * "no error handling for impossible scenarios" rule) -- this build cannot
 * fail. */
static void
adr_build_refine_chain (
    async_dsss_receiver_state_t *s, double chip_phase, double doppler_hz_est,
    costas_state_t *car_frozen_out, dll_state_t **refine_dll_out,
    RateConverter_state_t **refine_rc_out, carrier_acq_state_t **ca_out,
    float _Complex **dll_out_buf_out, size_t *dll_out_cap_out,
    float _Complex **rc_out_buf_out, size_t *rc_out_cap_out)
{
  /* Frozen carrier: costas_update() is never called on this instance --
   * the direct C equivalent of Python's freeze_carrier=True. bn/tsamps
   * below are inert placeholders; only costas_wipeoff()'s phase accumulator
   * is ever touched. bn_fll = 0: no FLL anywhere in this object (see the
   * ASYNC_DSSS_RX_BN_CARRIER comment). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  costas_init (car_frozen_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
               doppler_hz_est / front_end_rate, s->tsamps, 0.0);

  dll_state_t *refine_dll = dp_xnn (
      dll_create (s->code, s->code_len, s->spc, chip_phase,
                  ASYNC_DSSS_RX_DLL_BN, 0.707, 0.5, s->refine_segments));

  /* Carrier->code aiding on the collection Dll too: without it the coupled
     code-rate Doppler drifts the code phase over the frozen refine prefix
     (~55 chips at a 50 kHz offset / 2.5 GHz over ~0.9 s), degrading the very
     despread stream CarrierAcquisition estimates from. Seeded off the coarse
     handoff Doppler (the refined value isn't known yet); the ~1 kHz coarse
     residual leaves only a small aiding error. Off when carrier_freq_hz==0. */
  if (s->carrier_freq_hz > 0.0)
    dll_set_rate_aid (refine_dll, doppler_hz_est / s->carrier_freq_hz);

  double partial_rate
      = s->chip_rate * (double)s->refine_segments / (double)s->code_len;
  double target_rate = (double)s->refine_samples_per_symbol * s->symbol_rate;

  RateConverter_state_t *refine_rc
      = dp_xnn (RateConverter_create (target_rate / partial_rate, 0));

  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  carrier_acq_state_t *ca = adr_new_carrier_acq (s, target_rate);

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
adr_free_refine_chain (async_dsss_receiver_state_t *s)
{
  carrier_acq_destroy (s->ca);
  RateConverter_destroy (s->refine_rc);
  dll_destroy (s->refine_dll);
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
adr_rebuild_refine_chain (async_dsss_receiver_state_t *s, double chip_phase,
                          double doppler_hz_est)
{
  costas_state_t         car_frozen;
  dll_state_t           *refine_dll  = NULL;
  RateConverter_state_t *refine_rc   = NULL;
  carrier_acq_state_t   *ca          = NULL;
  float _Complex        *dll_out_buf = NULL;
  size_t                 dll_out_cap = 0;
  float _Complex        *rc_out_buf  = NULL;
  size_t                 rc_out_cap  = 0;

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
  s->car_carry_len      = 0;
}

/* Allocate a fresh live-tracking chain (Dll/RateConverter/MpskReceiver +
 * the pre-despread carrier loop), mirroring dsss_receiver_core.c's own
 * dsss_rx_build_chain: costas_init()'s tsamps is one whole code period, and
 * costas_update() is called once per period from a non-data-aided
 * (squaring) combine of that period's emitted coherent-I&D partials (see
 * adr_track_period()) -- not once per partial (see this function's own
 * comment on costas_init() below for why). */
static void
adr_build_track_chain (async_dsss_receiver_state_t *s, double chip_phase,
                       double doppler_hz_est, size_t segments, size_t sps,
                       int n, costas_state_t *car_out, dll_state_t **dll_out,
                       RateConverter_state_t **rc_out,
                       mpsk_receiver_state_t **rx_out)
{
  double partial_rate = s->chip_rate * (double)segments / (double)s->code_len;
  double target_rate  = (double)sps * s->symbol_rate;

  dll_state_t *dll = dp_xnn (dll_create (
      s->code, s->code_len, s->spc, chip_phase, ASYNC_DSSS_RX_DLL_BN, 0.707,
      ASYNC_DSSS_RX_DLL_SPACING, segments));
  /* The lock detector's looks, sized for THIS operating point rather than
     left at the DLL's default 20 partials: the symbol period in partials is
     known from configuration, so the detector's looks are the symbol-scale
     max-power windows (dll_set_symbol_period, docs/design/
     async-dsss-receiver.md §3.7), and n_looks is det_n_noncoh over the
     window's coherent length at the design C/N0. Measured before this: at
     Es/N0 5.7 dB the default detector read "unlocked" 96% of the time on a
     loop that never lost the code (§12.3). A period under 2 partials (a
     very fast data clock) keeps per-partial looks and only sizes them. */
  if (segments > 1)
    {
      double partials_per_symbol = (double)segments * s->chip_rate
                                   / ((double)s->code_len * s->symbol_rate);
      (void)dll_set_symbol_period (dll, partials_per_symbol);
      size_t win  = dll_get_symbol_window (dll);
      size_t look = (win ? win : 1) * (s->tsamps / segments);
      double amp  = sqrt (pow (10.0, s->cn0_dbhz / 10.0)
                          / (s->chip_rate * (double)s->spc));
      int    nl   = det_n_noncoh (amp, (int)look, 0.99, 1e-3, 4000);
      if (nl >= 1)
        {
          (void)dll_configure_lock (dll, 1e-3, (size_t)nl, 0.0);
          /* The drop count from the miss probability the sizing bought
             (1 - 0.99) at a 1e-6 per-decision false-drop budget -- three,
             against configure_lock's fixed two. */
          (void)dll_set_lock_verify (dll, dll->lock.n_up,
                                     (uint32_t)det_verify_count (0.01, 1e-6));
        }
    }

  /* Carrier->code aiding: the code-rate Doppler is coupled to the carrier
     offset through the same v/c, so feed the refined carrier estimate into
     the code NCO's rate. The discriminator alone cannot pull in that
     constant rate offset at low SNR (~24.6 chips/s at 20 kHz / 2.5 GHz), so
     unaided the code loop walks off even with a correct carrier. Off when
     carrier_freq_hz == 0 (a pure baseband offset with no clock dilation). */
  if (s->carrier_freq_hz > 0.0)
    dll_set_rate_aid (dll, doppler_hz_est / s->carrier_freq_hz);

  RateConverter_state_t *rc
      = dp_xnn (RateConverter_create (target_rate / partial_rate, 0));

  /* MpskReceiver's own carrier loop is seeded at 0, NOT doppler_hz_est
   * again -- same reasoning as dsss_receiver_core.c's own
   * dsss_rx_build_chain(): the pre-despread Costas loop below already removes
   * the FULL physical Doppler, so only a small residual should reach
   * MpskReceiver. Re-seeding it with the full doppler_hz_est (evaluated at
   * target_rate=sps*symbol_rate, a much smaller rate than the front end)
   * double-counts and can alias past Nyquist at large offsets, far
   * outside MpskReceiver's own carrier_nda pull-in range. */
  /* bn_timing 0.005, matching dsss_receiver_core.c's own dsss_rx_build_chain
     -- see the measurement table there. Short version: the rebuild's timing
     loop steers RateSync's accumulator rather than a Farrow interpolator, 0.01
     is too wide for that on a despread stream, and the value is chosen on
     steady-state EVM rather than BER (BPSK's BER saturates first, and the
     BER-optimal 0.001 is ~8 dB worse in EVM). */
  mpsk_receiver_state_t *rx = dp_xnn (mpsk_receiver_create (
      s->m, (double)sps, (size_t)n, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707,
      0.005, 0.3, 0.0, s->differential, MPSK_RX_NUM_PHASES, 1,
      MPSK_RX_AGC_BW_RATIO));

  /* Per-CODE-PERIOD cadence (tsamps = one whole period), matching
   * dsss_receiver_core.c's own mechanism -- NOT once per dll_steps()
   * partial. The carrier discriminator combines the whole period's coherent-
   * I&D partials (a non-data-aided squaring combine, adr_track_period()) into
   * one full-period error; a per-partial cadence would shrink each update's
   * integration by `segments`, too weak/noisy to steer a clean carrier
   * estimate. Pure PLL, no FLL (bn_fll = 0 below). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  /* bn_fll = 0: pure PLL, no FLL (see ASYNC_DSSS_RX_BN_CARRIER's comment --
     the FLL cross-product is too noisy on the despread-symbol input and
     drives the carrier wander that causes the residual slips). */
  costas_init (car_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
               doppler_hz_est / front_end_rate, s->tsamps, 0.0);

  *dll_out = dll;
  *rc_out  = rc;
  *rx_out  = rx;
}

static void
adr_free_track_chain (async_dsss_receiver_state_t *s)
{
  mpsk_receiver_destroy (s->rx);
  RateConverter_destroy (s->rc);
  dll_destroy (s->dll);
  s->rx  = NULL;
  s->rc  = NULL;
  s->dll = NULL;
}

/* Size the track chain's per-period scratch for the chain just built. One
 * output per input sample bounds the Dll; the resampler's output is that
 * times its rate plus its own margin. Grow-only, so a regrid to a lower
 * rate leaves the larger buffer in place rather than churning. */
static void
adr_size_track_scratch (async_dsss_receiver_state_t *s)
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
adr_rebuild_track_chain (async_dsss_receiver_state_t *s, double chip_phase,
                         double doppler_hz_est, size_t segments, size_t sps,
                         int n)
{
  costas_state_t         car;
  dll_state_t           *dll = NULL;
  RateConverter_state_t *rc  = NULL;
  mpsk_receiver_state_t *rx  = NULL;
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
}

/* The residual-carrier estimator on a despread stream at `target_rate`
 * samples per second -- the refine chain's, and the cell mode's on its
 * own live chain (adr_cell_refine). Sized once for both: the resolution
 * is the refine's (refine_samples_per_symbol * symbol_rate / refine_n_fft,
 * so the same Hz per bin at either rate), the design SNR the derated
 * C/N0's, the dwell floored at refine_min_blocks (#1265, design section
 * 12.16: det_n_noncoh sized it for detection at the derated C/N0, two
 * blocks at 45 dB-Hz with the shipped margin, while the estimate's noise
 * the chain must pull in from is 210 Hz there and 77 at seven), bounded
 * by the give-up cap. */
static carrier_acq_state_t *
adr_new_carrier_acq (const async_dsss_receiver_state_t *s, double target_rate)
{
  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  double effective_cn0_dbhz = s->cn0_dbhz - s->refine_design_margin_db;
  double design_snr
      = sqrt (pow (10.0, effective_cn0_dbhz / 10.0) / target_rate);
  double resolution_hz = (double)s->refine_samples_per_symbol * s->symbol_rate
                         / (double)s->refine_n_fft;
  carrier_acq_state_t *ca = dp_xnn (carrier_acq_create (
      target_rate, s->symbol_rate, resolution_hz, s->refine_zero_pad,
      0 /* window=hann */, 0.0f, NULL, 0, s->pfa, s->pd, design_snr,
      s->refine_sequential, s->refine_max_n_blocks));
  if (ca->dwell_target < s->refine_min_blocks)
    ca->dwell_target = s->refine_min_blocks;
  if (ca->dwell_target > ca->max_n_blocks)
    ca->dwell_target = ca->max_n_blocks;
  return ca;
}

/* The cell mode's carrier pull-in (design section 12.27): the seed's
 * residual estimated on the live chain's own despread stream -- the
 * hand-off flavor's estimator, fed what the RateConverter hands
 * MpskReceiver, no second chain -- and applied once, when the estimator
 * is ready or has given up: loop 1 retuned there (its phase continues;
 * the aid the Dll is steered on follows it every period), MpskReceiver
 * re-centred so its own estimate starts from zero. A searcher's data-block
 * copy seeds hundreds of Hz off (section 12.14), past loop 1's bound
 * (ASYNC_DSSS_RX_CARRIER_PULLIN_HZ); without this a cell receiver holds
 * code lock on it and never symbol lock, and the holder's zone breaks on
 * its flickering flag. */
static void
adr_cell_refine (async_dsss_receiver_state_t *s, size_t n_rc)
{
  carrier_acq_state_t *ca = s->ca;
  carrier_acq_steps (ca, s->track_rc_out_buf, n_rc);
  const size_t cap = ca->sequential ? ca->max_n_blocks : ca->dwell_target;
  if (!ca->ready && ca->n_blocks < cap)
    return;
  const double fs = s->chip_rate * (double)s->spc;
  const double f  = costas_get_norm_freq (&s->car) * fs
                    + (ca->ready ? ca->residual_hz : 0.0);
  costas_set_norm_freq (&s->car, f / fs);
  mpsk_receiver_set_norm_freq (s->rx, 0.0);
  s->cell_refined = 1;
}

/* One frozen-carrier-wiped code period through refine_dll -> refine_rc ->
 * CarrierAcquisition. `period` must be exactly s->tsamps samples.
 * carrier_acq_steps() is documented as a no-op once ready (or the
 * give-up cap) is reached, so calling it past that point is harmless.
 *
 * `refine_dll` MUST oversample the epoch: with asynchronous data the
 * residual carrier rides a ~symbol_rate-wide data-modulated spectrum, and
 * CarrierAcquisition's PSDMF can only locate its centre if that spectrum is
 * sampled above its Nyquist. `dll_lookback_segments(refine_max_error_db)`
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
adr_refine_period (async_dsss_receiver_state_t *s,
                   const float _Complex        *period)
{
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car_frozen, period[i]);

  size_t n_dll = dll_steps (s->refine_dll, s->car_wiped_buf, s->tsamps,
                            s->refine_dll_out_buf, s->refine_dll_out_cap);
  if (n_dll == 0)
    return;

  size_t n_rc
      = RateConverter_execute (s->refine_rc, s->refine_dll_out_buf, n_dll,
                               s->refine_rc_out_buf, s->refine_rc_out_cap);
  if (n_rc == 0)
    return;

  carrier_acq_steps (s->ca, s->refine_rc_out_buf, n_rc);
}

/* Refine-stage entry point: buffer raw samples into whole `tsamps`-sample
 * periods (the shared car_carry_buf -- refine and track never run
 * concurrently) and run adr_refine_period() on each complete one. */
static void
adr_process_refine (async_dsss_receiver_state_t *s, const float _Complex *x,
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
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      adr_refine_period (s, x + pos);
      pos += s->tsamps;
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
/* The cell mode's correction, once per `correct_periods` code periods
 * (design section 12.22-12.24 as a mode of this receiver, #1283): the held
 * phase is dead-reckoned across the interval on the carrier loop's Doppler
 * (the chips dilate with the carrier), moved by a gain -- in chips, through
 * the discriminator's design slope -- times what the coasting Dll's
 * discriminator read over the interval (dll_take_error: every steer, the
 * block mean), and the Dll put back at it. A coasting loop drifts on its
 * NCO's quantisation of the aid (1.9 chips/s at 18 ppm, 12.22), so the
 * phase is kept in double here and the loop re-put each interval. Gain 1
 * through the pull-in intervals (the seed's residual, up to the
 * discriminator's clamp), the design gain after. The correction is applied
 * before the first lock, or while the code flag is up; with the flag down
 * the phase only dead-reckons, so a departed emitter's receiver cannot walk
 * onto a neighbour (#1271's hazard in the searcher-timed form). The carrier
 * is the hand-off flavor's own loop 1, running (it is what follows SPEC's
 * 500 Hz/s pre-despread; MpskReceiver's loop alone cannot), held on both
 * flags down as there.
 *
 * The Dll is brought to the held phase by RATE, not by a phase kick, and
 * every PERIOD, not once an interval: what it must make up (the held phase
 * dead-reckoned to this period's end against its own, the correction plus
 * its NCO's quantisation drift) becomes a bias on its rate aid for the
 * next period, the way its own loop steers -- adr_cell_steer(). A kick
 * (dll_set_code_phase) at a period boundary -- where a receiver fed whole
 * periods always is -- lands on the code's wrap, and moved across it the
 * Dll's partial bookkeeping emits or skips a period's partials (#1287;
 * measured: a symbol slip every few intervals on SPEC's geometry, where
 * the hand-off flavor on the same capture decoded clean). And spread over
 * a whole interval the bias falls under the NCO's 32-bit rate step (a few
 * parts in 10^7 -- 0.075 chip over 154 periods), so the Dll's phase
 * sawtoothed 0.06 chip about the held one (measured on the jitter
 * harness); over one period the same step is half a thousandth of a chip. */
static void
adr_cell_correct (async_dsss_receiver_state_t *s, int code_up)
{
  const double fs       = s->chip_rate * (double)s->spc;
  const double interval = (double)s->period_count * (double)s->tsamps;
  s->period_count       = 0;
  double       sum;
  const size_t n   = dll_take_error (s->dll, &sum);
  const double aid = s->carrier_freq_hz > 0.0 ? costas_get_norm_freq (&s->car)
                                                    * fs / s->carrier_freq_hz
                                              : 0.0;
  s->held_phase += (1.0 + aid) / (double)s->spc * interval;
  if (n && (!s->had_lock || code_up))
    {
      const double g
          = s->intervals < (uint64_t)s->pullin_intervals ? 1.0 : s->cell_gain;
      /* A positive discriminator (early over late) says the local code is
         behind the signal: the loop would have advanced it. The read is in
         the discriminator's units, (2 - spacing) per chip, so the gain
         applies in chips. */
      s->held_phase += g * sum / (double)n / ASYNC_DSSS_RX_DLL_DISC_SLOPE;
    }
  s->intervals++;
}

/* The per-period steer of the coasting Dll onto the held phase: the held
 * phase dead-reckoned to the end of the period just processed, against the
 * Dll's own, becomes a rate bias for the next period. */
static void
adr_cell_steer (async_dsss_receiver_state_t *s, double aid)
{
  const double sf    = (double)s->code_len;
  const double h_now = s->held_phase
                       + (1.0 + aid) / (double)s->spc
                             * ((double)s->period_count * (double)s->tsamps);
  double       delta = fmod (h_now - dll_get_code_phase (s->dll), sf);
  if (delta > 0.5 * sf)
    delta -= sf;
  else if (delta <= -0.5 * sf)
    delta += sf;
  s->cell_rate_bias = delta / sf;
  dll_set_rate_aid (s->dll, aid + s->cell_rate_bias);
}

static size_t
adr_track_period (async_dsss_receiver_state_t *s, const float _Complex *period,
                  float _Complex *dll_out, size_t max_out)
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
     up (dll_hold_here, car_held), so what a blip steers is dropped on
     re-entry (measured: a neighbour 2 kHz off crossing at 4 chips per
     second, 8-10 blips, no follow). Holding on ONE flag down was tried
     and measured wrong: two live emitters 475 Hz apart crossing at a chip
     per second degrade both receivers' symbol flags, and a code loop held
     through that cannot re-centre on its own emitter afterwards -- both
     flags then stay down for the rest of the watch, a false loss. Run,
     it rides the crossing out (code lock held in 5 of 6 trials). A
     neighbour within about a kilohertz crossing at a chip or two a second
     can lock the carrier, and to a receiver on its own that IS a return --
     the pool, which knows that emitter has a slot, is where it is told
     apart (#1275). Before the first lock the loops must run: that is the
     pull-in. */
  const int code_up = dll_get_locked (s->dll), sym_up = s->sym_lockdet.locked;
  if (code_up && sym_up)
    {
      s->had_lock = 1;
      s->car_held = s->car;
      if (!s->cell)
        dll_hold_here (s->dll);
    }
  const int car_coast = s->had_lock && !code_up && !sym_up;
  if (car_coast && !s->car_coasting)
    s->car = s->car_held;
  s->car_coasting = car_coast;
  /* The cell mode's Dll coasts from the seed: its own loop never closes,
     the interval correction below is its steer. */
  dll_set_coast (s->dll, s->cell || (s->had_lock && !code_up && !sym_up));
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car, period[i]);
  size_t n_out
      = dll_steps (s->dll, s->car_wiped_buf, s->tsamps, dll_out, max_out);
  if (n_out > 0 && !car_coast)
    {
      /* NON-DATA-AIDED carrier discriminator on the despread coherent-I&D
       * windows. Each emitted partial is dll_out[i] = A * d * exp(j*phi),
       * d = +/-1 BPSK data, phi = residual carrier phase. Squaring removes
       * the data (d^2 = 1) with NO per-partial sign DECISION -- the failure
       * mode of the old decision-directed sum, which at SPEC's async ratio
       * (~0.9 symbols/code period) straddles a data transition almost every
       * period, giving a garbage +/-57deg discriminator that averages to
       * zero so the loop never tracks. The dll_steps() windows are short
       * (dll_lookback_segments oversampling, the same transition-free I&D the
       * refine PSDMF consumes): a window entirely within one symbol squares
       * cleanly; one straddling a transition merely loses amplitude, not
       * phase -- exactly why NDA is transition-robust where the sign-wipe is
       * not. Sum the squares and halve the angle for the carrier phase
       * (mod pi, the harmless BPSK sign ambiguity). Feed it to the existing
       * Costas loop via a unit phasor (Re = cos(phi) >= 0, so its own data-
       * wipe never flips it, and its discriminator reads sin(phi)). This is
       * the same NDA principle the downstream MpskReceiver carrier loop uses,
       * applied PRE-despread so despreading stays coherent (no residual
       * carrier rotating within the correlation) instead of leaving the whole
       * carrier burden to the post-despread loop. */
      float _Complex sq = 0.0f;
      for (size_t i = 0; i < n_out; i++)
        sq += dll_out[i] * dll_out[i];
      double phi       = 0.5 * atan2 (cimag (sq), creal (sq));
      float _Complex P = (float)cos (phi) + (float)sin (phi) * I;
      /* The cell mode holds loop 1 at the seed's frequency until its
         estimate is folded (adr_cell_refine): a residual past the loop's
         bound wraps its discriminator as a zero-mean sinusoid and the loop
         flails, and the estimator would then read a wandering residual --
         the hand-off's refine wipes with a FROZEN carrier for the same
         reason. Measured: at 45 dB-Hz the estimate found the peak through
         the flailing; at 40 an 822 Hz data-block seed did not pull in
         (section 12.27). */
      if (!(s->cell && !s->cell_refined))
        costas_update (&s->car, P);
      /* Continuous carrier->code aiding: the pre-despread Costas tracks the
         FULL carrier offset (including the 500 Hz/s ramp), so refresh the
         code NCO's rate bias from it every period. This keeps the initial
         build-time seed (dll_set_rate_aid in adr_build_track_chain) fresh as
         the ramp drifts, and applies at the next period boundary
         (dll_set_rate_ aid only stores the field, never clobbers phase_inc).
         Off when carrier_freq_hz == 0. */
      if (s->carrier_freq_hz > 0.0)
        dll_set_rate_aid (s->dll, costas_get_norm_freq (&s->car)
                                      * (s->chip_rate * (double)s->spc)
                                      / s->carrier_freq_hz);
    }
  if (s->cell)
    {
      s->period_count++;
      const double aid = s->carrier_freq_hz > 0.0
                             ? costas_get_norm_freq (&s->car)
                                   * (s->chip_rate * (double)s->spc)
                                   / s->carrier_freq_hz
                             : 0.0;
      if (s->period_count >= s->correct_periods)
        adr_cell_correct (s, code_up); /* resets period_count */
      adr_cell_steer (s, aid);
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
adr_track_period_chain (async_dsss_receiver_state_t *s,
                        const float _Complex *period, float _Complex *out,
                        size_t max_out)
{
  size_t n_dll = adr_track_period (s, period, s->track_dll_out_buf,
                                   s->track_dll_out_cap);
  size_t n_rc
      = RateConverter_execute (s->rc, s->track_dll_out_buf, n_dll,
                               s->track_rc_out_buf, s->track_rc_out_cap);
  if (s->cell && !s->cell_refined && n_rc)
    adr_cell_refine (s, n_rc);
  return mpsk_receiver_steps (s->rx, s->track_rc_out_buf, n_rc, out, max_out);
}

static size_t
adr_track_chain (async_dsss_receiver_state_t *s, const float _Complex *x,
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
      double re   = (double)crealf (out[i]);
      double im   = (double)cimagf (out[i]);
      s->lock_num = ema_step (s->lock_num, re * re - im * im, s->lock_alpha);
      s->lock_den = ema_step (s->lock_den, re * re + im * im, s->lock_alpha);
      s->lock_metric = (s->lock_den > 0.0) ? s->lock_num / s->lock_den : 0.0;
      (void)lockdet_step (&s->sym_lockdet, s->lock_metric);
    }
  return emitted;
}

/* The one constructor behind both flavors. `cell` decides whether
 * the embedded Acquisition exists (the searching flavor) or the receiver
 * waits idle for an outside seed (hand-off mode, section 11.1 of the
 * design page): the chains past the seed are identical, so everything
 * else is shared verbatim. */
static async_dsss_receiver_state_t *
adr_new (const uint8_t *code, size_t code_len, double chip_rate,
         double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
         double pd, double doppler_uncertainty, size_t segments, size_t sps,
         int differential, double refine_max_error_db,
         size_t refine_samples_per_symbol, double refine_design_margin_db,
         size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
         size_t refine_max_n_blocks, double carrier_freq_hz,
         double lost_confirm_s, bool cell, size_t correct_periods, double gain,
         size_t pullin_intervals)
{
  if (!code || code_len < 1 || chip_rate <= 0.0 || symbol_rate <= 0.0
      || spc < 1 || (m != 2 && m != 4 && m != 8) || segments < 1 || sps < 1
      || refine_samples_per_symbol < 1 || refine_n_fft < 1
      || refine_zero_pad < 1 || carrier_freq_hz < 0.0
      || !(lost_confirm_s >= 0.0))
    return NULL;
  /* The cell mode's own: an interval of at least one period, a gain in
     (0, 1] (1 puts the phase at the read). */
  if (cell && (correct_periods < 1 || !(gain > 0.0 && gain <= 1.0)))
    return NULL;

  async_dsss_receiver_state_t *obj = dp_xcalloc (1, sizeof (*obj));

  obj->code = dp_xmalloc (code_len);
  memcpy (obj->code, code, code_len);
  obj->code_len = code_len;

  obj->acq = !cell ? dp_xnn (acq_create_continuous (
                         obj->code, code_len, spc, chip_rate, symbol_rate,
                         cn0_dbhz, doppler_uncertainty, pfa, pd,
                         0 /* noise_mode=mean */, 1, 0.0))
                   : NULL;
  /* A physically-coupled carrier moves the code too: the searcher's
     hand-off advances its hit's phase by the drift over half its dwell
     (#1254), and a coherent block aligns its epochs (#1256). The same
     carrier the tracking Dll's aid uses. */
  if (obj->acq && carrier_freq_hz > 0.0)
    (void)acq_set_carrier_freq_hz (obj->acq, carrier_freq_hz);

  obj->spc          = spc;
  obj->m            = m;
  obj->differential = differential;
  obj->chip_rate    = chip_rate;
  obj->symbol_rate  = symbol_rate;
  obj->cn0_dbhz     = cn0_dbhz;
  obj->pfa          = pfa;
  obj->pd           = pd;
  adr_enter (obj, cell ? ASYNC_DSSS_RX_IDLE : ASYNC_DSSS_RX_SEARCHING);

  /* The release clock in input samples: the rule is a time, and the input
   * rate is the one clock every state of this object is fed at. */
  obj->lost_confirm_s = lost_confirm_s;
  obj->lost_confirm_samples
      = (uint64_t)llround (lost_confirm_s * chip_rate * (double)spc);

  obj->refine_max_error_db       = refine_max_error_db;
  obj->refine_samples_per_symbol = refine_samples_per_symbol;
  obj->refine_design_margin_db   = refine_design_margin_db;
  obj->refine_n_fft              = refine_n_fft;
  obj->refine_zero_pad           = refine_zero_pad;
  obj->refine_sequential         = refine_sequential;
  obj->refine_max_n_blocks       = refine_max_n_blocks;
  obj->refine_min_blocks         = ASYNC_DSSS_RX_REFINE_MIN_BLOCKS;
  obj->carrier_freq_hz           = carrier_freq_hz;
  obj->cell                      = cell ? 1 : 0;
  obj->correct_periods           = correct_periods;
  obj->cell_gain                 = gain;
  obj->pullin_intervals          = pullin_intervals;

  /* tsamps (one code period, samples) is fixed for this object's entire
   * lifetime -- refine_segments is likewise fixed (depends only on
   * tsamps/refine_max_error_db, both construction-time invariants), so
   * both the shared carry scratch and refine_segments are computed ONCE
   * here, never per-rebuild. */
  obj->tsamps = code_len * spc;
  obj->refine_segments
      = dll_lookback_segments (obj->tsamps, refine_max_error_db);
  obj->car_wiped_buf = dp_xmalloc (obj->tsamps * sizeof (*obj->car_wiped_buf));
  obj->car_carry_buf = dp_xmalloc (obj->tsamps * sizeof (*obj->car_carry_buf));
  obj->car_carry_len = 0;

  /* Placeholder chains (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (fixed shape, same rationale
   * dsss_receiver_core.c's own state struct doc comment gives). */
  /* The cell mode has no refine: the searcher-timed correction pulls the
     seed's residual in (12.23), so none of that chain is built -- the
     blob skips it too (keyed on `cell`, as `handoff` keys the engine). */
  if (!cell)
    adr_build_refine_chain (obj, 0.0, 0.0, &obj->car_frozen, &obj->refine_dll,
                            &obj->refine_rc, &obj->ca,
                            &obj->refine_dll_out_buf, &obj->refine_dll_out_cap,
                            &obj->refine_rc_out_buf, &obj->refine_rc_out_cap);
  obj->refine_samples_fed = 0;

  adr_build_track_chain (obj, 0.0, 0.0, segments, sps, adr_derive_m_out (sps),
                         &obj->car, &obj->dll, &obj->rc, &obj->rx);
  obj->segments      = segments;
  obj->sps           = sps;
  obj->n             = adr_derive_m_out (sps);
  obj->car_carry_len = 0; /* both placeholder builds share this buffer */
  adr_size_track_scratch (obj);

  obj->seed_chip_phase     = 0.0;
  obj->seed_doppler_hz_est = 0.0;
  obj->doppler_hz_est      = 0.0;
  obj->cn0_dbhz_est        = 0.0;
  obj->samples_fed         = 0;

  /* Symbol-lock detector config (running state reset per track-chain build,
   * adr_reset_lock()). */
  obj->lock_alpha = 1.0 / (double)ASYNC_DSSS_RX_LOCK_DWELL;
  lockdet_init (&obj->sym_lockdet, ASYNC_DSSS_RX_LOCK_UP,
                ASYNC_DSSS_RX_LOCK_DOWN, ASYNC_DSSS_RX_LOCK_N_UP,
                ASYNC_DSSS_RX_LOCK_N_DOWN);
  adr_reset_lock (obj);
  /* The cell mode's carrier pull-in estimator, on the live chain's
     despread stream at its own rate (adr_cell_refine); the hand-off
     flavor's is built with its refine chain per seed. */
  if (cell)
    {
      obj->ca = adr_new_carrier_acq (obj, (double)sps * symbol_rate);
      /* The live chain hands the estimator `sps` samples per symbol
         where the refine chain hands it refine_samples_per_symbol; at the
         refine's resolution a block then holds the same symbols in twice
         the samples, and the estimate needs the dwell scaled by that
         ratio to reach the refine's noise. Measured (validate_receiver
         _pullin, design section 12.28): at 40 dB-Hz the refine's dwell
         pulled in 7-9 draws of 10 from 100 Hz on, twice it 10 of 10 to
         1000 Hz -- the hand-off's own curve -- for 40 ms more per seed
         at 45 dB-Hz. */
      const size_t ratio = sps > refine_samples_per_symbol
                               ? sps / refine_samples_per_symbol
                               : 1;
      obj->ca->dwell_target *= ratio;
      if (obj->ca->dwell_target > obj->ca->max_n_blocks)
        obj->ca->dwell_target = obj->ca->max_n_blocks;
    }
  return obj;
}

async_dsss_receiver_state_t *
async_dsss_receiver_create (
    const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate,
    size_t spc, int m, double cn0_dbhz, double pfa, double pd,
    double doppler_uncertainty, size_t segments, size_t sps, int differential,
    double refine_max_error_db, size_t refine_samples_per_symbol,
    double refine_design_margin_db, size_t refine_n_fft,
    size_t refine_zero_pad, bool refine_sequential, size_t refine_max_n_blocks,
    double carrier_freq_hz, double lost_confirm_s)
{
  return adr_new (code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz,
                  pfa, pd, doppler_uncertainty, segments, sps, differential,
                  refine_max_error_db, refine_samples_per_symbol,
                  refine_design_margin_db, refine_n_fft, refine_zero_pad,
                  refine_sequential, refine_max_n_blocks, carrier_freq_hz,
                  lost_confirm_s, false, 1, 1.0, 0);
}

async_dsss_receiver_state_t *
async_dsss_receiver_create_cell (const uint8_t *code, size_t code_len,
                                 double chip_rate, double symbol_rate,
                                 size_t spc, int m, double cn0_dbhz,
                                 double pfa, double pd, size_t segments,
                                 size_t sps, int differential,
                                 double carrier_freq_hz, double lost_confirm_s,
                                 size_t correct_periods, double gain,
                                 size_t pullin_intervals)
{
  /* The refine parameters are the searching flavor's defaults: the cell
     mode builds no refine chain, so they size nothing. */
  return adr_new (code, code_len, chip_rate, symbol_rate, spc, m, cn0_dbhz,
                  pfa, pd, 0.0, segments, sps, differential, 0.5, 4, 14.0, 64,
                  8, false, 100000, carrier_freq_hz, lost_confirm_s, true,
                  correct_periods, gain, pullin_intervals);
}

void
async_dsss_receiver_destroy (async_dsss_receiver_state_t *state)
{
  if (!state)
    return;
  adr_free_track_chain (state);
  adr_free_refine_chain (state);
  free (state->track_dll_out_buf);
  free (state->track_rc_out_buf);
  free (state->car_wiped_buf);
  free (state->car_carry_buf);
  acq_destroy (state->acq);
  free (state->code);
  free (state);
}

void
async_dsss_receiver_reset (async_dsss_receiver_state_t *state)
{
  if (state->acq)
    acq_reset (state->acq);
  /* Best-effort: on OOM, leave the current chains in place rather than
   * signal a failure this void-returning lifecycle function can't report
   * (matches dsss_receiver_reset()'s own contract). */
  if (!state->cell)
    adr_rebuild_refine_chain (state, 0.0, 0.0);
  adr_rebuild_track_chain (state, 0.0, 0.0, state->segments, state->sps,
                           state->n);
  state->held_phase     = 0.0;
  state->cell_rate_bias = 0.0;
  state->period_count   = 0;
  state->intervals      = 0;
  /* Hand-off mode has no search to return to: idle, waiting for the next
   * seed, is how the holder of a pool reuses the object. */
  adr_enter (state, state->acq ? ASYNC_DSSS_RX_SEARCHING : ASYNC_DSSS_RX_IDLE);
  state->seed_chip_phase     = 0.0;
  state->seed_doppler_hz_est = 0.0;
  state->doppler_hz_est      = 0.0;
  state->cn0_dbhz_est        = 0.0;
  state->samples_fed         = 0;
}

size_t
async_dsss_receiver_steps_max_out (async_dsss_receiver_state_t *state)
{
  (void)state;
  return 0; /* caller falls back to allocating x_len -- always sufficient,
               symbols emitted <= raw samples in. */
}

int
async_dsss_receiver_seed (async_dsss_receiver_state_t *state,
                          double chip_phase, double doppler_hz_est,
                          double cn0_dbhz_est)
{
  /* "Assigned once" lives here: only a receiver with nothing to lose takes
   * a seed. Lost is deliberately refused too -- the holder releases the
   * assignment through reset(), so a stale one is never silently
   * overwritten. */
  if (state->state != ASYNC_DSSS_RX_IDLE
      && state->state != ASYNC_DSSS_RX_SEARCHING)
    return DP_ERR_INVALID;
  if (!isfinite (chip_phase) || !isfinite (doppler_hz_est) || chip_phase < 0.0
      || chip_phase >= (double)state->code_len)
    return DP_ERR_INVALID;

  if (state->cell)
    {
      /* No refine: the track chain from the seed, its Dll held from the
         first sample (the loop coasts; the correction steers, 12.22), the
         held phase the seed's, the carrier loop seeded at the seed's
         Doppler as the hand-off's is. Refining is the pull-in: gain 1 for
         pullin_intervals, then tracking. */
      adr_rebuild_track_chain (state, chip_phase, doppler_hz_est,
                               state->segments, state->sps, state->n);
      dll_hold_here (state->dll);
      dll_set_coast (state->dll, 1);
      state->held_phase     = chip_phase;
      state->cell_rate_bias = 0.0;
      state->period_count   = 0;
      state->intervals      = 0;
      state->had_lock       = 0;
      state->car_coasting   = 0;
      carrier_acq_reset (state->ca);
      state->cell_refined = 0;
    }
  else
    adr_rebuild_refine_chain (state, chip_phase, doppler_hz_est);

  adr_enter (state, ASYNC_DSSS_RX_REFINING);
  state->seed_chip_phase     = chip_phase;
  state->seed_doppler_hz_est = doppler_hz_est;
  state->doppler_hz_est      = doppler_hz_est;
  state->cn0_dbhz_est        = cn0_dbhz_est;
  return DP_OK;
}

/* The release rule of the design page's section 11.2, run once per
 * tracking call: both lock flags down, without a break, for longer than
 * the confirm interval means the emitter is gone. One flag down is a
 * degrade and leaves the clock alone; either flag up restarts it. */
static void
adr_release_clock (async_dsss_receiver_state_t *s, size_t x_len)
{
  if (s->lost_confirm_samples == 0)
    return;
  if (dll_get_locked (s->dll) || s->sym_lockdet.locked)
    {
      s->both_down_samples = 0;
      return;
    }
  s->both_down_samples += x_len;
  if (s->both_down_samples > s->lost_confirm_samples)
    adr_enter (s, ASYNC_DSSS_RX_LOST);
}

size_t
async_dsss_receiver_steps (async_dsss_receiver_state_t *state,
                           const float _Complex *x, size_t x_len,
                           float _Complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;
  state->state_samples += x_len;

  if (state->state == ASYNC_DSSS_RX_LOST)
    state->both_down_samples += x_len; /* since the flags dropped */
  if (state->state == ASYNC_DSSS_RX_IDLE || state->state == ASYNC_DSSS_RX_LOST)
    return 0; /* consumed and discarded: the feeding loop has no case */

  if (state->state == ASYNC_DSSS_RX_SEARCHING)
    {
      uint64_t     before = state->samples_fed;
      acq_result_t hit;
      state->samples_fed += x_len;
      size_t n_hits = acq_push (state->acq, x, x_len, &hit, 1);
      if (n_hits == 0)
        return 0;

      /* Same "exact unconsumed tail of THIS call" technique
       * dsss_receiver_core.c's own steps() uses. */
      uint64_t total_after = before + (uint64_t)x_len;
      uint64_t consumed    = state->acq->samples_consumed;
      uint64_t tail64
          = (total_after > consumed) ? (total_after - consumed) : 0;
      size_t tail_len = (tail64 > (uint64_t)x_len) ? x_len : (size_t)tail64;
      const float _Complex *tail = x + (x_len - tail_len);

      /* A hit is a seed the object made for itself -- the same path an
       * outside detection takes. acq_build_handoff() folds the phase into
       * [0, code_len), so the seed is never refused from here. */
      acq_handoff_t ho;
      acq_build_handoff (state->acq, &hit, state->code_len, state->spc, &ho);
      (void)async_dsss_receiver_seed (state, ho.chip_phase, ho.doppler_hz_est,
                                      ho.cn0_dbhz_est);

      return async_dsss_receiver_steps (state, tail, tail_len, out, max_out);
    }

  if (state->state == ASYNC_DSSS_RX_REFINING && state->cell)
    {
      /* The pull-in: the live chain from the seed, the carrier residual
         estimated on its own despread stream and folded once
         (adr_cell_refine), gain 1 on the correction through
         pullin_intervals (the gain schedule, whatever the state).
         Tracking once the estimate is folded (or given up) and a lock
         flag is up -- what the hand-off flavor's hand-over means -- or
         at the end of the pull-in intervals regardless, so a seed that
         never locks reaches the release clock. No release clock before
         that: lost is reached from tracking, as the hand-off's is. */
      size_t n = adr_track_chain (state, x, x_len, out, max_out);
      if (state->cell_refined
          && (dll_get_locked (state->dll) || state->sym_lockdet.locked
              || state->intervals >= (uint64_t)state->pullin_intervals))
        adr_enter (state, ASYNC_DSSS_RX_TRACKING);
      return n;
    }

  if (state->state == ASYNC_DSSS_RX_REFINING)
    {
      uint64_t before = state->refine_samples_fed;
      state->refine_samples_fed += x_len;
      adr_process_refine (state, x, x_len);

      carrier_acq_state_t *ca = state->ca;
      size_t cap = ca->sequential ? ca->max_n_blocks : ca->dwell_target;
      if (!ca->ready && ca->n_blocks < cap)
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
       * count `carrier_acq_create()` consumes per `n_blocks` increment)
       * -- NOT `ca->nfft` (the zero-padded PSD *transform* length,
       * `next_pow2(refine_n_fft*refine_zero_pad)`, a different and much
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
      double handover_chip_phase = state->seed_chip_phase;
      if (state->carrier_freq_hz > 0.0)
        {
          double dilation = refined_doppler_hz_est / state->carrier_freq_hz;
          double chips_elapsed
              = (double)samples_consumed_refine / (double)state->spc;
          handover_chip_phase
              = dp_fmod_pos (handover_chip_phase + dilation * chips_elapsed,
                             (double)state->code_len);
        }
      adr_rebuild_track_chain (state, handover_chip_phase,
                               refined_doppler_hz_est, state->segments,
                               state->sps, state->n);

      adr_enter (state, ASYNC_DSSS_RX_TRACKING);
      state->doppler_hz_est = refined_doppler_hz_est;

      return async_dsss_receiver_steps (state, tail, tail_len, out, max_out);
    }

  size_t n_out = adr_track_chain (state, x, x_len, out, max_out);
  adr_release_clock (state, x_len);
  return n_out;
}

int
async_dsss_receiver_configure_search_raw (async_dsss_receiver_state_t *state,
                                          size_t doppler_bins, size_t n_noncoh)
{
  if (!state->acq)
    return -1; /* hand-off mode: no search to pin */
  return acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
}

int
async_dsss_receiver_set_refine_min_blocks (async_dsss_receiver_state_t *state,
                                           size_t n_blocks)
{
  if (state->cell)
    return DP_ERR_INVALID; /* no refine to floor */
  state->refine_min_blocks = n_blocks;
  return DP_OK;
}

void
async_dsss_receiver_configure_lock_raw (async_dsss_receiver_state_t *state,
                                        double up_thresh, double down_thresh,
                                        size_t n_looks, double alpha,
                                        uint32_t n_up, uint32_t n_down)
{
  dll_configure_lock_raw (state->dll, up_thresh, down_thresh, n_looks, alpha,
                          n_up, n_down);
}

int
async_dsss_receiver_configure_chain_raw (async_dsss_receiver_state_t *state,
                                         size_t segments, size_t sps, int n)
{
  if (segments < 1 || sps < 1 || n < 1 || (int)(sps % (size_t)n) != 0)
    return -1;

  double chip_phase      = dll_get_code_phase (state->dll);
  double old_target_rate = (double)state->sps * state->symbol_rate;
  double doppler_hz_now
      = mpsk_receiver_get_norm_freq (state->rx) * old_target_rate;

  adr_rebuild_track_chain (state, chip_phase, doppler_hz_now, segments, sps,
                           n);
  return 0;
}

int
async_dsss_receiver_get_tracking (const async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_TRACKING;
}
int
async_dsss_receiver_get_refining (const async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_REFINING;
}
async_dsss_receiver_status_t
async_dsss_receiver_status (const async_dsss_receiver_state_t *s)
{
  /* Where the emitter is NOW. Once tracking (and held where it was in
   * lost) the estimate is the WHOLE carrier: loop 1's, in cycles per
   * front-end sample, plus what loop 2 -- the post-despread MpskReceiver
   * loop, at sps * symbol_rate -- has taken up beyond it, its loop-filter
   * integrator, the same sum configure_chain_raw() re-seeds a rebuilt
   * chain from (doppler#1261). While refining the frozen carrier IS the
   * seed; idle has no emitter. */
  double fs = s->chip_rate * (double)s->spc;
  double doppler_hz;
  switch (s->state)
    {
    case ASYNC_DSSS_RX_TRACKING:
    case ASYNC_DSSS_RX_LOST:
      doppler_hz = costas_get_norm_freq (&s->car) * fs
                   + mpsk_receiver_get_norm_freq (s->rx)
                         * ((double)s->sps * s->symbol_rate);
      break;
    case ASYNC_DSSS_RX_REFINING:
      doppler_hz = s->cell ? costas_get_norm_freq (&s->car) * fs
                                 + mpsk_receiver_get_norm_freq (s->rx)
                                       * ((double)s->sps * s->symbol_rate)
                           : costas_get_norm_freq (&s->car_frozen) * fs;
      break;
    default:
      doppler_hz = 0.0;
    }
  async_dsss_receiver_status_t r = {
    .state             = s->state,
    .doppler_hz        = doppler_hz,
    .chip_phase        = async_dsss_receiver_get_chip_phase (s),
    .code_rate         = async_dsss_receiver_get_code_rate (s),
    .cn0_dbhz_est      = s->cn0_dbhz_est,
    .code_locked       = async_dsss_receiver_get_code_locked (s),
    .locked            = async_dsss_receiver_get_locked (s),
    .lock_metric       = s->lock_metric,
    .lock_threshold    = async_dsss_receiver_get_lock_threshold (s),
    .car_last_error    = async_dsss_receiver_get_car_last_error (s),
    .mpsk_last_error   = async_dsss_receiver_get_mpsk_last_error (s),
    .state_samples     = s->state_samples,
    .both_down_samples = s->both_down_samples,
  };
  return r;
}

int
async_dsss_receiver_get_idle (const async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_IDLE;
}
int
async_dsss_receiver_get_lost (const async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_LOST;
}
double
async_dsss_receiver_get_doppler_hz (const async_dsss_receiver_state_t *state)
{
  return state->doppler_hz_est;
}
double
async_dsss_receiver_get_cn0_dbhz_est (const async_dsss_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}
size_t
async_dsss_receiver_get_segments (const async_dsss_receiver_state_t *state)
{
  return state->segments;
}
size_t
async_dsss_receiver_get_sps (const async_dsss_receiver_state_t *state)
{
  return state->sps;
}
int
async_dsss_receiver_get_n (const async_dsss_receiver_state_t *state)
{
  return state->n;
}
double
async_dsss_receiver_get_chip_phase (const async_dsss_receiver_state_t *state)
{
  return dll_get_code_phase (state->dll);
}
double
async_dsss_receiver_get_code_rate (const async_dsss_receiver_state_t *state)
{
  return dll_get_code_rate (state->dll);
}
double
async_dsss_receiver_get_lock (const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_lock (state->rx);
}
double
async_dsss_receiver_get_norm_freq (const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_norm_freq (state->rx);
}

double
async_dsss_receiver_get_nco_freq (const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_nco_freq (state->rx);
}

int
async_dsss_receiver_get_locked (const async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.locked;
}

double
async_dsss_receiver_get_lock_metric (const async_dsss_receiver_state_t *state)
{
  return state->lock_metric;
}

double
async_dsss_receiver_get_lock_threshold (
    const async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.up_thresh;
}

int
async_dsss_receiver_get_code_locked (const async_dsss_receiver_state_t *state)
{
  return dll_get_locked (state->dll);
}

double
async_dsss_receiver_get_car_last_error (
    const async_dsss_receiver_state_t *state)
{
  return costas_get_last_error (&state->car);
}

double
async_dsss_receiver_get_car_nco_freq (const async_dsss_receiver_state_t *state)
{
  return costas_get_nco_freq (&state->car);
}

double
async_dsss_receiver_get_mpsk_last_error (
    const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_last_error (state->rx);
}

size_t
async_dsss_receiver_state_bytes (const async_dsss_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (async_dsss_receiver_extra_t)
         + (s->acq ? acq_state_bytes (s->acq) : 0)
         + costas_state_bytes (&s->car_frozen)
         + (s->cell ? 0
                    : dll_state_bytes (s->refine_dll)
                          + RateConverter_state_bytes (s->refine_rc))
         + carrier_acq_state_bytes (s->ca) /* the refine's, or the cell's */
         + costas_state_bytes (&s->car) + costas_state_bytes (&s->car_held)
         + dll_state_bytes (s->dll) + RateConverter_state_bytes (s->rc)
         + mpsk_receiver_state_bytes (s->rx)
         + s->tsamps * sizeof (float _Complex);
}

void
async_dsss_receiver_get_state (const async_dsss_receiver_state_t *s,
                               void                              *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               async_dsss_receiver_state_bytes (s));
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
    .cell                = (uint8_t)(s->cell != 0),
    .lock_num            = s->lock_num,
    .lock_den            = s->lock_den,
    .lock_metric         = s->lock_metric,
    .sym_lockdet         = s->sym_lockdet,
    .held_phase          = s->held_phase,
    .cell_rate_bias      = s->cell_rate_bias,
    .period_count        = (uint64_t)s->period_count,
    .intervals           = s->intervals,
    .cell_refined        = (uint8_t)(s->cell_refined != 0),
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  if (s->acq)
    DP_W_CHILD (&_w, acq, s->acq);
  DP_W_CHILD (&_w, costas, &s->car_frozen);
  if (!s->cell)
    {
      DP_W_CHILD (&_w, dll, s->refine_dll);
      DP_W_CHILD (&_w, RateConverter, s->refine_rc);
    }
  DP_W_CHILD (&_w, carrier_acq, s->ca); /* the refine's, or the cell's */
  DP_W_CHILD (&_w, costas, &s->car);
  DP_W_CHILD (&_w, costas, &s->car_held);
  DP_W_CHILD (&_w, dll, s->dll);
  DP_W_CHILD (&_w, RateConverter, s->rc);
  DP_W_CHILD (&_w, mpsk_receiver, s->rx);
  dp_w_cf32 (&_w, s->car_carry_buf, s->tsamps);
}

int
async_dsss_receiver_set_state (async_dsss_receiver_state_t *s,
                               const void                  *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n
      || extra.refine_segments != (uint64_t)s->refine_segments
      || extra.car_carry_len > (uint64_t)s->tsamps
      || extra.cell != (uint8_t)(s->cell != 0)
      || extra.state > ASYNC_DSSS_RX_LOST)
    return DP_ERR_INVALID;
  if (s->acq)
    DP_R_CHILD (&_r, acq, s->acq);
  DP_R_CHILD (&_r, costas, &s->car_frozen);
  if (!s->cell)
    {
      DP_R_CHILD (&_r, dll, s->refine_dll);
      DP_R_CHILD (&_r, RateConverter, s->refine_rc);
    }
  DP_R_CHILD (&_r, carrier_acq, s->ca);
  DP_R_CHILD (&_r, costas, &s->car);
  DP_R_CHILD (&_r, costas, &s->car_held);
  DP_R_CHILD (&_r, dll, s->dll);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  DP_R_CHILD (&_r, mpsk_receiver, s->rx);
  dp_r_cf32 (&_r, s->car_carry_buf, s->tsamps);
  s->state               = extra.state;
  s->seed_chip_phase     = extra.seed_chip_phase;
  s->seed_doppler_hz_est = extra.seed_doppler_hz_est;
  s->doppler_hz_est      = extra.doppler_hz_est;
  s->cn0_dbhz_est        = extra.cn0_dbhz_est;
  s->refine_samples_fed  = extra.refine_samples_fed;
  s->car_carry_len       = (size_t)extra.car_carry_len;
  s->state_samples       = extra.state_samples;
  s->both_down_samples   = extra.both_down_samples;
  s->had_lock            = extra.had_lock;
  s->car_coasting        = extra.car_coasting;
  s->lock_num            = extra.lock_num;
  s->lock_den            = extra.lock_den;
  s->lock_metric         = extra.lock_metric;
  s->sym_lockdet         = extra.sym_lockdet;
  s->held_phase          = extra.held_phase;
  s->cell_rate_bias      = extra.cell_rate_bias;
  s->cell_refined        = extra.cell_refined != 0;
  s->period_count        = (size_t)extra.period_count;
  s->intervals           = extra.intervals;
  return DP_OK;
}
