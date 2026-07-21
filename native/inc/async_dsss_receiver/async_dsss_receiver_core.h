/**
 * @file async_dsss_receiver_core.h
 * @brief Composed continuous DSSS receiver: Acquisition -> handoff ->
 *        CarrierAcquisition refine -> Costas/Dll/RateConverter/
 *        MpskReceiver track, one object.
 *
 * The production C port of the validated Python prototype's own
 * search -> refine -> track pipeline
 * (`prototypes/async_despreader/{despreader_coupled,freq_refine,
 * e2e_acq_to_despreader}.py`). Unlike `DsssReceiver` (which goes
 * straight from an acquisition hit to tracking with the hit's own
 * coarse Doppler estimate), this object inserts a REFINING stage
 * between the two, closing a low-Es/N0 pull-in gap the coarse-only
 * estimate leaves at large static Doppler offsets:
 *
 *   - **searching** (`get_tracking() == 0 && get_refining() == 0`):
 *     samples feed the embedded `Acquisition`. On a hit,
 *     `acq_build_handoff()` seeds the refine-stage chain (a FROZEN
 *     carrier derotation -- `costas_wipeoff()` at the coarse estimate,
 *     `costas_update()` never called, the direct C equivalent of the
 *     Python prototype's `freeze_carrier=True` -- feeding a collection
 *     `Dll` whose `dll_lookback_segments(refine_max_error_db)` windows
 *     OVERSAMPLE each epoch with coherent integrate-and-dump dumps -- the
 *     asynchronous data's residual carrier rides a ~symbol_rate-wide
 *     spectrum that a single per-epoch dump would undersample and alias
 *     (see the `refine_max_error_db` doc comment on
 *     `async_dsss_receiver_create()`), then a `RateConverter` to
 *     `CarrierAcquisition`'s own
 *     operating rate, then `CarrierAcquisition` itself), and the
 *     unconsumed tail of the same call is handed straight to it.
 *   - **refining** (`get_refining() == 1`): samples feed the refine-stage
 *     chain. Every call, `CarrierAcquisition`'s own `ready`/give-up state
 *     is checked; once either fires, the live tracking chain is built
 *     FRESH (mirroring the already-learned "rebuild fresh, don't nudge in
 *     place" lesson) -- seeded from the ORIGINAL handoff chip phase (not
 *     wherever the refine-stage `Dll` drifted to) and the refined (or, on
 *     a give-up, unrefined) Doppler estimate -- and the object
 *     transitions to tracking.
 *   - **tracking** (`get_tracking() == 1`): the refined carrier estimate is
 *     UNFROZEN into a live pre-despread carrier loop
 *     (`costas_wipeoff`/`costas_update`) -> `Dll` -> `RateConverter` ->
 *     `MpskReceiver` -- the "track" leg of coarse -> freeze -> refine ->
 *     unfreeze/track. `costas_update()` runs once per code period, driven by
 *     a NON-DATA-AIDED (squaring) discriminator over that period's coherent-
 *     I&D partials (`_track_period()`): a code period spans ~0.9 data symbols
 *     at SPEC's async ratio, so a transition lands inside nearly every
 *     period, and squaring is what makes the carrier error transition-robust
 *     (a decision-directed sign-aligned combine, tried first, thrashed
 *     +/-57deg and averaged to zero, so loop 1 never tracked and the
 *     post-despread MpskReceiver loop silently inherited the whole carrier +
 *     its Type-II ramp phase error). With that clean error and a bandwidth
 *     wide enough to pull the refined seed in and ride the ramp
 *     (`ASYNC_DSSS_RX_BN_CARRIER`), the pre-despread loop removes the FULL
 *     coupled Doppler (offset AND 500 Hz/s ramp), so despreading is coherent
 *     and MpskReceiver is left only a small residual. (Pure PLL -- no FLL
 *     anywhere, see the `ASYNC_DSSS_RX_BN_CARRIER` comment.)
 *
 * Both the refine and track stages share ONE carrier-wipe scratch/carry
 * buffer set (`car_wiped_buf`/`car_carry_buf`/`car_carry_len`, sized
 * `tsamps = code_len*spc`) since they never run concurrently.
 *
 * @code
 * async_dsss_receiver_state_t *rx = async_dsss_receiver_create(
 *     code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
 *     2, 2,                            // spc, m (BPSK)
 *     55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
 *                                      // doppler_uncertainty
 *     4, 8, 0,                         // segments, sps, differential
 *     0.5, 4, 14.0, 64, 8, false, 100000,  // refine_* tuning
 *     0.0);                            // carrier_freq_hz (0 = aiding off)
 * float complex syms[4096];
 * size_t n = async_dsss_receiver_steps(rx, x, x_len, syms, 4096);
 * async_dsss_receiver_destroy(rx);
 * @endcode
 */
