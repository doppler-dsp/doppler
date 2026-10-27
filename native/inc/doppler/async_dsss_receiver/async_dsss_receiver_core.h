/**
 * @file async_dsss_receiver_core.h
 * @brief Composed continuous DSSS receiver: Acquisition -> hand-off ->
 *        CarrierAcquisition refine -> Costas/Dll/RateConverter/
 *        MpskReceiver track, one object.
 *
 * Unlike `DsssReceiver`, which goes straight from an acquisition hit to
 * tracking on the hit's coarse Doppler estimate, this object refines the
 * estimate between the two, closing the low-Es/N0 pull-in gap a coarse-only
 * estimate leaves at large static Doppler offsets. Its design of record is
 * docs/design/async-dsss-parallel.md: N of these receivers, always on,
 * each searching and tracking by itself under a controller that only
 * watches. The despreader, the states and the release under it are
 * docs/design/async-dsss-receiver.md. Its states are the
 * `ASYNC_DSSS_RX_*` values below, also `status().state`:
 *
 *   - **searching** (the start): samples feed the
 *     embedded `Acquisition`. A hit is made into a seed by
 *     `dp_acq_build_handoff()` and taken through
 *     `dp_async_dsss_receiver_seed()`, and the unconsumed tail of the same
 *     call goes straight on. The refine chain is a FROZEN carrier wipe
 *     (`costas_wipeoff()` at the coarse estimate, `costas_update()` never
 *     called) feeding a collection `Dll` whose
 *     `dp_dll_lookback_segments(refine_max_error_db)` windows oversample
 *     each epoch -- the asynchronous data's residual carrier rides a
 *     ~symbol_rate-wide spectrum a single per-epoch dump would alias (see
 *     `refine_max_error_db` on `dp_async_dsss_receiver_create()`) -- then a
 *     `RateConverter` to `CarrierAcquisition`'s rate, then
 *     `CarrierAcquisition` itself.
 *   - **refining**: samples feed the refine chain until `CarrierAcquisition`
 *     is ready or gives up; then the live tracking chain is built FRESH,
 *     from the hand-off's chip phase advanced by the clock dilation over the
 *     refine (not wherever the refine-stage `Dll` drifted) and the refined
 *     -- on a give-up, the seed's -- Doppler.
 *   - **tracking**: the pre-despread carrier loop (`costas_wipeoff`/
 *     `costas_update`) -> `Dll` -> `RateConverter` -> `MpskReceiver`. The
 *     carrier loop updates once per code period from a NON-DATA-AIDED
 *     (squaring) discriminator over that period's coherent-I&D partials
 *     (`adr_track_period()`): a code period spans ~0.9 data symbols at
 *     the specification's ratio, so a transition lands inside nearly every period, and (docs/design/async-dsss-receiver.md)
 *     squaring is what makes the error transition-robust. At
 *     `ASYNC_DSSS_RX_BN_CARRIER` the loop pulls the refined seed in and
 *     removes the full coupled Doppler (offset and 500 Hz/s ramp) before
 *     despreading, so `MpskReceiver` is left a small residual. A pure PLL:
 *     no FLL.
 *   - **lost**: the emitter is gone. Without `reacquire`, entered from
 *     tracking when BOTH lock flags have been down, without a break, for
 *     longer than `lost_confirm_s` (docs/design/async-dsss-receiver.md §10,
 *     §11.2); the loops stop and samples are discarded until `reset()`.
 *     `lost_confirm_s = 0` (the default) never enters
 *     it. With `reacquire` the release is symbol lock alone and returns the
 *     receiver to searching instead, so it never reads lost
 *     (docs/design/async-dsss-parallel.md §3). While the confirm interval
 *     runs the
 *     loops HOLD what they settled on with both flags up rather than run on
 *     noise (dp_dll_set_coast(); docs/design/async-dsss-receiver.md §10;
 *     docs/design/async-dsss-receiver-measurements.md §12.19, doppler#1271),
 *     so a departed emitter's receiver stands where the emitter left it, and
 *     neither sweeps onto a neighbour's code nor restarts its release clock on
 *     one it followed. One flag down is a degrade: reported, and the loops
 *     run, which is what rides out two live emitters crossing each other's
 *     code phase.
 *
 * `dp_async_dsss_receiver_seed()` hands a searching receiver a detection
 * from outside: the record its own hit would have produced (a hit is a seed
 * the object makes for itself). It refuses on a receiver that already holds
 * one, so a hold is never silently overwritten.
 *
 * The refine and track stages share ONE carrier-wipe scratch/carry buffer
 * set (`car_wiped_buf`/`car_carry_buf`/`car_carry_len`, sized
 * `tsamps = code_len*spc`); they never run concurrently. The carry holds
 * 0..tsamps-1 live samples (a full period is processed at once, and
 * set_state refuses tsamps); the blob packs its fixed tsamps region last,
 * the live samples then zeros.
 *
 * @code
 * dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create(
 *     code, code_len, 3.0e6, 2100.0,   // chip_rate, symbol_rate
 *     2, 2,                            // spc, m (BPSK)
 *     55.0, 1e-3, 0.9, 100.0,          // cn0_dbhz, pfa, pd,
 *                                      // doppler_uncertainty
 *     4, 8, 0,                         // segments, sps, differential
 *     0.5, 4, 14.0, 64, 8, false, 100000,  // refine_* tuning
 *     0.0,                             // carrier_freq_hz (0 = aiding off)
 *     0.0,                             // lost_confirm_s (0 = never lost)
 *     false);                          // reacquire (off: lost waits)
 * float _Complex syms[4096];
 * size_t n = dp_async_dsss_receiver_steps(rx, x, x_len, syms, 4096);
 * dp_async_dsss_receiver_destroy(rx);
 * @endcode
 */
