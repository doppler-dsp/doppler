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
 *   - **tracking** (`get_tracking() == 1`): samples feed a pre-despread
 *     carrier loop (`costas_wipeoff`/`costas_update`) -> `Dll` ->
 *     `RateConverter` -> `MpskReceiver`, the SAME per-code-period cadence
 *     `DsssReceiver` already validates (`costas_update()` called once per
 *     period on a sign-aligned sum of that period's `dll_steps()`
 *     partials -- `_track_period()`'s own doc comment). A per-partial
 *     cadence (`costas_update()` once per emitted partial, mirroring the
 *     Python prototype's `car_update_windows=True`) was tried first and
 *     found NOT to track SPEC's own 500Hz/s ramp at this object's real
 *     operating geometry -- see `_build_track_chain()`'s own comment for
 *     the loop-dynamics reason (`k_fll`'s fixed per-call gain vs. a 4x
 *     shorter inter-prompt interval).
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

  /* SPEC-derived defaults for the pre-despread carrier loop -- the same
   * values DsssReceiver's own DSSS_RX_BN_CARRIER/DSSS_RX_BN_FLL use
   * (hardcoded, not public constructor params, matching this object's
   * "just works" philosophy). The refine-stage's frozen carrier never
   * calls costas_update(), so its own bn/zeta/bn_fll are irrelevant --
   * only its tsamps (the wipe period length) and init_norm_freq matter. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.01
#define ASYNC_DSSS_RX_BN_FLL 0.03
  /* Dll's own bn: this story's validated stable loop bandwidth for a
   * one-update-per-code-epoch (or, for tracking, one-update-per-partial)
   * geometry -- same value DsssReceiver's own Dll uses, not
   * dll_create()'s own default of 0.01. */
#define ASYNC_DSSS_RX_DLL_BN 0.002

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

    /* Track stage: DsssReceiver's own composition, with ONE difference --
     * costas_init()'s tsamps here is the PER-PARTIAL interval
     * (code_len*spc/segments), and costas_update() is called once per
     * dll_steps()-emitted partial directly (see _track_carrier_dll() in
     * the .c file) -- no combine-and-sign-align step. */
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
  } async_dsss_receiver_extra_t;

#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 1u

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