#ifndef ASYNC_DSSS_RECEIVER_CORE_H
#define ASYNC_DSSS_RECEIVER_CORE_H

#include "RateConverter/RateConverter_core.h"
#include "acq/acq_core.h"
#include "carrier_acq/carrier_acq_core.h"
#include "cic/cic_core.h"
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "dp_state.h"
#include "hbdecim/hbdecim_core.h"
#include "lockdet/lockdet_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include "resamp/resamp_core.h"
#include "resample/resample_core.h"
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include "psd/psd_core.h"
#include "detector/detector_core.h"
#include "detection/detection_core.h"
#include "spectral/spectral_core.h"
#include "corr/corr_core.h"
#include "fft/fft_core.h"
#include "acc_trace/acc_trace_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* SPEC-derived carrier-loop bandwidth for the pre-despread Costas
   * (hardcoded, not a public constructor param, matching this object's
   * "just works" philosophy). The refine-stage's frozen carrier never
   * calls costas_update(), so its own bn/zeta are irrelevant -- only its
   * tsamps (the wipe period length) and init_norm_freq matter.
   *
   * Wider than the 0.01 first used, for two reasons the narrow value could
   * not meet: (1) PULL-IN -- the refined seed still leaves ~tens of Hz of
   * residual (PSDMF accuracy + the ramp drifting the carrier over the
   * refine->track latency), and a per-code-period PLL at 0.01 has a pull-in
   * range far under that, so the residual wraps as a zero-mean sinusoid and
   * the loop never locks; (2) DYNAMICS -- tracking SPEC's 500 Hz/s ramp with
   * a per-period Type-II loop needs the bandwidth to keep the velocity lag
   * (phase error ~ ramp/omega_n^2) small. 0.04 pulls the seed in and rides
   * the ramp with a clean (~0 deg) despread constellation; the minimum that
   * works is ~0.03 (0.02 under-pulls). Because the phase detector is
   * |P|-normalized (unit-slope), this is the true noise bandwidth, so it
   * costs some phase noise at the 0-Doppler floor -- but the ~5 dB Es/N0
   * floor is still met.
   *
   * NO FLL: the pre-despread Costas always passes bn_fll = 0 (a pure PLL).
   * The FLL cross-product frequency discriminator is far too noisy on this
   * loop's data-modulated despread-symbol input. The phase discriminator is
   * NON-DATA-AIDED (squaring the emitted coherent-I&D partials) so it is
   * immune to the async data transitions that land inside nearly every code
   * period (see _track_period()); with that clean, transition-robust error
   * the plain PLL tracks the coupled Doppler (offset AND 500 Hz/s ramp)
   * pre-despread. So the FLL is not exposed here at all, not merely
   * defaulted off. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.04
  /* Dll's own bn: the validated stable code-loop bandwidth for the
   * one-update-per-partial tracking geometry -- same value DsssReceiver's
   * own Dll uses, not dll_create()'s own default of 0.01. (A wider 0.005 was
   * tried to chase sustained code-rate Doppler, but with the FLL removed the
   * carrier-driven slips are gone and the narrower 0.002 keeps its noise
   * immunity at the low-Es/N0 floor.) */