#ifndef DP_ASYNC_DSSS_RECEIVER_CORE_H
#define DP_ASYNC_DSSS_RECEIVER_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/acq/acq_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/costas/costas_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_state.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/mpsk_receiver/mpsk_receiver_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/dp_complex.h"
#include <stdbool.h>
#include <stddef.h>
#include "doppler/psd/psd_core.h"
#include "doppler/detector/detector_core.h"
#include "doppler/detection/detection_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/corr/corr_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* The pre-despread Costas's loop bandwidth (not a constructor parameter).
   * The refine stage's frozen carrier never calls costas_update(), so its
   * own bn/zeta are irrelevant -- only its tsamps and init_norm_freq matter.
   *
   * 0.04 is set by pull-in and dynamics, not jitter: the refined seed
   * leaves a residual the loop must pull in, and tracking the specification's 500 Hz/s
   * ramp with a per-period Type-II loop needs the bandwidth to keep the
   * velocity lag (~ramp/omega_n^2) small. It rides the ramp with a clean
   * despread constellation; about 0.03 is the least that works. The phase
   * detector is |P|-normalized, so this is the true noise bandwidth, and
   * the ~5 dB Es/N0 floor is still met.
   *
   * The pull-in range is measured: native/validation/costas_pullin.c sweeps
   * it in multiples of the `bn/m` bound (`m = 2`, the squaring
   * discriminator), its spot check on every push (CI's sweep job, `make
   * test-sweep`). At bn = 0.04 over a 2046-sample code period the bound is
   * 9.775e-6 cycles/sample, 60 Hz at the specification's 6.138 MHz front end; the loop
   * acquires 100% inside it on both signs, is reliable to 2x and dead by 4x.
   * The receiver's hand-off residual was measured at -54..+375 Hz, routinely
   * 2-6x the bound (doppler#982): check the hand-off against the bound
   * before widening bn.
   *
   * No FLL: the cross-product frequency discriminator is far too noisy on
   * this loop's data-modulated input, and the squaring phase discriminator
   * is immune to the transitions, so the plain PLL tracks the coupled
   * Doppler. The FLL is not exposed at all. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.04
/** The pre-despread carrier loop's acquisition bound, Hz: `bn / m` cycles
 *  per sample over its one-code-period update (`m = 2`, the squaring
 *  discriminator), `native/validation/costas_pullin.c`'s measured 100%
 *  line -- reliable to twice it, dead by four. 97.8 Hz at 5 Mcps over
 *  Gold-1023. */
#define ASYNC_DSSS_RX_CARRIER_PULLIN_HZ(chip_rate, code_len)                  \
  (ASYNC_DSSS_RX_BN_CARRIER * (chip_rate) / (2.0 * (double)(code_len)))
  /* The Dll's bn, per epoch whatever the steer rate (the symbol-period aid
   * steers once per symbol and keeps bn per epoch): DsssReceiver's value,
   * narrower than dp_dll_create()'s default 0.01 for noise immunity at the
   * low-Es/N0 floor. */
#define ASYNC_DSSS_RX_DLL_BN 0.002
  /* The tracking Dll's early-late spacing, chips. */
#define ASYNC_DSSS_RX_DLL_SPACING 0.5
  /* The damping of both Dlls this receiver builds, the tracker and the
   * refine collector, and the collector's early-late spacing, chips. Named
   * once because create() checks the very arguments each build site passes
   * (dp_dll_params_ok(), doppler#2103). */
#define ASYNC_DSSS_RX_DLL_ZETA 0.707
#define ASYNC_DSSS_RX_REFINE_DLL_SPACING 0.5
  /* The demodulator, MpskReceiver, as the track chain builds it: its
   * carrier loop's bn (symbol-rate normalised) and damping, its timing
   * loop's bn (chosen on steady-state EVM, see the build site in
   * async_dsss_receiver_core.c), its lock threshold, and MpskReceiver's
   * default RRC arguments, which the integrate-and-dump pulse does not
   * read. Named once so a harness that builds the same demodulator alone
   * (native/validation/async_dsss_receiver_loss.c) cannot drift from it. */
#define ASYNC_DSSS_RX_DEMOD_RRC_BETA 0.35
#define ASYNC_DSSS_RX_DEMOD_RRC_SPAN 8
#define ASYNC_DSSS_RX_DEMOD_BN_CARRIER 0.01
#define ASYNC_DSSS_RX_DEMOD_ZETA 0.707
#define ASYNC_DSSS_RX_DEMOD_BN_TIMING 0.005
#define ASYNC_DSSS_RX_DEMOD_LOCK_THRESH 0.3

/** Default floor on the refine's dwell, blocks: the floor dwell of
 *  docs/design/async-dsss-receiver-measurements.md §12.10 (77 Hz of
 *  estimate noise at 45 dB-Hz); see
 *  dp_async_dsss_receiver_set_refine_min_blocks(). */
#define ASYNC_DSSS_RX_REFINE_MIN_BLOCKS 7u

/** Peaks the searcher reports per dwell (dp_acq_set_max_peaks()): the list
 *  the receiver filters by exclusion, de-duplicates and ranks
 *  (dp_async_dsss_receiver_set_rank()). A rank is below this. */
#define ASYNC_DSSS_RX_MAX_PEAKS 16u

/** Most code phases dp_async_dsss_receiver_set_exclusions() holds: one per
 *  signal another receiver tracks. */
#define ASYNC_DSSS_RX_MAX_EXCLUSIONS 64u

/* Symbol-lock detector on the emitted symbols. The lock signal is the
 * BPSK phase-lock statistic (I^2 - Q^2)/(I^2 + Q^2) = cos(2*phi) per
 * symbol (locked -> +1, 45deg-rotated/noise -> 0). The lock METRIC is that
 * signal integrated proportionally to SNR -- a power-weighted running mean,
 * EMA(I^2-Q^2)/EMA(I^2+Q^2), so high-|symbol| (high-SNR) symbols dominate
 * and noisy ones contribute little -- over a dwell of LOCK_DWELL symbols
 * (>= 30). A hysteretic lockdet (lockdet_core.h) then declares `locked`
 * after LOCK_N_UP consecutive symbols with the metric strictly above
 * LOCK_UP, and drops it after LOCK_N_DOWN below LOCK_DOWN. */
#define ASYNC_DSSS_RX_LOCK_DWELL 30u
#define ASYNC_DSSS_RX_LOCK_UP 0.5
#define ASYNC_DSSS_RX_LOCK_DOWN 0.3
#define ASYNC_DSSS_RX_LOCK_N_UP 30u
#define ASYNC_DSSS_RX_LOCK_N_DOWN 15u

/* The state machine's values (`state` below, also the first byte of a
 * serialized blob's extra record). Searching -> refining -> tracking, and
 * seed() puts a searching receiver at refining; lost is reached from tracking by the release rule and left
 * only by reset(). With reacquire the release skips lost and goes straight
 * back to searching. */
/* SSOT: enum=async_dsss_rx_state */
typedef enum
{
  ASYNC_DSSS_RX_SEARCHING = 0, /**< the embedded search is running.        */
  ASYNC_DSSS_RX_REFINING  = 1, /**< the refine stage.                      */
  ASYNC_DSSS_RX_TRACKING  = 2, /**< the live chain holds the signal.       */
  /* 3 was the deleted cell mode's idle. The values are persisted (a blob,
     a telemetry capture), so it stays reserved: no state reaches it, and a
     blob that names it is refused. */
  ASYNC_DSSS_RX_RESERVED = 3,
  ASYNC_DSSS_RX_LOST     = 4, /**< released, waiting for reset().         */
} async_dsss_rx_state_t;

  /**
   * @brief Composed receiver state.
   *
   * Every child is allocated at create() with a placeholder seed (phase 0,
   * no Doppler) and REBUILT (not freed to NULL) on every real transition:
   * a fixed shape, independent of `state`. Treat all fields as internal (use the
   * getters).
   */
  typedef struct
  {
    dp_acq_state_t *acq; /**< The embedded search. */

    /* Refine stage: a frozen-carrier collection Dll feeding
     * CarrierAcquisition via a RateConverter. Rebuilt on every real
     * acquisition hit and on reset(); otherwise untouched. */
    dp_costas_state_t         car_frozen;
    dp_dll_state_t           *refine_dll;
    dp_RateConverter_state_t *refine_rc;
    dp_carrier_acq_state_t   *ca;
    size_t refine_segments; /**< dp_dll_lookback_segments() result,
                                  computed once at create: config for
                                  refine_dll's layout, needed again by
                                  set_state's check. */
    uint64_t refine_samples_fed; /**< Raw samples fed into the refine
                                       pipeline since it was last (re)built.
                                       At the hand-over, fed minus consumed
                                       is the tail of the last call that
                                       goes straight on to the live chain.
                                       The clock dilation is taken over the
                                       consumed count, from the refine's
                                       blocks, not over this. */
    uint64_t refine_periods;     /**< Diagnostic: code periods the refine
                                      stage processed since its chain was
                                      built. It stops at the period the
                                      carrier estimate decides in, so it is
                                      the same whatever the block size
                                      (#2395). Not in the state blob: it
                                      changes nothing, and a restore
                                      restarts it. */
    /* Scratch, not state -- sized once per refine-chain (re)build (no
     * allocation in the steps() hot path): refine_dll's own emitted
     * partials (capacity refine_segments), then refine_rc's resampled
     * output (capacity refine_segments*refine_rc->rate + margin). */
    float _Complex *refine_dll_out_buf;
    size_t         refine_dll_out_cap;
    float _Complex *refine_rc_out_buf;
    size_t         refine_rc_out_cap;

    /* Track stage: DsssReceiver's own composition. dp_costas_init()'s tsamps is
     * one whole code period, and the pre-despread carrier loop updates once
     * per period from a non-data-aided (squaring) combine of that period's
     * coherent-I&D partials (see adr_track_period() in the .c file). */
    dp_costas_state_t         car;
    dp_dll_state_t           *dll;
    dp_RateConverter_state_t *rc;
    dp_mpsk_receiver_state_t *rx;
    /* Scratch, not state -- sized at every track-chain (re)build and never
     * in the steps() hot path (#1192, docs/design/async-dsss-receiver.md
     * §11.5): the live
     * Dll's partials for ONE code period, then rc's resampled output for
     * it. The chain runs a period at a time, so both are fixed by tsamps
     * and rc->rate; they only ever grow. */
    float _Complex *track_dll_out_buf;
    size_t         track_dll_out_cap;
    float _Complex *track_rc_out_buf;
    size_t         track_rc_out_cap;

    /* Shared carrier-wipe scratch/carry -- refine and track stages never
     * run concurrently and both wipe in whole `tsamps`-sample periods, so
     * one buffer set serves either. */
    size_t         tsamps; /**< code_len*spc -- one code period, samples. */
    float _Complex *car_wiped_buf;
    float _Complex *car_carry_buf;
    size_t         car_carry_len;

    /* Own copy of the spreading code (same rationale as DsssReceiver's
     * own copy -- neither dp_acq_create_continuous() nor dp_dll_create()'s
     * borrow-vs-copy semantics are part of their public contract). */
    uint8_t *code;
    size_t   code_len;

    /* Config carried across every chain rebuild. */
    size_t spc;
    int    m;
    int    differential;
    size_t segments; /**< The live-tracking Dll's partial correlations per
                           code period (refine_segments is the refine
                           collector's). */
    size_t sps;      /**< MpskReceiver's samples per symbol.             */
    int    n;        /**< MpskReceiver's terminal outputs per symbol
                          (`m_out`: even, 2..8).                         */
    double chip_rate;
    double symbol_rate;
    double cn0_dbhz; /**< Design C/N0 -- feeds both Acquisition's own
                           sizing and (derated) CarrierAcquisition's
                           design_snr on every refine-chain (re)build. */
    double pfa;      /**< Also CarrierAcquisition's own pfa.             */
    double pd;       /**< Also CarrierAcquisition's own pd.              */

    /* Refine-stage tuning, fixed at construction (see each on
     * dp_async_dsss_receiver_create()). */
    double refine_max_error_db;
    size_t refine_samples_per_symbol;
    double refine_design_margin_db;
    size_t refine_n_fft;
    size_t refine_zero_pad;
    bool   refine_sequential;
    size_t refine_max_n_blocks;
    size_t refine_min_blocks; /**< Floor on the refine's dwell, in blocks
                                   (default 7). The refine's detection
                                   sizing needs fewer blocks the higher the
                                   C/N0, but the estimate the carrier chain
                                   must pull in from does not sharpen with
                                   it (#1265). Set with
                                   set_refine_min_blocks(), which carries
                                   the measurement. */
    double carrier_freq_hz; /**< nominal RF carrier, Hz; > 0 enables the
                                 carrier->code rate aiding, 0 = off (config,
                                 not running state -- restored by create). */

    int state; /**< One of the ASYNC_DSSS_RX_SEARCHING .. _LOST values.  */
    double   lost_confirm_s;       /**< Config: both flags down for longer
                                        than this is "gone"; 0 = never.  */
    uint64_t lost_confirm_samples; /**< lost_confirm_s at the input rate. */
    bool     reacquire;            /**< Config: release on symbol lock alone
                                        and reset to searching, never lost
                                        (see dp_async_dsss_receiver_create()). */
    uint64_t state_samples;        /**< Running: samples fed since the
                                        current state was entered.       */
    double   code_only_s;  /**< Config: the code-only window, seconds
                                (create()'s; restored by create).        */
    double   doppler_rate; /**< Config: the searcher's rate bound, Hz/s. */
    uint64_t samples_in;   /**< Running: input samples fed since create.
                                Never reset: every receiver in a pool is
                                fed every sample from create, so this IS
                                the shared sample count the searchers'
                                dwells align to.                         */
    bool     align_pending; /**< Running: the searcher was reset and
                                 discards input to the next multiple of
                                 `coherent_bins * code_len * spc` before
                                 its next dwell starts.                  */
    size_t   rank;   /**< set_rank(): which surviving peak of a dwell the
                          search takes, 0 = the strongest.               */
    size_t   n_excl; /**< set_exclusions(): code phases held.            */
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; /**< those phases, chips,
                          in [0, code_len), the status() convention.     */
    uint64_t refused_hits;         /**< Searching hits the seed domain refused
                                        as seeds, since create or reset. In
                                        the blob with state_samples, so a
                                        resumed receiver's status matches an
                                        unbroken run's.                  */
    uint64_t both_down_samples;    /**< Running: consecutive samples fed
                                        while tracking with BOTH lock
                                        flags down -- the release clock;
                                        lost keeps it counting. With
                                        reacquire it counts the symbol
                                        lock flag alone.                 */
    int      had_lock;             /**< Running: both lock flags have been
                                        up since the hand-over. After that
                                        both down holds both loops
                                        (dp_dll_set_coast, the carrier's own
                                        hold) so a departed emitter's
                                        receiver cannot free-run onto a
                                        neighbour's code (#1271).        */
    int      car_coasting;         /**< Running: the carrier loop is held. */
    dp_costas_state_t car_held;       /**< The carrier as of the last
                                        period with both lock flags up,
                                        restored on entering the hold.    */

    double   seed_chip_phase;     /**< The seed's chip phase; the live
                                        chain starts from it advanced by
                                        the clock dilation over the
                                        refine, not from wherever the
                                        refine-stage Dll drifted.         */
    double   seed_doppler_hz_est; /**< Original (unrefined) handoff Doppler
                                        estimate.                        */
    double   doppler_hz_est;      /**< The seed's Doppler while refining;
                                        the CarrierAcquisition-refined value
                                        once tracking. A refused hand-over zeroes both, as
                                        reset() does.                    */
    double   cn0_dbhz_est;        /**< The seed's C/N0 estimate (its own
                                        hit's, or an outside seed's);
                                        nothing refreshes it.             */

    /* Symbol-lock detector running state (see the ASYNC_DSSS_RX_LOCK_*
     * defines). lock_num/lock_den are the power-weighted EMAs of I^2-Q^2 and
     * I^2+Q^2; lock_metric = lock_num/lock_den = cos(2*phi); sym_lockdet is
     * the hysteretic up/down declare. Reset when the track chain is (re)built
     * (fresh lock per pass). */
    double          lock_num;
    double          lock_den;
    double          lock_metric;
    double          lock_alpha; /**< EMA coeff = 1/dwell (dwell >= 30).    */
    dp_lockdet_state_t sym_lockdet;

    /* Telemetry: the attach REQUEST, kept because every seed destroys and
     * rebuilds the tracking chain -- the loops it holds are re-attached
     * from here (dp_async_dsss_receiver_set_telemetry). Borrowed, and never
     * in a blob: a handle to the outside world is not state. */
    dp_tlm_t *tlm;                         /**< NULL = detached.          */
    char      tlm_prefix[DP_TLM_NAME_MAX]; /**< The attach's prefix.      */
    uint32_t  tlm_decim;                   /**< The attach's decimation.  */
    int32_t   tlm_sym_lock;   /**< "<prefix>.sym.lock" id, -1 detached.   */
    int32_t   tlm_sym_locked; /**< "<prefix>.sym.locked" id, -1 detached. */
    int32_t   tlm_state;      /**< "<prefix>.state" id, -1 detached.      */
    int32_t   tlm_handover;   /**< "<prefix>.handover_hz" id, -1 detached.*/
    int32_t   tlm_refused;    /**< "<prefix>.refused" id, -1 detached.    */
  } dp_async_dsss_receiver_state_t;

  /**
   * @brief Create an AsyncDsssReceiver in the searching state.
   *
   * Only `code`/`chip_rate`/`symbol_rate` describe the signal itself. The
   * `refine_*` defaults are sized to close the specification's 4-5 dB, 500 Hz/s combined
   * scenario as they are; a power user can override each one.
   *
   * @param code                       Spreading code, one 0/1 chip per
   *                                   element (0 -> +1, 1 -> -1 BPSK; only
   *                                   the low bit is used, so pass 0/1, not
   *                                   +/-1).
   * @param code_len                   Chips in `code`.
   * @param chip_rate                  Chip rate, Hz.
   * @param symbol_rate                Data-symbol rate, Hz: finite, > 0,
   *                                   at most `chip_rate`, and at most
   *                                   2^20 Dll partials per symbol at
   *                                   `segments`.
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
   *                                   default 0 (coherent). It acts in
   *                                   MpskReceiver's bits(), which this
   *                                   object does not offer, so steps()'
   *                                   symbols are the same either way
   *                                   (#2471).
   * @param refine_max_error_db        Max tolerable async-lookback
   *                                   correlation-power loss driving the
   *                                   refine-stage collection Dll's
   *                                   coherent-I&D window count via
   *                                   dp_dll_lookback_segments(). Oversampling
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
   * @param refine_sequential          CarrierAcquisition's sequential
   *                                   mode; default false -- its early
   *                                   per-block test fires on far too
   *                                   little averaging at the specification's Es/N0
   *                                   floor (as few as 4 blocks, 150-200+
   *                                   Hz off), so false waits the full
   *                                   design_snr-derived dwell.
   * @param refine_max_n_blocks        CarrierAcquisition's give-up cap in
   *                                   sequential mode, and the cap on the
   *                                   dwell in either mode; default
   *                                   100000.
   * @param carrier_freq_hz            Nominal RF carrier frequency, Hz,
   *                                   enabling carrier->code aiding; 0.0
   *                                   (default) = off. When > 0, the coupled
   *                                   code-rate Doppler
   *                                   (carrier_offset/carrier_freq) is fed to
   *                                   the tracking Dll via dp_dll_set_rate_aid()
   *                                   so the code loop rides a dilated clock
   *                                   the discriminator alone can't pull in
   *                                   at low SNR. Set to the receiver's own
   *                                   downlink RF frequency for a
   *                                   physically-coupled Doppler capture.
   *                                   Coupled, it is above half the sample
   *                                   rate (dp_acq_carrier_freq_ok()).
   * @param lost_confirm_s             Release rule: both lock flags down,
   *                                   continuously, for longer than this
   *                                   many seconds puts the receiver in
   *                                   the lost state (see get_lost()).
   *                                   Size it past the longest fade the
   *                                   link must ride. The clock also runs
   *                                   from the first tracking sample,
   *                                   when neither flag is up yet, so a
   *                                   hand-off that never locks within the
   *                                   interval is released the same way
   *                                   as an emitter that leaves. Default
   *                                   0.0 = never -- the exit is then
   *                                   reset().
   * @param reacquire                  Release on symbol lock alone, and
   *                                   search again by itself. With it set,
   *                                   the release clock counts samples of
   *                                   tracking with the SYMBOL lock flag
   *                                   down (code lock is not consulted),
   *                                   and when it passes lost_confirm_s the
   *                                   receiver resets itself to searching
   *                                   instead of entering lost: it never
   *                                   waits for reset(). A pool of
   *                                   receivers that are always online
   *                                   uses it. The clock runs from the
   *                                   first tracking sample, as above, so
   *                                   a track that never locks (a noise
   *                                   false alarm) is released too; size
   *                                   lost_confirm_s past the pull-in time
   *                                   to symbol lock, or a slow lock is
   *                                   released before it comes. Requires a
   *                                   lost_confirm_s of at least one input
   *                                   sample, since a release that never
   *                                   fires would never search again; a
   *                                   blob in the lost state is refused by
   *                                   set_state(). Default false: the
   *                                   release rule above.
   * @param code_only_s  Seconds of code-only signal the waveform holds at
   *                     any chip phase (a symbol-free window; design
   *                     async-dsss-parallel.md §4), >= 0 and finite. The
   *                     receiver hands its searcher `code_only_epochs =
   *                     floor(code_only_s * chip_rate / code_len) - 1`
   *                     (at least 1), and the searcher's slow-time depth
   *                     follows: `coherent_bins = D`, at most
   *                     `(code_only_epochs + 1)/2`, rounded down to a
   *                     7-smooth length (dp_acq_create_continuous()).
   *                     Default 0: no window, D = 1.
   * @param doppler_rate  Doppler rate in Hz/s, >= 0 and finite, that the
   *                      slow-time depth is bounded against: at most
   *                      `f_epoch / sqrt(2 * doppler_rate)` epochs
   *                      (dp_acq_create_continuous()). Default 0: the
   *                      window is the only bound.
   * @return A new receiver, searching; NULL on an invalid argument,
   *         including reacquire with a lost_confirm_s under one sample.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> (rx.tracking, rx.refining, rx.lost)       # searching
   * (0, 0, 0)
   *
   * @endcode
   */
  dp_async_dsss_receiver_state_t *dp_async_dsss_receiver_create (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, size_t spc, int m, double cn0_dbhz, double pfa,
      double pd, double doppler_uncertainty, size_t segments, size_t sps,
      int differential, double refine_max_error_db,
      size_t refine_samples_per_symbol, double refine_design_margin_db,
      size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s, bool reacquire, double code_only_s,
      double doppler_rate);

  /** @brief Destroy a receiver and release every child.
   *  @param state May be NULL. */
  void dp_async_dsss_receiver_destroy (dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Return to the searching state.
   * Resets the embedded Acquisition and rebuilds the refine-stage chain
   * and the live tracking chain back to their
   * placeholder seed (phase 0, no Doppler). A receiver that has locked
   * cannot be "reset back to tracking the same signal," only back to
   * searching -- every object's reset() semantics in this codebase.
   * @param state Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> rx.reset()                 # abort any lock, hunt from scratch
   * >>> (rx.tracking, rx.refining, rx.chip_phase)   # all cleared
   * (0, 0, 0.0)
   *
   * @endcode
   */
  void dp_async_dsss_receiver_reset (dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Attach (or detach) a telemetry context across the receiver's
   *        loops, and register the receiver's own lifecycle and
   *        symbol-lock decision.
   *
   * The receiver reports under the prefix, once per steps() call that
   * carries samples, after the call, so on the time axis at the stamp the
   * caller set for that block (dp_tlm_set_now()):
   * - "<prefix>.state" -- where the call left the receiver, the
   *   ASYNC_DSSS_RX_* value: 0 searching, 1 refining, 2 tracking, 4 lost
   *   (status().state);
   * - "<prefix>.handover_hz" -- the Doppler the chain was built from, Hz
   *   (dp_async_dsss_receiver_get_doppler_hz()): 0 before a seed, the seed's
   *   while refining, the refined value from the hand-over on, and 0 again after a refused hand-over or a
   *   reset(), which return the receiver to searching. A record is a
   *   float, so it resolves about 0.5 Hz near 5 MHz. Not the live carrier:
   *   status().doppler_hz reads that, and ".car.freq" and ".car.nco" are
   *   the carrier loop's own view of it, in cycles a sample at the input
   *   rate, so times chip_rate * spc for Hz;
   * - "<prefix>.refused" -- the hits refused as seeds so far
   *   (status().refused_hits).
   *
   * Feed blocks of about a code period, as the examples do, and these read
   * as a time series of the lifecycle: when the search hit, how long the
   * refine took, what it handed over, when the release came.
   *
   * Each loop reports under the prefix, at its own update rate:
   * - "<prefix>.car" -- the chip-domain carrier loop
   *   (dp_costas_set_telemetry(): ".lock", ".e", ".freq", ".locked",
   *   ".nco"), once per carrier update: once a code period, except while it
   *   coasts, when loop 1 is held;
   * - "<prefix>.code" -- the code loop (dp_dll_set_telemetry(): ".e",
   *   ".rate", ".lock", ".locked", ".nco"), once per discriminator update;
   * - "<prefix>.demod" -- the symbol-domain demodulator
   *   (dp_mpsk_receiver_set_telemetry(): its lock, its carrier and timing
   *   loops, the symbols, its AGC), once per recovered symbol (the AGC's
   *   pair once per gain update);
   * - "<prefix>.acq" -- the acquisition engine
   *   (dp_acq_set_telemetry()), once per decided dwell;
   * - "<prefix>.rc" -- the rate converter between the code loop and the
   *   demodulator (dp_RateConverter_set_telemetry()). It reports only an
   *   AGC, and this receiver builds it without one, so it registers no
   *   probes;
   * - "<prefix>.sym.lock" and "<prefix>.sym.locked" -- the receiver's own
   *   symbol-lock metric (dp_async_dsss_receiver_get_lock_metric()) and
   *   its decision (dp_async_dsss_receiver_get_locked()), the one its
   *   users act on, once per emitted symbol.
   *
   * The carrier and code loops report their filters two ways: ".car.freq"
   * and ".code.rate" (and the demodulator's ".demod.car.freq") are the
   * integrators, each loop's frequency memory, which a coast holds;
   * ".car.nco", ".code.nco" and ".demod.car.nco" are the complete commands
   * the NCOs run at, the proportional term (and the code loop's carrier
   * aid) included. ".demod.sync.rate" is the timing loop's output, and ".e"
   * is each discriminator's input to its filter.
   *
   * The attachment is a REQUEST kept on the receiver: every seed rebuilds
   * the tracking chain, and the rebuilt loops are attached again under the
   * same names (so the same probe ids). Records are therefore continuous
   * across re-seeds, and a reset keeps the attachment too.
   *
   * Setup path, never hot; the context is borrowed and must outlive the
   * attachment, and the receiver must step on one thread (SPSC rules in
   * dp_tlm/dp_tlm_core.h). Passing NULL detaches every loop.
   *
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix; NULL means "rx".
   * @param decim  Emit every decim-th event on each probe; >= 1.
   * @return DP_OK, or DP_ERR_INVALID on decim 0, a prefix too long for
   *         some probe's name -- refused before anything registers on @p
   *         tlm, because the loops are first attached to a scratch context
   *         that finds the refusal -- or a probe table with no room for
   *         every probe (found the same way, from the names the scratch
   *         holds). Every refusal leaves @p tlm's table untouched and the
   *         receiver as it was: detached, or still on its previous context
   *         under its previous prefix.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.telemetry import Telemetry
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> tlm = Telemetry(1 << 12)
   * >>> rx.set_telemetry(tlm, "rx")
   * >>> sorted(p for p in tlm.probe_names if p.startswith("rx.c"))
   * ['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', 'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', 'rx.code.nco', 'rx.code.rate']
   * >>> sorted(p for p in tlm.probe_names if p.startswith(("rx.s", "rx.h",
   * ...                                                    "rx.r")))
   * ['rx.handover_hz', 'rx.refused', 'rx.state', 'rx.sym.lock', 'rx.sym.locked']
   * >>> rx.set_telemetry(None, "rx")      # detach
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_telemetry (
      dp_async_dsss_receiver_state_t *state, dp_tlm_t *tlm, const char *prefix,
      uint32_t decim);

  /**
   * @brief The output capacity steps() needs beyond the input's.
   *
   * Returns 0, which is NOT a bound. A call that completes a code
   * period carried over from earlier calls emits all of that
   * period's symbols (code_len times symbol_rate over chip_rate of
   * them), even if it brings one sample. At more than one symbol per
   * period a call can therefore emit more symbols than its `x_len`,
   * and those past `max_out` are dropped (see
   * dp_async_dsss_receiver_steps()). A true bound is #2469.
   *
   * @param state  Must be non-NULL.
   * @return 0.
   */
  size_t dp_async_dsss_receiver_steps_max_out (dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Stream raw cf32 samples; emit demodulated symbols once tracking.
   *
   * Drives the state machine. It emits nothing while
   * searching or refining (an empty return is normal, not an error): a hit
   * seeds the frozen-carrier refine chain, `CarrierAcquisition` sharpens the
   * coarse Doppler estimate, and only once it is ready (or gives up) is the
   * live tracking chain built and demodulation begins. A hit inside a call
   * hands the rest of the call straight to the refine chain, so no sample is
   * dropped there; at the refine's end, what the refine did not consume of the
   * last call goes on to the live chain. Accepts any block size; state
   * carries across calls, so a capture can be fed in frames of any length with
   * no seam. Lost (after the release rule fires) consumes the samples and emit nothing, so the feeding loop is the
   * same in every state; while tracking, the release clock runs on the two
   * lock flags after every call, or on the symbol lock flag alone with
   * `reacquire` (see `lost_confirm_s`). Tracking emits symbols whether or
   * not they are data: a noise false alarm can reach tracking and emit
   * until the release rule lets it go -- or, at the default lost_confirm_s
   * of 0, until reset() -- and symbol lock (`locked`) is what says the
   * symbols are a signal's, so read it rather than the count (#2439).
   * Under the specification's coupled offset + 500 Hz/s Doppler ramp the pre-despread
   * Costas removes the full carrier dynamics before the code loop, so the
   * recovered constellation lands cleanly on the BPSK real axis.
   *
   * @param state    Must be non-NULL.
   * @param x        Input cf32 samples.
   * @param x_len    Number of input samples.
   * @param out      Output symbols; caller provides @p max_out capacity.
   * @param max_out  Output capacity. steps() never writes past it, and the
   *                 symbols past it are dropped, not held for the next
   *                 call. @p x_len is not always enough: a call that
   *                 completes a carried code period emits the whole
   *                 period's symbols (#2469).
   * @return Number of symbols written: 0 while searching, refining or lost,
   *         or before a full symbol's
   *         worth of input.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> sf, chip, sym, spc = 1023, 3.069e6, 2700.0, 2
   * >>> fs, te, tsym = chip * spc, sf * spc, chip * spc / sym
   * >>> code = np.asarray(Gold().generate(sf)).astype(np.uint8)
   * >>> csign = np.where(code & 1, -1.0, 1.0)
   * >>> rng = np.random.default_rng(21)
   * >>> n = int(600 * tsym) + 4 * te            # 600 async BPSK symbols
   * >>> idx = np.arange(n)
   * >>> data = (rng.integers(0, 2, 604) * 2 - 1).astype(float)
   * >>> si = np.clip((idx / tsym).astype(int), 0, 603)
   * >>> t = idx / fs
   *
   * DSSS chips on a carrier sweeping at 500 Hz/s — the ramp the async
   * receiver has to track:
   *
   * >>> sig = (data[si] * csign[(idx // spc) % sf]
   * ...        * np.exp(1j * 2 * np.pi * 0.5 * 500.0 * t * t))
   * >>> cn0 = 20.0 + 10 * np.log10(sym)         # Es/N0 = 20 dB
   * >>> sigma = np.sqrt(fs / 10 ** (cn0 / 10))
   * >>> pre = 5 * te                            # noise-only lead-in
   * >>> noise = (sigma / np.sqrt(2)) * (rng.standard_normal(pre + n)
   * ...          + 1j * rng.standard_normal(pre + n))
   * >>> x = (np.concatenate([np.zeros(pre), sig]).astype(np.complex64)
   * ...      + noise.astype(np.complex64))
   * >>> rx = AsyncDsssReceiver(
   * ...     code, chip_rate=chip, symbol_rate=sym, spc=spc,
   * ...     cn0_dbhz=cn0, doppler_uncertainty=500.0)
   * >>> syms = [rx.steps(x[p:p + te]) for p in range(0, len(x) - te, te)]
   * >>> syms = np.concatenate([s for s in syms if len(s)])
   * >>> rx.tracking                  # searched, refined, now tracking
   * 1
   * >>> len(syms) > 300              # symbols recovered under the ramp
   * True
   *
   * Nearly all the energy lands on I, so the BPSK phase is resolved:
   *
   * >>> bool(np.mean(syms.real**2) > 10 * np.mean(syms.imag**2))
   * True
   *
   * @endcode
   */
  size_t dp_async_dsss_receiver_steps (dp_async_dsss_receiver_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    float _Complex *out, size_t max_out);

  /**
   * @brief Take a detection from outside and start refining from it.
   *
   * The hand-off of docs/design/async-dsss-receiver.md §2.2: the
   * three numbers a searcher's hit carries that this receiver uses --
   * `acq_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --
   * exactly as its own hit would have produced them (`steps()` calls this on
   * its own hit). `chip_phase` is the code's
   * instantaneous phase in chips, Dll's convention, at the FIRST sample of
   * the next `steps()` call; dp_acq_build_handoff() makes it from a hit's
   * lag. The refine chain is rebuilt from the seed and the state becomes
   * refining; the unconsumed tail is the caller's to feed.
   *
   * Refused (`DP_ERR_INVALID`, nothing changes) on a receiver that is not
   * waiting for one -- refining, tracking or lost -- because "assigned once"
   * is a property of the object, not of the caller's bookkeeping; `reset()`
   * releases it. Accepted while searching: an outside hit simply beats the
   * receiver's own. Also refused
   * outside the seed domain, dp_async_dsss_receiver_seed_ok().
   *
   * @param state          Must be non-NULL.
   * @param chip_phase     Code phase at the next sample, chips, in
   *                       `[0, code_len)`.
   * @param doppler_hz_est Coarse Doppler estimate, Hz (the refine stage
   *                       sharpens it).
   * @param cn0_dbhz_est   The hit's C/N0 estimate, dB-Hz; reported back by
   *                       get_cn0_dbhz_est(), unchanged until reset().
   * @return `DP_OK`, or `DP_ERR_INVALID` when refused.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
   * ...                        symbol_rate=2700.0, spc=2)
   * >>> rx.seed(chip_phase=512.25, doppler_hz_est=-1500.0,
   * ...         cn0_dbhz_est=48.0)
   * >>> (rx.refining, rx.doppler_hz, rx.cn0_dbhz_est)
   * (1, -1500.0, 48.0)
   *
   * Already assigned -- refused until reset():
   *
   * >>> rx.seed(0.0, 0.0, 48.0)      # doctest: +ELLIPSIS
   * Traceback (most recent call last):
   *     ...
   * ValueError: seed refused: ...
   * >>> rx.reset()
   *
   * A chip phase must be inside the code, `[0, code_len)`:
   *
   * >>> rx.seed(1023.0, 0.0, 48.0)   # doctest: +ELLIPSIS
   * Traceback (most recent call last):
   *     ...
   * ValueError: seed refused: ...
   *
   * @endcode
   */
  int dp_async_dsss_receiver_seed (dp_async_dsss_receiver_state_t *state,
                                double chip_phase, double doppler_hz_est,
                                double cn0_dbhz_est);

  /**
   * @brief The seed domain: whether `(chip_phase, doppler_hz_est)` is one
   *        dp_async_dsss_receiver_seed() accepts, and one a live receiver can
   *        hold.
   *
   * `chip_phase` must be finite and inside `[0, code_len)`, and
   * `doppler_hz_est` must be strictly inside `(-fs/2, fs/2)`, where `fs` is
   * `chip_rate * spc`. A Doppler at or past half the sample rate is not
   * reportable (dp_acq_carrier_freq_ok() relies on this bound), and a seed
   * there sent the hand-off's phase to NaN (doppler#2103).
   *
   * seed() refuses by this predicate, set_state() restores the seed fields
   * only inside it, the searching receiver's hit path does not seed from a
   * hit it fails, and the hand-over holds the REFINED Doppler to it too: a
   * seed in range whose residual carries it to or past fs/2 is refused at
   * the hand-over, counted in refused_hits, and the receiver searches
   * again rather than track a Doppler it cannot report. It reads only the
   * receiver's fixed config (code length, chip rate, spc), so any receiver
   * built from the same config gives the same answer.
   *
   * @param state          Must be non-NULL.
   * @param chip_phase     Code phase, chips.
   * @param doppler_hz_est Doppler estimate, Hz.
   * @return 1 inside the seed domain, 0 outside it (a NaN is outside).
   */
  int dp_async_dsss_receiver_seed_ok (const dp_async_dsss_receiver_state_t *state,
                                      double chip_phase,
                                      double doppler_hz_est);

  /**
   * @brief One consistent picture of what the receiver is doing, by value.
   *
   * The status record of docs/design/async-dsss-receiver.md §11.3: a
   * holder -- the controller of docs/design/async-dsss-parallel.md §4 --
   * reads it on demand, because the exclusions it hands the other
   * receivers are keyed on this receiver's CURRENT code phase, not on its
   * seed, and gets every field from one call between `steps()` calls,
   * rather than assembling a picture from the one-at-a-time getters. It is
   * a read of live state, not `get_state()`: the bytes triplet resumes the
   * receiver elsewhere, this
   * describes it here. Its `doppler_hz` is the live carrier (loop 1 plus
   * loop 2), where get_doppler_hz() reports the estimate the chain was
   * built from. No timestamp: the counters are in input samples, and the
   * holder, which owns the sample clock, stamps them
   * (docs/design/async-dsss-receiver.md §11.3).
   */
  typedef struct
  {
    int state; /**< ASYNC_DSSS_RX_SEARCHING .. _LOST -- where it is.      */
    double doppler_hz; /**< Where the emitter is NOW: the whole carrier
                            estimate, Hz -- loop 1's plus what loop 2 has
                            taken up beyond it (the seed while refining,
                            0 while searching, frozen where it was when
                            lost).
                            Only as good as `locked`: with the carrier
                            unlocked, loop 1 free-runs and this wanders. */
    double chip_phase; /**< Code phase, chips: the live Dll's; while
                            refining, the seed's advanced by the clock
                            dilation since (what the hand-over starts
                            from), so a refining receiver's signal can be
                            excluded too.                              */
    double code_rate;  /**< The Dll's code rate, chips per nominal chip
                            (~1.0 at lock), without the carrier aid; a
                            coasting Dll's is its held rate.            */
    double cn0_dbhz_est; /**< C/N0 estimate, dB-Hz (the seed's).            */
    int    code_locked;  /**< Presence flag: the Dll's lock detector.        */
    int    locked;       /**< Health flag: the symbol-lock detector.         */
    double lock_metric;  /**< cos(2*phi) over the symbols, drives `locked`.  */
    double lock_threshold; /**< `locked` declares strictly above this.      */
    double car_last_error; /**< Pre-despread Costas residual, rad.          */
    double mpsk_last_error; /**< Post-despread carrier residual, rad.       */
    uint64_t state_samples; /**< Input samples since `state` was entered.   */
    uint64_t both_down_samples; /**< Input samples both flags have been down
                                     without a break (the release clock);
                                     in lost it keeps counting -- samples
                                     since the flags dropped. With
                                     reacquire, the symbol lock flag's
                                     clock alone.                         */
    uint64_t refused_hits;      /**< Searching hits refused as seeds by the
                                     seed domain (a Doppler at or past fs/2),
                                     since create or reset. The search went on
                                     over each one's tail.                */
  } async_dsss_receiver_status_t;

  /**
   * @brief Read the status record, by value: the state; where the emitter
   *        is now (the live carrier), its chip phase, code rate and C/N0;
   *        both lock flags with the symbol-lock metric and threshold; both
   *        carrier residuals; and the two clocks in input samples (since
   *        the state was entered; both flags down without a break).
   *
   * Cheap and allocation-free: every field is a read of live state. The
   * one-at-a-time getters report the same fields, `doppler_hz` apart (see
   * the record); this is the face a controller reads. No timestamp: the
   * clocks are in input samples, and the holder, which owns the sample
   * clock, stamps the record.
   *
   * @param state Must be non-NULL.
   * @return The record, by value.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6,
   * ...                        symbol_rate=2700.0, spc=2)
   * >>> st = rx.status()
   * >>> (st.state, st.doppler_hz, st.code_locked, st.locked)   # searching
   * (0, 0.0, 0, 0)
   * >>> rx.seed(chip_phase=100.0, doppler_hz_est=-250.0, cn0_dbhz_est=50.0)
   * >>> st = rx.status()
   * >>> (st.state, round(st.doppler_hz, 6), st.cn0_dbhz_est)  # refining
   * (1, -250.0, 50.0)
   * >>> _ = rx.steps(np.zeros(2046, np.complex64))
   * >>> rx.status().state_samples                             # since seed
   * 2046
   *
   * @endcode
   */
  async_dsss_receiver_status_t dp_async_dsss_receiver_status (
      const dp_async_dsss_receiver_state_t *state);

  /** @brief 1 once the release rule has fired: both lock flags were down,
   * continuously, for longer than `lost_confirm_s` while tracking -- an
   * emitter that left, or a seed that never locked. The loops have stopped
   * and samples are consumed and discarded; the holder releases the
   * assignment and calls reset(). 0 in every other state, and always 0
   * with `lost_confirm_s = 0`. */
  int dp_async_dsss_receiver_get_lost (const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Pin the embedded Acquisition's search grid directly.
   * Forwards to `dp_acq_configure_search_raw()` -- the escape hatch under
   * this object's `symbol_rate`-driven auto-sizing, for a power user who
   * wants a specific `(doppler_bins, n_noncoh)`. The pin replaces the whole
   * auto-sized grid, the window-tiled search included. Only meaningful
   * while searching; the search does not run again until the next
   * `reset()`.
   *
   * @param state         Must be non-NULL.
   * @param doppler_bins  The coherent depth to pin, in `[1, reps]`: 1, as
   *                      the receiver's continuous engine has `reps = 1`.
   * @param n_noncoh      Non-coherent looks accumulated per grid cell
   *                      (1..256); more looks buys sensitivity at the cost
   *                      of dwell, replacing the auto-sized count.
   * @return 0 on success, -1 on invalid grid (see dp_acq_configure_search_raw).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> rx.configure_search_raw(doppler_bins=1, n_noncoh=16)  # pin it
   * >>> rx.refining                # still searching, on the pinned grid
   * 0
   *
   * @endcode
   */
  int dp_async_dsss_receiver_configure_search_raw (
      dp_async_dsss_receiver_state_t *state, size_t doppler_bins,
      size_t n_noncoh);

  /**
   * @brief Floor the refine's dwell at @p n_blocks, whatever the detection
   *        sizing asks
   *        (docs/design/async-dsss-receiver-measurements.md §12.16,
   *        #1265).
   *
   * CarrierAcquisition's dwell is sized for DETECTION at the derated C/N0
   * (`cn0_dbhz - refine_design_margin_db`), so it shortens as the C/N0
   * rises -- two blocks at 45 dB-Hz with the shipped margin -- while the
   * noise of the estimate it hands the tracking chain does not shorten
   * with it: 210 Hz at two blocks against a chain that pulls in from a few
   * hundred, so one hand-over in sixty landed outside and tracked the code
   * with the carrier never locked. Seven blocks (42 ms, the default and the
   * floor dwell of docs/design/async-dsss-receiver-measurements.md §12.10)
   * hold the estimate to 77 Hz. Applied to
   * the next refine chain built -- a receiver already refining keeps its
   * dwell. A mutator's value, so it travels in the state blob (#2022).
   * `n_blocks` of 0 removes the floor; the value is clamped by
   * `refine_max_n_blocks` where that cap is lower.
   *
   * @param state     Must be non-NULL.
   * @param n_blocks  The floor, blocks.
   * @return `DP_OK`.
   * @code
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
   * ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
   * >>> rx.refine_min_blocks                     # the default floor
   * 7
   * >>> rx.set_refine_min_blocks(12)
   * >>> rx.refine_min_blocks
   * 12
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_refine_min_blocks (
      dp_async_dsss_receiver_state_t *state, size_t n_blocks);

  /**
   * @brief Set the workers the searcher fans its Doppler tiles across.
   *
   * Forwards to dp_acq_set_threads() on the receiver's searcher. The
   * search is bit-identical at every count, so this changes the cost of a
   * steps() call and nothing about its output. A pool that steps its
   * receivers in parallel sets each one to 1. A mutator's value, so it
   * travels in the state blob (#2022). The blob carries the count that
   * RAN, not the request: set_threads(0) on a 20-core writer restores as
   * 20 workers on any reader, by design -- a resumed receiver runs as the
   * one that was checkpointed, and a pool's 1 is 1 everywhere. Setup path;
   * not while another thread is inside steps().
   *
   * @param state Must be non-NULL.
   * @param n     Thread count; 0 = online cores, 1 = serial.
   * @return `DP_OK`. The count actually running is
   *         dp_async_dsss_receiver_get_threads(); a searcher with one
   *         Doppler tile always runs 1.
   * @code
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
   * ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
   * >>> rx.set_threads(1)
   * >>> rx.threads
   * 1
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_threads (
      dp_async_dsss_receiver_state_t *state, int n);

  /**
   * @brief Take the @p rank-th surviving peak of a dwell, not the strongest.
   *
   * The searcher reports up to @ref ASYNC_DSSS_RX_MAX_PEAKS peaks per dwell,
   * strongest first. The receiver drops every peak within one chip
   * (circularly) of a stronger one, so one signal's neighbouring cells hold
   * one place; then every peak a stronger signal's own sidelobes could
   * explain; then every peak within one chip of an excluded code phase
   * (set_exclusions()). The survivors are ranked from 0. A dwell with no
   * peak at this rank hands nothing off and the search goes on.
   *
   * The sidelobe rule: a strong signal's list also holds its code's
   * Doppler-offset sidelobes, up to dp_acq_dsl_db() below it and hundreds
   * of chips from its phase (design acq-surface.md §8). A peak counts as a
   * second signal only if its test_stat exceeds the stronger signals'
   * bound, times that level, by more than the detector's own gate
   * (dp_acq_gate_stat()). The bound is the strongest signal's test_stat;
   * on a deep block (get_coherent_bins() > 1) it is the sum of the
   * stronger signals on the peak's own Doppler row, modulo the epoch rate,
   * when that is larger, since their fixed sidelobes can stack on one cell.
   * So the range does not narrow with the number of signals on the air
   * (doppler#2554). A sidelobe then passes only when the noise on it
   * alone would have cleared the gate, the rate the search already runs
   * at. What it costs is near-far: a second signal much more than
   * dp_acq_dsl_db() below a stronger one (-16.95 dB on the specification Gold-1023
   * search) is not taken while the stronger one is on the air
   * (async-dsss-parallel.md §3). Searchers
   * that share a stream and its block grid (get_coherent_bins()) and hold
   * ranks 0..k-1 therefore take k different signals in one dwell. Takes
   * effect at the next dwell. A mutator's value, so it travels in the state
   * blob (#2022).
   *
   * @param state Must be non-NULL.
   * @param rank  0 (the strongest, the default) to
   *              @ref ASYNC_DSSS_RX_MAX_PEAKS - 1.
   * @return `DP_OK`; `DP_ERR_INVALID` (rank unchanged) past the list.
   * @code
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
   * ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
   * >>> rx.set_rank(1)
   * >>> rx.rank
   * 1
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_rank (dp_async_dsss_receiver_state_t *state,
                                       size_t rank);

  /** @brief Which surviving peak of a dwell the search takes; 0 = the
   *  strongest. Set with dp_async_dsss_receiver_set_rank(). */
  size_t dp_async_dsss_receiver_get_rank (
      const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Code phases the search must not take: signals another receiver
   *        already tracks.
   *
   * Replaces the list. A peak within one chip, circularly, of any of them is
   * dropped from a dwell's list at any Doppler, before ranking
   * (set_rank()). Phases are chips in status().chip_phase's convention and
   * are folded into [0, code_len). An empty list (the default) excludes
   * nothing. A tracking receiver is not affected: the list applies to its
   * next search. A mutator's value, so it travels in the state blob
   * (#2022).
   *
   * @param state       Must be non-NULL.
   * @param phases      Code phases, chips; finite. May be NULL when
   *                    @p phases_len is 0.
   * @param phases_len  At most @ref ASYNC_DSSS_RX_MAX_EXCLUSIONS.
   * @return `DP_OK`; `DP_ERR_INVALID` (list unchanged) on a non-finite
   *         phase or too many.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
   * ...                        symbol_rate=1e6 / 28.0, spc=4, cn0_dbhz=60.0)
   * >>> rx.set_exclusions(np.array([2.5, 9.0]))   # 9 folds to 2
   * >>> rx.n_exclusions
   * 2
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_exclusions (
      dp_async_dsss_receiver_state_t *state, const double *phases,
      size_t phases_len);

  /** @brief Code phases set_exclusions() holds. */
  size_t dp_async_dsss_receiver_get_n_exclusions (
      const dp_async_dsss_receiver_state_t *state);

  /** @brief Workers the searcher runs, the calling thread included; 1 =
   *  serial. Set with dp_async_dsss_receiver_set_threads(). */
  int dp_async_dsss_receiver_get_threads (
      const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief The searcher's slow-time depth D: epochs per coherent block.
   *
   * Derived at create from `code_only_s` and `doppler_rate`
   * (dp_async_dsss_receiver_create()); 1 without a code-only window. Every
   * restarted search -- reset(), a release with `reacquire`, a refused
   * hand-over, configure_search_raw() -- starts its next dwell on a
   * multiple of `D * code_len * spc` input samples counted from create: the
   * receiver discards input up to that boundary first, at most one block.
   * Receivers fed the same stream from create therefore search on the same
   * block grid.
   *
   * @param state Must be non-NULL.
   * @return D, >= 1.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> rx.coherent_bins                  # no code-only window
   * 1
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0,
   * ...                        code_only_s=0.0627)
   * >>> rx.coherent_bins                  # 187 epochs: 94, run 7-smooth
   * 90
   *
   * @endcode
   */
  size_t dp_async_dsss_receiver_get_coherent_bins (
      const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Re-tune the live-tracking Dll's code-lock detector directly.
   * Forwards to `dp_dll_configure_lock_raw()` on the live tracking Dll (the
   * one behind `get_code_locked()`), NOT the refine-stage collection Dll. It
   * retunes the Dll the receiver holds now; every chain build (a seed, the
   * hand-over, reset() or configure_chain_raw()) replaces that Dll with a
   * freshly sized one, so call it once tracking.
   * The detector is the hysteretic lockdet over the DLL's per-N-look CFAR
   * statistic — the levels and verify counts trade declare latency against
   * false-alarm rate.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    CFAR-statistic level to declare code lock (hit
   *                     when the statistic exceeds it).
   * @param down_thresh  Level below which a look is a miss; choose
   *                     <= @p up_thresh for level hysteresis.
   * @param n_looks      Looks per decision — the DLL's non-coherent
   *                     integration depth feeding one statistic.
   * @param alpha        EMA smoothing coefficient on the lock statistic
   *                     (0..1); smaller is smoother/slower.
   * @param n_up         Consecutive hits required to declare lock.
   * @param n_down       Consecutive misses required to drop lock.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> rx.configure_lock_raw(up_thresh=0.4, down_thresh=0.2, n_looks=20,
   * ...                       alpha=0.1, n_up=5, n_down=3)
   * >>> rx.tracking                       # replaced at the next chain build
   * 0
   *
   * @endcode
   */
  void dp_async_dsss_receiver_configure_lock_raw (
      dp_async_dsss_receiver_state_t *state, double up_thresh,
      double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
      uint32_t n_down);

  /**
   * @brief Pin the live-tracking despread/resample/demod grid directly.
   * The escape hatch for the composition-specific knob: `segments` (the
   * live Dll's tracking parameter) and `sps`/`n` (MpskReceiver's samples
   * and terminal outputs per symbol) are independently overridable,
   * still bridged by a freshly-sized `RateConverter` and never coupled to
   * each other. While searching/refining it re-pins the grid used to build
   * the next tracking chain; once tracking it rebuilds `dll`/`rc`/`rx` in
   * place, allocating every replacement before adopting it, and re-seeds
   * the rebuilt chain from the whole carrier -- loop 1 plus loop 2's
   * integrator, the sum status() reports -- so a retune keeps the signal
   * it was tracking.
   *
   * The new grid is checked as create() checked the first, before anything
   * is rebuilt, and refused with the receiver left on its prior grid: the
   * Dll's domain at @p segments, @p sps >= 2, a symbol period of at most
   * 2^20 Dll partials, which the Dll's aid would otherwise refuse mid-build
   * (doppler#2103), and an @p n MpskReceiver would refuse (#2421). Still an
   * abort: an allocation failure in the rebuild (doppler#2112).
   *
   * @param state     Must be non-NULL.
   * @param segments  Live-tracking Dll segments per code period.
   * @param sps       MpskReceiver samples per symbol (the resample target),
   *                  >= 2.
   * @param n         MpskReceiver's terminal outputs per symbol (`m_out`):
   *                  inside dp_ratesync_m_ok() (even, 2..8), and dividing
   *                  @p sps.
   * @return 0 on success, -1 on a grid refused as above.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        spc=2, doppler_uncertainty=500.0)
   * >>> rx.configure_chain_raw(segments=6, sps=8, n=8)  # re-pin the chain
   * >>> rx.segments                       # tracking grid updated in place
   * 6
   *
   * @endcode
   */
  int dp_async_dsss_receiver_configure_chain_raw (
      dp_async_dsss_receiver_state_t *state, size_t segments, size_t sps,
      int n);

  /** @brief 1 once the live tracking chain is built and demodulating; 0 in
   *  every other state. */
  int    dp_async_dsss_receiver_get_tracking (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief 1 while refining: the CarrierAcquisition dwell; 0 in every
   *  other state. */
  int    dp_async_dsss_receiver_get_refining (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The Doppler estimate the chain was built from, Hz: the seed's
   *  while refining, the CarrierAcquisition-refined value once tracking.
   *  The live carrier is status().doppler_hz. */
  double dp_async_dsss_receiver_get_doppler_hz (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The seed's C/N0 estimate, dB-Hz, unchanged until reset(). */
  double dp_async_dsss_receiver_get_cn0_dbhz_est (
      const dp_async_dsss_receiver_state_t *state);
  size_t dp_async_dsss_receiver_get_segments (
      const dp_async_dsss_receiver_state_t *state);
  size_t dp_async_dsss_receiver_get_sps (const dp_async_dsss_receiver_state_t *state);
  int    dp_async_dsss_receiver_get_n (const dp_async_dsss_receiver_state_t *state);
  /** @brief The live Dll's code phase, chips, in the Dll's instantaneous
   *  convention -- the mirror of acq_result_t::code_phase's correlation-lag
   *  convention (dp_acq_build_handoff()). While refining there is no live
   *  Dll: it is the seed's phase advanced by the clock dilation since, the
   *  phase the hand-over starts the live chain from. */
  double dp_async_dsss_receiver_get_chip_phase (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The Dll's code rate, chips per nominal chip (~1.0 at lock),
   *  without the carrier aid; a coasting Dll reports its held rate. */
  double dp_async_dsss_receiver_get_code_rate (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The carrier lock statistic of the MpskReceiver this receiver
   *  tracks with: the EMA of its M-th-power lock signal, near 1 when locked
   *  and near 0 on noise. */
  double dp_async_dsss_receiver_get_lock (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The post-despread carrier the MpskReceiver tracks: its centre
   *  plus its loop's integrator, cycles/sample at its input rate
   *  (`sps * symbol_rate`). Lags a Doppler ramp by the Type-II ramp
   *  error. */
  double dp_async_dsss_receiver_get_norm_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The MpskReceiver's carrier NCO command, its loop filter's whole
   *  output, cycles/sample at its input rate (`sps * symbol_rate`). Its
   *  mean tracks a Doppler ramp with no lag (unlike get_norm_freq()); its
   *  variance is the carrier loop's stress. */
  double dp_async_dsss_receiver_get_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The receiver's symbol-lock flag: a hysteretic detector over
   *  get_lock_metric(), declared after 30 consecutive symbols strictly
   *  above get_lock_threshold() (0.5) and dropped after 15 consecutive
   *  symbols strictly below 0.3 (ASYNC_DSSS_RX_LOCK_N_UP, _UP, _N_DOWN and
   *  _DOWN, fixed at create). The flag the release rule and a pool's holder
   *  act on. */
  int dp_async_dsss_receiver_get_locked (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief Binary code-lock flag from the live tracking Dll's own
   * verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
   * despreading" lock, de-chattered by up/down hysteresis. */
  int dp_async_dsss_receiver_get_code_locked (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief Pre-despread Costas phase discriminator (rad): the residual
   * carrier phase LOOP 1 (which de-rotates before the Dll) is not nulling. */
  double dp_async_dsss_receiver_get_car_last_error (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief LOOP 1 (pre-despread Costas) loop-filter output = NCO frequency
   * command, cycles/sample of the front-end (chip_rate*spc) rate. */
  double dp_async_dsss_receiver_get_car_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief MpskReceiver carrier phase discriminator (rad): the residual
   * carrier phase LOOP 2 (post-despread) is not nulling. */
  double dp_async_dsss_receiver_get_mpsk_last_error (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2)
   * = cos(2*phi) over the emitted symbols (locked -> ~+1). Drives `locked`. */
  double dp_async_dsss_receiver_get_lock_metric (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The lock-metric threshold `locked` is declared strictly above
   * (0.5, ASYNC_DSSS_RX_LOCK_UP); exposed alongside lock_metric for
   * engineering debug. */
  double dp_async_dsss_receiver_get_lock_threshold (
      const dp_async_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car_frozen + refine_dll + refine_rc + ca + car +
   * car_held + dll + rc + rx, a fixed shape; segments/sps/n/
   * refine_segments are the layout key. */

  typedef struct
  {
    uint8_t  state;
    uint8_t  had_lock;     /**< v4: both flags have been up; the loops
                                coast. */
    uint8_t  car_coasting; /**< v4: the carrier loop is held.            */
    uint8_t  _pad[5]; /**< eight NAMED bytes with the three above:
                           unnamed padding is indeterminate in a
                           designated initialiser, and a blob that
                           carried it would not round-trip byte for byte
                           under every compiler. */
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
    uint64_t state_samples;
    uint64_t both_down_samples;
    double   lock_num;  /**< symbol-lock EMAs + hysteretic detector: the */
    double   lock_den;  /**< running state that survives a checkpoint    */
    double   lock_metric;         /**< (config -- alpha, thresholds -- is */
    dp_lockdet_state_t sym_lockdet;  /**< restored by create()).             */
    uint64_t refused_hits; /**< v8: the searching hits refused as seeds. */
    uint64_t refine_min_blocks; /**< v9: set_refine_min_blocks()'s value, a
                                     mutator's, so it travels (#2022). */
    uint64_t samples_in;    /**< v12: input samples since create.        */
    uint64_t align_pending; /**< v12: 0 or 1, a dwell alignment owed.    */
    int64_t  threads;       /**< v12: set_threads()'s result (#2022).    */
    uint64_t rank;          /**< v13: set_rank()'s value (#2022).        */
    uint64_t n_excl;        /**< v13: set_exclusions()'s count ...       */
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; /**< ... and phases, the
                                 unused tail zeroed.                     */
  } async_dsss_receiver_extra_t;

#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 13u /* v13: rank, exclusions; v12: samples_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine_min_blocks; v8: refused_hits; v7: no hand-off flavor; v6: the cell pull-in */

  size_t dp_async_dsss_receiver_state_bytes (
      const dp_async_dsss_receiver_state_t *state);
  void   dp_async_dsss_receiver_get_state (
      const dp_async_dsss_receiver_state_t *state, void *blob);
  int dp_async_dsss_receiver_set_state (dp_async_dsss_receiver_state_t *state,
                                     const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_CORE_H */