#define ASYNC_DSSS_RX_DLL_BN 0.002

  /* Symbol-lock detector on the emitted symbols. The lock signal is the
   * BPSK phase-lock statistic (I^2 - Q^2)/(I^2 + Q^2) = cos(2*phi) per
   * symbol (locked -> +1, 45deg-rotated/noise -> 0). The lock METRIC is that
   * signal integrated proportionally to SNR -- a power-weighted running mean,
   * EMA(I^2-Q^2)/EMA(I^2+Q^2), so high-|symbol| (high-SNR) symbols dominate
   * and noisy ones contribute little -- over a dwell of LOCK_DWELL symbols
   * (>= 30). A hysteretic lockdet (lockdet_core.h) then declares `locked`
   * after LOCK_N_UP consecutive symbols with the metric >= LOCK_UP and drops
   * it after LOCK_N_DOWN below LOCK_DOWN. */
#define ASYNC_DSSS_RX_LOCK_DWELL 30u
#define ASYNC_DSSS_RX_LOCK_UP 0.5
#define ASYNC_DSSS_RX_LOCK_DOWN 0.3
#define ASYNC_DSSS_RX_LOCK_N_UP 30u
#define ASYNC_DSSS_RX_LOCK_N_DOWN 15u

  /**
   * @brief Composed receiver state.
   *
   * Every child (acq, both refine-stage children, both track-stage
   * children) is allocated at create() with a placeholder seed (phase 0,
   * no Doppler) and REBUILT (not freed to NULL) on every real transition
   * -- a fixed shape, independent of `state`, matching `DsssReceiver`'s
   * own serialization-simplicity rationale. Treat all fields as internal
   * (use the getters).
   */
  typedef struct
  {
    acq_state_t *acq;

    /* Refine stage: a frozen-carrier collection Dll feeding
     * CarrierAcquisition via a RateConverter. Rebuilt on every real
     * acquisition hit and on reset(); otherwise untouched. */
    costas_state_t         car_frozen;
    dll_state_t           *refine_dll;
    RateConverter_state_t *refine_rc;
    carrier_acq_state_t   *ca;
    size_t refine_segments; /**< dll_lookback_segments() result, cached at
                                  the hit that (re)built the refine chain
                                  -- config for refine_dll's own layout,
                                  needed again by set_state's check. */
    uint64_t refine_samples_fed; /**< Raw samples fed into the refine
                                       pipeline since it was last (re)built
                                       -- the running counter the give-up
                                       cap and the tail computation on
                                       refine->track transition both use. */
    /* Scratch, not state -- sized once per refine-chain (re)build (no
     * allocation in the steps() hot path): refine_dll's own emitted
     * partials (capacity refine_segments), then refine_rc's resampled
     * output (capacity refine_segments*refine_rc->rate + margin). */
    float complex *refine_dll_out_buf;
    size_t         refine_dll_out_cap;
    float complex *refine_rc_out_buf;
    size_t         refine_rc_out_cap;

    /* Track stage: DsssReceiver's own composition. costas_init()'s tsamps is
     * one whole code period, and the pre-despread carrier loop updates once
     * per period from a non-data-aided (squaring) combine of that period's
     * coherent-I&D partials (see _track_period() in the .c file). */
    costas_state_t         car;
    dll_state_t           *dll;
    RateConverter_state_t *rc;
    mpsk_receiver_state_t *rx;

    /* Shared carrier-wipe scratch/carry -- refine and track stages never
     * run concurrently and both wipe in whole `tsamps`-sample periods, so
     * one buffer set serves either. */
    size_t         tsamps; /**< code_len*spc -- one code period, samples. */
    float complex *car_wiped_buf;
    float complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code (same rationale as DsssReceiver's
     * own copy -- neither acq_create_continuous() nor dll_create()'s
     * borrow-vs-copy semantics are part of their public contract). */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across every chain rebuild. */
    size_t spc;
    int    m;
    int    differential;
    size_t segments; /**< Live-tracking Dll's own segments -- distinct
                           from refine_segments above (see the module
                           docstring / dll_lookback_segments()'s own doc
                           on the WINDOWS vs TRACK_WINDOWS split). */
    size_t sps;      /**< MpskReceiver's own samples/symbol.             */
    int    n;        /**< MpskReceiver's own carrier-arm count.          */
    double chip_rate;
    double symbol_rate;
    double cn0_dbhz; /**< Design C/N0 -- feeds both Acquisition's own
                           sizing and (derated) CarrierAcquisition's
                           design_snr on every refine-chain (re)build. */
    double pfa;      /**< Also CarrierAcquisition's own pfa.             */
    double pd;       /**< Also CarrierAcquisition's own pd.              */

    /* Refine-stage tuning, fixed at construction (see objects/
     * async_dsss_receiver.toml for the rationale behind each default --
     * all mirror freq_refine.refine_seed_carrier_acq()'s own already-
     * validated defaults verbatim). */
    double refine_max_error_db;
    size_t refine_samples_per_symbol;
    double refine_design_margin_db;
    size_t refine_n_fft;
    size_t refine_zero_pad;
    bool   refine_sequential;
    size_t refine_max_n_blocks;
    double carrier_freq_hz; /**< nominal RF carrier, Hz; > 0 enables the
                                 carrier->code rate aiding, 0 = off (config,
                                 not running state -- restored by create). */

    int state; /**< 0 = searching, 1 = refining, 2 = tracking.           */
    double   seed_chip_phase;     /**< Original handoff chip phase --
                                        reused verbatim to seed the FRESH
                                        live-tracking Dll, not wherever the
                                        refine-stage Dll drifted to.      */
    double   seed_doppler_hz_est; /**< Original (unrefined) handoff Doppler
                                        estimate.                        */
    double   doppler_hz_est;      /**< Current best estimate: == seed_
                                        doppler_hz_est while refining, the
                                        CarrierAcquisition-refined value
                                        once tracking.                   */
    double   cn0_dbhz_est;        /**< Cached from the winning acquisition
                                        hit.                              */
    uint64_t samples_fed;         /**< Running total handed to acq_push()
                                        so far -- diffed against
                                        acq->samples_consumed right after a
                                        hit, same technique DsssReceiver's
                                        own steps() uses.                 */

    /* Symbol-lock detector running state (see the ASYNC_DSSS_RX_LOCK_*
     * defines). lock_num/lock_den are the power-weighted EMAs of I^2-Q^2 and
     * I^2+Q^2; lock_metric = lock_num/lock_den = cos(2*phi); sym_lockdet is
     * the hysteretic up/down declare. Reset when the track chain is (re)built
     * (fresh lock per pass). */
    double          lock_num;
    double          lock_den;
    double          lock_metric;
    double          lock_alpha; /**< EMA coeff = 1/dwell (dwell >= 30).    */
    lockdet_state_t sym_lockdet;
  } async_dsss_receiver_state_t;

  /**
   * @brief Create an AsyncDsssReceiver in the searching state.
   *
   * Only `code`/`chip_rate`/`symbol_rate` describe the signal itself.
   * `refine_*` parameters mirror `freq_refine.refine_seed_carrier_acq()`'s
   * own already-validated defaults (see `objects/async_dsss_receiver.toml`
   * for the rationale behind each one) -- a power user can override, but
   * the defaults are sized to close SPEC's own 4-5dB/500Hz-s combined
   * gating scenario as-is.
   *
   * @param code                       Spreading code (chip values).
   * @param code_len                   Chips in `code`.
   * @param chip_rate                  Chip rate, Hz. Required.
   * @param symbol_rate                Data-symbol rate, Hz. Required.
   * @param spc                        Samples/chip; default 2.
   * @param m                          PSK order, 2/4/8; default 2 (BPSK).
   * @param cn0_dbhz                   Design C/N0, dB-Hz; default 55.0 --
   *                                   feeds BOTH the embedded Acquisition's
   *                                   own sizing AND (derated by
   *                                   `refine_design_margin_db`)
   *                                   CarrierAcquisition's `design_snr`.
   * @param pfa                        Acquisition false-alarm target;
   *                                   default 1e-3. Also CarrierAcquisition's
   *                                   own `pfa`.
   * @param pd                         Acquisition detection-probability
   *                                   target; default 0.9. Also
   *                                   CarrierAcquisition's own `pd`.
   * @param doppler_uncertainty        One-sided Doppler search half-range,
   *                                   Hz; default 100.0.
   * @param segments                   Live-tracking Dll's own segments;
   *                                   default 4.
   * @param sps                        MpskReceiver's samples/symbol;
   *                                   default 8.
   * @param differential                MpskReceiver's differential demap;
   *                                   default 0 (coherent).
   * @param refine_max_error_db        Max tolerable async-lookback
   *                                   correlation-power loss driving the
   *                                   refine-stage collection Dll's
   *                                   coherent-I&D window count via
   *                                   dll_lookback_segments(). Oversampling
   *                                   the epoch is required for the
   *                                   asynchronous data: the residual
   *                                   carrier rides a ~symbol_rate-wide
   *                                   data-modulated spectrum, so segments>1
   *                                   (default yields 11 at tsamps=2046)
   *                                   samples it above Nyquist; segments=1
   *                                   undersamples and aliases it. Default
   *                                   0.5.
   * @param refine_samples_per_symbol  CarrierAcquisition's own operating
   *                                   rate = this * symbol_rate; default 4.
   * @param refine_design_margin_db    Empirical derating of cn0_dbhz before
   *                                   CarrierAcquisition's design_snr;
   *                                   default 14.0.
   * @param refine_n_fft               CarrierAcquisition's own block size;
   *                                   default 64.
   * @param refine_zero_pad            CarrierAcquisition's own zero_pad;
   *                                   default 8.
   * @param refine_sequential          CarrierAcquisition's own sequential
   *                                   mode; default false -- sequential
   *                                   mode's early per-block test fires
   *                                   on far too little averaging at
   *                                   SPEC's own Es/N0 floor (confirmed:
   *                                   as few as 4 blocks, 150-200+ Hz
   *                                   off); false waits the full
   *                                   design_snr-derived dwell_target,
   *                                   matching freq_refine.refine_seed_
   *                                   carrier_acq()'s own validated
   *                                   default.
   * @param refine_max_n_blocks        CarrierAcquisition's own give-up cap
   *                                   in sequential mode; default 100000.
   * @param carrier_freq_hz            Nominal RF carrier frequency, Hz,
   *                                   enabling carrier->code aiding; 0.0
   *                                   (default) = off. When > 0, the coupled
   *                                   code-rate Doppler
   *                                   (carrier_offset/carrier_freq) is fed to
   *                                   the tracking Dll via dll_set_rate_aid()
   *                                   so the code loop rides a dilated clock
   *                                   the discriminator alone can't pull in
   *                                   at low SNR. Set to the receiver's own
   *                                   downlink RF frequency for a
   *                                   physically-coupled Doppler capture.
   * @return Heap-allocated state, or NULL on invalid args / allocation
   *         failure.
   */
  async_dsss_receiver_state_t *async_dsss_receiver_create (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
      double pd, double doppler_uncertainty, size_t segments, size_t sps,
      int differential, double refine_max_error_db,
      size_t refine_samples_per_symbol, double refine_design_margin_db,
      size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz);

  /** @brief Destroy a receiver and release every child.
   *  @param state May be NULL. */
  void async_dsss_receiver_destroy (async_dsss_receiver_state_t *state);

  /**
   * @brief Return to the searching state.
   * @param state Must be non-NULL.
   */
  void async_dsss_receiver_reset (async_dsss_receiver_state_t *state);

  size_t async_dsss_receiver_steps_max_out (async_dsss_receiver_state_t *state);

  /**
   * @brief Stream raw cf32 samples; emit demodulated symbols once tracking.
   * @param state    Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity.
   * @return Number of symbols written (0 while searching/refining, or
   *         while tracking with not yet a full symbol's worth of input).
   */
  size_t async_dsss_receiver_steps (async_dsss_receiver_state_t *state,
                                    const float complex *x, size_t x_len,
                                    float complex *out, size_t max_out);

  /** @brief Pin the embedded Acquisition's search grid directly. */
  int async_dsss_receiver_configure_search_raw (
      async_dsss_receiver_state_t *state, size_t doppler_bins,
      size_t n_noncoh);

  /** @brief Re-tune the live-tracking Dll's code-lock detector directly. */
  void async_dsss_receiver_configure_lock_raw (
      async_dsss_receiver_state_t *state, double up_thresh,
      double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
      uint32_t n_down);

  /** @brief Pin the live-tracking despread/resample/demod grid directly. */
  int async_dsss_receiver_configure_chain_raw (
      async_dsss_receiver_state_t *state, size_t segments, size_t sps,
      int n);

  int    async_dsss_receiver_get_tracking (
      const async_dsss_receiver_state_t *state);
  int    async_dsss_receiver_get_refining (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_doppler_hz (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_cn0_dbhz_est (
      const async_dsss_receiver_state_t *state);
  size_t async_dsss_receiver_get_segments (
      const async_dsss_receiver_state_t *state);
  size_t async_dsss_receiver_get_sps (const async_dsss_receiver_state_t *state);
  int    async_dsss_receiver_get_n (const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_chip_phase (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_code_rate (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_lock (
      const async_dsss_receiver_state_t *state);
  double async_dsss_receiver_get_norm_freq (
      const async_dsss_receiver_state_t *state);
  /** @brief Live carrier loop-filter output = NCO frequency command
   * (cycles/sample of the MpskReceiver output rate). Its mean tracks a
   * Doppler ramp with no lag (unlike get_norm_freq's integrator estimate);
   * its variance is the carrier loop stress. */
  double async_dsss_receiver_get_nco_freq (
      const async_dsss_receiver_state_t *state);
  /** @brief Binary carrier-lock flag from the loop's hysteretic (up/down
   * verify-counted) lock detector — the de-chattered lock indicator, unlike
   * the raw `lock` metric. */
  int async_dsss_receiver_get_locked (
      const async_dsss_receiver_state_t *state);
  /** @brief Binary code-lock flag from the live tracking Dll's own
   * verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
   * despreading" lock, de-chattered by up/down hysteresis. */
  int async_dsss_receiver_get_code_locked (
      const async_dsss_receiver_state_t *state);
  /** @brief Pre-despread Costas phase discriminator (rad): the residual
   * carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling. */
  double async_dsss_receiver_get_car_last_error (
      const async_dsss_receiver_state_t *state);
  /** @brief LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency
   * command, cycles/sample of the front-end (chip_rate*spc) rate. */
  double async_dsss_receiver_get_car_nco_freq (
      const async_dsss_receiver_state_t *state);
  /** @brief MpskReceiver carrier phase discriminator (rad): the residual
   * carrier phase LOOP 2 (post-despread) is not nulling. */
  double async_dsss_receiver_get_mpsk_last_error (
      const async_dsss_receiver_state_t *state);
  /** @brief Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2)
   * = cos(2*phi) over the emitted symbols (locked -> ~+1). Drives `locked`. */
  double async_dsss_receiver_get_lock_metric (
      const async_dsss_receiver_state_t *state);
  /** @brief The lock-metric declare threshold `locked` latches above (the
   * lockdet up_thresh); exposed alongside lock_metric for engineering debug. */
  double async_dsss_receiver_get_lock_threshold (
      const async_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car_frozen + refine_dll + refine_rc + ca + car +
   * dll + rc + rx, always all nine (a fixed shape, DsssReceiver's own
   * rationale). segments/sps/n/refine_segments are the layout key. */

  typedef struct
  {
    uint8_t  state;
    uint8_t  _pad[7];
    double   seed_chip_phase;
    double   seed_doppler_hz_est;
    double   doppler_hz_est;
    double   cn0_dbhz_est;
    uint64_t segments;
    uint64_t sps;
    uint64_t n;
    uint64_t refine_segments;
    uint64_t refine_samples_fed;
    uint64_t car_carry_len;
    double   lock_num;  /**< symbol-lock EMAs + hysteretic detector: the */
    double   lock_den;  /**< running state that survives a checkpoint    */
    double   lock_metric;         /**< (config -- alpha, thresholds -- is */
    lockdet_state_t sym_lockdet;  /**< restored by create()).             */
  } async_dsss_receiver_extra_t;

#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 2u

  size_t async_dsss_receiver_state_bytes (
      const async_dsss_receiver_state_t *state);
  void   async_dsss_receiver_get_state (
      const async_dsss_receiver_state_t *state, void *blob);
  int async_dsss_receiver_set_state (async_dsss_receiver_state_t *state,
                                     const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_CORE_H */
