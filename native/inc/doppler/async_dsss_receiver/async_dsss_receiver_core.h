/**
 * @file async_dsss_receiver_core.h
 * @brief Composed continuous DSSS receiver: Acquisition -> hand-off ->
 *        CarrierAcquisition refine -> carrier_nda/Dll/RateConverter/
 *        MpskReceiver track, one object.
 *
 * Unlike `DsssReceiver`, which goes straight from an acquisition hit to
 * tracking on the hit's coarse Doppler estimate, this object refines the
 * estimate between the two, closing the low-Es/N0 pull-in gap a coarse-only
 * estimate leaves at large static Doppler offsets. Its design of record is
 * docs/design/async-dsss-receiver.md: the receiver (§1-§4) and its place
 * in an AsyncDsssPool (§5-§9). The despreader, the states and the release under it are
 * docs/design/async-dsss-receiver.md. Its states are the
 * `ASYNC_DSSS_RX_*` values below, also `status().state`:
 *
 *   - **searching** (the start): samples feed the
 *     embedded continuous search (the `Correlator` flavor). A hit is made
 *     into a seed by
 *     `dp_correlator_build_handoff()` and taken through
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
 *     -- on a give-up, the seed's -- Doppler. It starts at the refine's
 *     estimate of where it stopped, `ceil(k * period)` samples from the
 *     hand-off (k whole code periods, the fraction carried in its phase),
 *     on the same input sample at any chunking (doppler#2974). The refine
 *     decides up to its chain's lag past that estimate, so the receiver
 *     keeps its last refine samples in a ring sized at create for that lag
 *     (`refine_hist_cap`, from dp_RateConverter_get_count_lag()), and the
 *     start is read from it when it is behind the call. A start the input
 *     has not reached yet is waited for, the receiver still refining. A
 *     refine that decides further past than the ring holds (a refine Dll
 *     drifting on noise; no signal geometry measured) starts at the first
 *     such point the ring still holds, still the same at any chunking. The
 *     ring is held for the receiver's lifetime -- up to ~38 KiB (Gold-1023
 *     at 2 samples a chip) -- and carried in every blob, a pool member's
 *     included.
 *   - **tracking**: the pre-despread carrier loop (`carrier_nda_wipeoff`/
 *     `carrier_nda_update`) -> `Dll` -> `RateConverter` -> `MpskReceiver`.
 *     The carrier loop is carrier_nda at M = 2. It updates once a code
 *     period on the Dll's despread window sum (`adr_track_period()`): the
 *     look-back's window does not span a data transition, and its M = 2
 *     error `Im(w^2)` removes the data with no sign decision. The error is
 *     divided by the window's signal power (the unit denominator: the
 *     averaged `|w|^2` less the Dll's window noise), and the lock is the
 *     ratio of sums of `I^2 - Q^2` over `I^2 + Q^2` on blocks sized by
 *     det_ratio_block_len() (docs/design/async-dsss-receiver.md, "The
 *     loop's lock statistic"). It removes the coupled Doppler (offset and
 *     500 Hz/s ramp) before despreading, so `MpskReceiver` is left a small
 *     residual. A pure PLL: no FLL.
 *   - **lost**: the emitter is gone. Without `reacquire`, entered from
 *     tracking when BOTH lock flags have been down, without a break, for
 *     longer than `lost_confirm_s` (docs/design/async-dsss-receiver.md §10,
 *     §11.2); the loops stop and samples are discarded until `reset()`.
 *     `lost_confirm_s = 0` (the default) never enters
 *     it. With `reacquire` the release is symbol lock alone and returns the
 *     receiver to searching instead, so it never reads lost
 *     (docs/design/async-dsss-receiver.md §10). While the confirm interval
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
 * // 3 Mcps, 2,100 sym/s, BPSK at 2 samples a chip; a one-epoch search.
 * const double chip_rate           = 3.0e6;
 * const double symbol_rate         = 2100.0;
 * const double sample_rate         = 6.0e6;  // 2 samples a chip
 * const int    m                   = 2;      // BPSK
 * const double cn0_dbhz            = 55.0;
 * const double pfa                 = 1e-3;
 * const double pd                  = 0.9;
 * const double doppler_uncertainty = 100.0;  // Hz, one-sided
 * const size_t segments = 4, sps = 8;
 * const int    differential        = 0;
 * const double refine_max_error_db = 0.5;    // the refine_* tuning
 * const size_t refine_sps = 4, refine_n_fft = 64, refine_zero_pad = 8;
 * const double refine_margin_db    = 14.0;
 * const bool   refine_sequential   = false;
 * const size_t refine_max_blocks   = 100000;
 * const double carrier_freq_hz     = 0.0;    // 0 = aiding off
 * const double lost_confirm_s      = 0.0;    // 0 = never lost
 * const bool   reacquire           = false;  // lost waits for reset()
 * const double code_only_s         = 0.0;    // no window: D = 1
 * const double doppler_rate        = 0.0;
 * const bool   refine              = true;   // D = 1 needs the refine
 *
 * dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
 *     code, code_len, chip_rate, symbol_rate, sample_rate, m, cn0_dbhz, pfa,
 *     pd, doppler_uncertainty, segments, sps, differential,
 *     refine_max_error_db, refine_sps, refine_margin_db, refine_n_fft,
 *     refine_zero_pad, refine_sequential, refine_max_blocks, carrier_freq_hz,
 *     lost_confirm_s, reacquire, code_only_s, doppler_rate, refine);
 * float _Complex syms[4096];
 * size_t n = dp_async_dsss_receiver_steps(rx, x, x_len, syms, 4096);
 * dp_async_dsss_receiver_destroy(rx);
 * @endcode
 */
#ifndef DP_ASYNC_DSSS_RECEIVER_CORE_H
#define DP_ASYNC_DSSS_RECEIVER_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/correlator/correlator_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/carrier_nda/carrier_nda_core.h"
#include "doppler/ppe/ppe_core.h"
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
#include "doppler/detection/detection_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/fft/fft_core.h"
#include "doppler/acc_trace/acc_trace_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** The widest bandwidth the pre-despread carrier loop takes, per update
   * (a code period): the receiver's own is
   * dp_async_dsss_receiver_carrier_bn() at its design C/N0, which is this
   * value wherever the loop-SNR rule allows it and narrower where it does
   * not. The refine stage's frozen carrier (a Costas whose update is never
   * called) reads only its seed frequency and update length.
   *
   * 0.04 is the widest the loop has been designed at: tracking the
   * specification's 500 Hz/s ramp with a per-period Type-II loop needs the
   * bandwidth to keep the velocity lag (~ramp/omega_n^2) small, and at the
   * design C/N0's rule the loop is narrower. bn is the closed loop's noise
   * bandwidth (carrier_nda divides out its detector's slope, #3224). No
   * FLL. */
#define ASYNC_DSSS_RX_BN_CARRIER 0.04
/** The pre-despread carrier loop's least loop SNR at the design C/N0, dB:
 *  the owner's rule (async-dsss-receiver.md §1.3), `rho S_L / (2 bn)` with
 *  `rho = C/N0 T` the update's SNR and `S_L = 1 / (1 + 1/(2 rho))` the M = 2
 *  detector's squaring loss (the owner's ruling, 2026-11-22), so `bn <= rho
 *  S_L / 200`. */
#define ASYNC_DSSS_RX_CARRIER_LOOP_SNR_DB 20.0
/** The pre-despread carrier loop's unit denominator: the EMA weight an
 *  update of the window sum's power (docs/design/async-dsss-receiver.md,
 *  "The averaging length": 81 / 64 ms at the floor, 10 to 12 times the
 *  loop's memory). */
#define ASYNC_DSSS_RX_CAR_DEN_ALPHA (1.0 / 256.0)
/** The carrier block lock's false-alarm rate a block, the code lock's. */
#define ASYNC_DSSS_RX_CAR_LOCK_PFA 1e-3
/** The carrier block lock's detection probability a block at the design
 *  C/N0, the code lock's. */
#define ASYNC_DSSS_RX_CAR_LOCK_PD 0.99
/** Half the pre-despread carrier loop's noise bandwidth, Hz, at @p bn:
 *  `B_L / 2 = bn / (2 T)`, T a code period. 97.8 Hz at 5 Mcps over
 *  Gold-1023 and bn 0.04. It is the owner's half-bandwidth rule for a
 *  frequency step; the sweep on carrier_nda (sweep-1b7312b09) measured
 *  its no-slip reach at the 4 dB floor at 0.25 `B_L` (unit form), half of
 *  this, so a step at this bound may slip during pull-in, where slips are
 *  allowed (the owner's ruling, 2026-11-22). */
#define ASYNC_DSSS_RX_CARRIER_PULLIN_HZ(bn, chip_rate, code_len)              \
  ((bn) * (chip_rate) / (2.0 * (double)(code_len)))
/** The least search depth, `coherent_bins`, a receiver built with `refine`
 *  false takes at carrier bandwidth @p bn: half the search's Doppler row,
 *  `chip_rate / (2 * code_len * coherent_bins)`, is then inside
 *  ASYNC_DSSS_RX_CARRIER_PULLIN_HZ, which is `coherent_bins >= 1 / bn` at
 *  any chip rate (async-dsss-receiver.md §4.4): the ceiling of `1 / bn`,
 *  a reciprocal within 1e-9 of a whole number taken as that number (so
 *  bn 0.04 asks 25, and 1/25.4 asks 26). */
#define ASYNC_DSSS_RX_NO_REFINE_MIN_BINS(bn)                                  \
  ((size_t)ceil (1.0 / (bn) - 1e-9))

  /**
   * @brief The pre-despread carrier loop's bandwidth at a design C/N0: the
   *        owner's loop-SNR rule (async-dsss-receiver.md §1.3), no wider
   *        than ASYNC_DSSS_RX_BN_CARRIER.
   *
   * The loop updates once a code period, `T = code_len / chip_rate`, on a
   * coherent sum whose SNR is `rho = C/N0 T`. Its loop SNR, counting the
   * M = 2 detector's squaring loss `S_L = 1 / (1 + 1/(2 rho))`, is `rho S_L
   * / (2 bn)`, so ASYNC_DSSS_RX_CARRIER_LOOP_SNR_DB of it asks `bn <= rho
   * S_L / 200`. The bandwidth is that, or ASYNC_DSSS_RX_BN_CARRIER where
   * the rule allows more: the rule is a ceiling, and the release's 0.04 is
   * the widest the loop has been designed at. At the 415.1-B-1 floor (C/N0
   * 38.77 dB-Hz) it is 0.00993 at 2.2 GHz and 0.00744 at 2.8 GHz.
   * create() sizes the receiver's carrier loop with it, at `cn0_dbhz`.
   *
   * @param cn0_dbhz   Design C/N0, dB-Hz.
   * @param chip_rate  Chips per second, > 0.
   * @param code_len   Chips a code period, > 0.
   * @return The bandwidth, per update.
   * @code
   * #include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
   * #include <stdio.h>
   * int main (void)
   * {
   *   double chip = 2.2e9 * 31.0 / 21216.0;  // 415.1-B-1 at 2.2 GHz
   *   printf ("%.4f\n", dp_async_dsss_receiver_carrier_bn (38.77, chip, 1023));
   *   printf ("%.2f\n", dp_async_dsss_receiver_carrier_bn (70.0, chip, 1023));
   *   return 0;  // prints 0.0099 then 0.04
   * }
   * @endcode
   */
  double dp_async_dsss_receiver_carrier_bn (double cn0_dbhz, double chip_rate,
                                            size_t code_len);

  /** The fine hand-over's design: the refine's squared pair sums, located by
   *  PolynomialPhaseEstimator, and the dwell that hands over inside the
   *  carrier loop's edge with probability `p_ho`. Every field is derived by
   *  dp_async_dsss_receiver_fine_design() (docs/design/async-dsss-receiver.md,
   *  "The hand-over: the M-th power of the refine's partial dumps"). */
  typedef struct
  {
    int    k;        /**< sums a code period: the smallest divisor of
                          refine_segments whose range K/(2MT) covers e. */
    double g;        /**< one sum's SNR, linear, with every loss.       */
    double gamma_sq; /**< one squared sum's SNR, linear.                */
    size_t blocks;   /**< the dwell, refine blocks.                     */
    size_t n_s;      /**< sums in the dwell.                            */
    double rho;      /**< the estimate's output SNR, linear: n_s gamma_sq. */
    double p_w;      /**< a wrong peak, the union bound.                */
    double p_miss;   /**< the check refuses the right one.              */
    double thr_db;   /**< the check on snr_db: 10 log10(ln(n_s/pfa)) +
                          10 gamma_E / ln 10 (the exponential bin's mean
                          dB under the dB of its mean).                 */
    double sd_hz;    /**< the estimate's sd, Hz, once divided by M.     */
    double chirp_loss_db; /**< the tone's loss at max_rate 0 over the
                               rate-free dwell, at the design rate, dB.  */
    double max_rate; /**< the estimator's rate half-span, cycles a sum
                          squared: 0 when the chirp loss leaves the dwell
                          inside p_ho, else M rate (T/K)^2.              */
    size_t n_rate;   /**< the estimator's rate rows (1 when off): they
                          join the union bound and the check's cells.    */
  } async_dsss_receiver_fine_t;

  /**
   * @brief Design the fine hand-over at a design point: K, the dwell and
   *        its rates.
   *
   * One pair sum's SNR is `g = (gamma/K) rho^2/kappa (1 - eps)^2
   * sinc^2(e T / K)`, with `gamma = C/N0 T` (T a code period), the
   * replica's `rho^2/kappa` from the code's share of sign-changing
   * boundaries `p_flip` (`rho = 1 - p/4`, `kappa = 1 - p/3`), the code
   * error `eps` chips and the carrier error `e` Hz. Squared (M = 2) it is
   * `gamma_sq = c^2 g^2 / (4 g + 2 + v g^2)`, where a sum straddles a
   * symbol edge with probability `q = T symbol_rate / K`, `c = 1 - q/3`
   * and `v = 1 - 0.4 q - c^2`. Over a dwell of `D` refine blocks of
   * `block_s` seconds the estimate sees `n_s = K D block_s / T` sums at
   * `rho = n_s gamma_sq`, through PolynomialPhaseEstimator's Kaiser window
   * (50 dB), whose peak-bin share `p` and variance ratio `eta` are computed
   * from the window itself. A wrong peak is `(n_s - 1)/2 exp(-p rho/2)`.
   * The check refuses the right one with `1 - Q1(sqrt(2 p rho),
   * sqrt(2 ln(n_s/pfa)))`. The dwell is the least `D` at which the two sum
   * to at most `1 - p_ho`.
   *
   * **The rate axis.** A carrier changing at `rate_hz_s` Hz/s puts a chirp
   * of `M rate (T/K)^2` cycles a sum squared on the tone. Over the dwell
   * found above, the window's coherent sum loses `chirp_loss_db` at
   * max_rate 0. The axis turns on exactly when that loss would leave the
   * dwell short of `p_ho`; the estimator then searches that half-span, its
   * rows (PolynomialPhaseEstimator's own grid at the buffer's length) join
   * the union bound and the check's cells, and the dwell is designed again
   * with them. Derived, not a threshold.
   *
   * At the 415.1-B-1 floor (C/N0 38.77 dB-Hz, Gold-1023, 3,000 symbols a
   * second, refine_segments 10, e 3,000 Hz, eps 0.25, blocks of 5.33 ms,
   * pfa 1e-3) K is 5 and the dwell is 39 blocks at 2.2 GHz and 42 at 2.8
   * GHz, at p_ho 0.99; 29 / 32 at 0.9 and 47 / 51 at 0.999, with no rate.
   *
   * @param cn0_dbhz         Design C/N0, dB-Hz.
   * @param chip_rate        Chips a second, > 0.
   * @param code_len         Chips a code period, > 0.
   * @param p_flip           The code's share of sign-changing chip
   *                         boundaries, from 0 to 1.
   * @param symbol_rate      Data symbols a second, > 0.
   * @param m                Constellation order; the loss law is M = 2's.
   * @param refine_segments  The refine Dll's dumps a code period.
   * @param e_hz             The seed's design carrier error bound, Hz.
   * @param rate_hz_s        The carrier rate the receiver is designed to,
   *                         Hz/s, >= 0 (its doppler_rate).
   * @param eps_chip         The design code error, chips, in [0, 1).
   * @param block_s          One refine block, seconds.
   * @param pfa              The check's false-alarm rate, in (0, 1).
   * @param p_ho             The hand-over's probability, in (0, 1).
   * @param max_blocks       The longest dwell to consider.
   * @param out              Receives the design.
   * @return DP_OK; DP_ERR_INVALID for M other than 2, an argument out of
   *         range, no divisor of refine_segments that covers @p e_hz, or no
   *         dwell up to @p max_blocks that reaches @p p_ho.
   */
  int dp_async_dsss_receiver_fine_design (
      double cn0_dbhz, double chip_rate, size_t code_len, double p_flip,
      double symbol_rate, int m, size_t refine_segments, double e_hz,
      double rate_hz_s, double eps_chip, double block_s, double pfa,
      double p_ho, size_t max_blocks, async_dsss_receiver_fine_t *out);
  /** The Dll's bn, per epoch whatever the steer rate (the symbol-period aid
   * steers once per symbol and keeps bn per epoch): DsssReceiver's value,
   * narrower than dp_dll_create()'s default 0.01 for noise immunity at the
   * low-Es/N0 floor. */
#define ASYNC_DSSS_RX_DLL_BN 0.002
  /** The tracking Dll's early-late spacing, chips. */
#define ASYNC_DSSS_RX_DLL_SPACING 0.5
  /* The damping of both Dlls this receiver builds, the tracker and the
   * refine collector, and the collector's early-late spacing, chips. Named
   * once because create() checks the very arguments each build site passes
   * (dp_dll_params_ok(), doppler#2103). */
  /** Damping factor of both Dlls (the tracker and the refine collector). */
#define ASYNC_DSSS_RX_DLL_ZETA 0.707
  /** The refine-stage collection Dll's early-late spacing, chips. */
#define ASYNC_DSSS_RX_REFINE_DLL_SPACING 0.5
  /* The demodulator, MpskReceiver, as the track chain builds it: its
   * carrier loop's bn (symbol-rate normalised) and damping, its timing
   * loop's bn (chosen on steady-state EVM, see the build site in
   * async_dsss_receiver_core.c), its lock threshold, and MpskReceiver's
   * default RRC arguments, which the integrate-and-dump pulse does not
   * read. Named once so a harness that builds the same demodulator alone
   * (native/validation/async_dsss_receiver_loss.c) cannot drift from it. */
  /** MpskReceiver's RRC roll-off argument (unused by its I&D pulse). */
#define ASYNC_DSSS_RX_DEMOD_RRC_BETA 0.35
  /** MpskReceiver's RRC span argument, symbols (unused by its I&D pulse). */
#define ASYNC_DSSS_RX_DEMOD_RRC_SPAN 8
  /** MpskReceiver's carrier-loop bn, normalised to the symbol rate. */
#define ASYNC_DSSS_RX_DEMOD_BN_CARRIER 0.01
  /** MpskReceiver's carrier-loop damping factor. */
#define ASYNC_DSSS_RX_DEMOD_ZETA 0.707
  /** MpskReceiver's timing-loop bn, normalised to the symbol rate. */
#define ASYNC_DSSS_RX_DEMOD_BN_TIMING 0.005
  /** MpskReceiver's carrier lock threshold. */
#define ASYNC_DSSS_RX_DEMOD_LOCK_THRESH 0.3

/** Default floor on the refine's dwell, blocks: the floor dwell of
 *  docs/design/async-dsss-receiver-measurements.md §12.10 (77 Hz of
 *  estimate noise at 45 dB-Hz); see
 *  dp_async_dsss_receiver_set_refine_min_blocks(). */
#define ASYNC_DSSS_RX_REFINE_MIN_BLOCKS 7u

/** Peaks the searcher reports per dwell (dp_correlator_set_max_peaks()): the list
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
/** Symbol-lock metric EMA dwell, symbols (EMA weight 1/dwell). */
#define ASYNC_DSSS_RX_LOCK_DWELL 30u
/** Lock metric `locked` must be strictly above to count up. */
#define ASYNC_DSSS_RX_LOCK_UP 0.5
/** Lock metric `locked` must be strictly below to count down. */
#define ASYNC_DSSS_RX_LOCK_DOWN 0.3
/** Consecutive symbols above LOCK_UP that declare `locked`. */
#define ASYNC_DSSS_RX_LOCK_N_UP 30u
/** Consecutive symbols below LOCK_DOWN that drop `locked`. */
#define ASYNC_DSSS_RX_LOCK_N_DOWN 15u

/** @brief Where the last hand-over's Doppler came from (".ho_src" and
 *  dp_async_dsss_receiver_get_handover_source()): none since the last
 *  seed (searching or refining), the seed's own (with refine false, where
 *  the seed is the hand-over; or the refine gave up, or its estimates
 *  failed the hand-over's checks), CarrierAcquisition's (§4.7), or the
 *  fine estimate's. */
#define ASYNC_DSSS_RX_HO_NONE 0
/** @brief The hand-over took the seed's own Doppler. */
#define ASYNC_DSSS_RX_HO_SEED 1
/** @brief The hand-over took the data-template estimator's. */
#define ASYNC_DSSS_RX_HO_DATA 2
/** @brief The hand-over took the fine estimate: the refine's squared pair
 *  sums located by PolynomialPhaseEstimator, divided by M. */
#define ASYNC_DSSS_RX_HO_FINE 3
/** The fine hand-over's probability, the dwell's target: the owner's
 *  ruling, 2026-11-22 ("0.99 is fine"; docs/design/async-dsss-receiver.md,
 *  "The dwell, derived"). */
#define ASYNC_DSSS_RX_FINE_P_HO 0.99

/** The state machine's values (`state` below, also the first byte of a
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
  ASYNC_DSSS_RX_IDLE = 3, /**< a pooled receiver waiting for a seed: it
                               consumes samples and does not search
                               (set_pooled(); docs/design/
                               async-dsss-receiver.md §6). A blob that
                               names it on a receiver that is not
                               pooled is refused.                     */
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
    dp_correlator_state_t *acq; /**< The embedded search. */

    /* Refine stage: a frozen-carrier collection Dll feeding
     * CarrierAcquisition via a RateConverter. Rebuilt on every real
     * acquisition hit and on reset(); otherwise untouched. */
    dp_costas_state_t         car_frozen; /**< Refine-stage carrier wipe,
                                               frozen at the seed's coarse
                                               Doppler (never updated). */
    dp_dll_state_t           *refine_dll; /**< Owned refine-stage
                                               collection Dll.          */
    dp_RateConverter_state_t *refine_rc;  /**< Owned resampler to
                                               CarrierAcquisition's
                                               rate.                    */
    dp_carrier_acq_state_t   *ca;         /**< Owned CarrierAcquisition
                                               refining the Doppler.    */
    size_t refine_segments; /**< dp_dll_lookback_segments() result,
                                  computed once at create: config for
                                  refine_dll's layout, needed again by
                                  set_state's check. */
    uint64_t refine_samples_fed; /**< Raw samples fed into the refine
                                       pipeline since it was last (re)built.
                                       At the hand-over, fed minus the
                                       live chain's start is the tail that
                                       goes straight on to it: this call's,
                                       and the ring's newest samples when
                                       the start is behind it. The clock
                                       dilation is taken over the live
                                       chain's start, the estimate from the
                                       refine's blocks, not over this. */
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
    float _Complex *refine_dll_out_buf; /**< Owned scratch: refine_dll's
                                             partials.                 */
    size_t         refine_dll_out_cap;  /**< Capacity of
                                             refine_dll_out_buf, samples. */
    float _Complex *refine_rc_out_buf;  /**< Owned scratch: refine_rc's
                                             resampled output.         */
    size_t         refine_rc_out_cap;   /**< Capacity of
                                             refine_rc_out_buf, samples. */

    /* Track stage. The pre-despread carrier loop is carrier_nda at M = 2,
     * updated once a code period (its update period is the period's
     * samples) on the Dll's despread window sum, with the unit
     * denominator fed the Dll's window noise and the ratio-of-sums block
     * lock (see adr_build_track_chain() and adr_track_period()). */
    dp_carrier_nda_state_t    car; /**< Track stage: pre-despread carrier
                                        loop (loop 1), by value.        */
    dp_dll_state_t           *dll; /**< Owned live tracking Dll.        */
    dp_RateConverter_state_t *rc;  /**< Owned resampler to the
                                        demodulator's sps*symbol_rate.  */
    dp_mpsk_receiver_state_t *rx;  /**< Owned post-despread demodulator
                                        (loop 2).                       */
    /* Scratch, not state -- sized at every track-chain (re)build and never
     * in the steps() hot path (#1192, docs/design/async-dsss-receiver.md
     * §11.5): the live
     * Dll's partials for ONE code period, then rc's resampled output for
     * it. The chain runs a period at a time, so both are fixed by tsamps
     * and rc->rate; they only ever grow. */
    float _Complex *track_dll_out_buf; /**< Owned scratch: the live Dll's
                                            partials for one period.   */
    size_t         track_dll_out_cap;  /**< Capacity of
                                            track_dll_out_buf, samples. */
    float _Complex *track_rc_out_buf;  /**< Owned scratch: rc's output
                                            for one period.            */
    size_t         track_rc_out_cap;   /**< Capacity of
                                            track_rc_out_buf, samples.  */

    /* Shared carrier-wipe scratch/carry -- refine and track stages never
     * run concurrently and both wipe in whole `tsamps`-sample periods, so
     * one buffer set serves either. */
    size_t         tsamps; /**< code_len*spc -- one code period, samples. */
    float _Complex *car_wiped_buf; /**< Owned scratch: one carrier-wiped
                                        period, tsamps samples.       */
    float _Complex *car_carry_buf; /**< Owned carry of input not yet a
                                        whole period, tsamps capacity. */
    size_t         car_carry_len;  /**< Running: live samples in the
                                        carry, 0..tsamps-1.            */

    /* Own copy of the spreading code (same rationale as DsssReceiver's
     * own copy -- neither dp_correlator_create_continuous() nor dp_dll_create()'s
     * borrow-vs-copy semantics are part of their public contract). */
    uint8_t *code;     /**< Owned copy of the spreading code, 0/1 chips. */
    size_t   code_len; /**< Chips in @c code.                            */

    /* Config carried across every chain rebuild. */
    double spc;          /**< Samples per chip, sample_rate / chip_rate:
                              at least one, not necessarily whole
                              (config).                               */
    int    m;            /**< PSK order, 2/4/8 (config).                */
    int    differential; /**< MpskReceiver's differential demap flag
                              (config).                               */
    size_t segments; /**< The live-tracking Dll's partial correlations per
                           code period (refine_segments is the refine
                           collector's). */
    size_t sps;      /**< MpskReceiver's samples per symbol.             */
    int    n;        /**< MpskReceiver's terminal outputs per symbol
                          (`m_out`: even, 2..8).                         */
    double chip_rate;    /**< Chip rate, Hz (config).                   */
    double symbol_rate;  /**< Data-symbol rate, Hz (config).            */
    double bn_carrier; /**< Config: the pre-despread carrier loop's
                            bandwidth, dp_async_dsss_receiver_carrier_bn()
                            at cn0_dbhz, set by create() and restored by
                            it (not in the blob: cn0_dbhz has no mutator). */
    double car_gamma; /**< Config: the carrier loop's update SNR at
                           cn0_dbhz, linear (C/N0 over a code period): the
                           unit denominator's clamp and the block lock's
                           sizing read it. Set and restored by create(). */
    uint32_t car_blk; /**< Config: the carrier block lock's length, updates,
                           det_ratio_block_len() at car_gamma and the loop's
                           jitter (0: no block reaches Pd, the EMA decides).
                           Set and restored by create(). */
    double cn0_dbhz; /**< Design C/N0 -- feeds both Acquisition's own
                           sizing and (derated) CarrierAcquisition's
                           design_snr on every refine-chain (re)build. */
    double pfa;      /**< Also CarrierAcquisition's own pfa.             */
    double pd;       /**< Also CarrierAcquisition's own pd.              */

    /* Refine-stage tuning, fixed at construction (see each on
     * dp_async_dsss_receiver_create()). */
    double refine_max_error_db;       /**< Max async-lookback correlation
                                           loss sizing refine_segments,
                                           dB.                         */
    size_t refine_samples_per_symbol; /**< CarrierAcquisition's rate, in
                                           samples per symbol.         */
    double refine_design_margin_db;   /**< Derating of cn0_dbhz into
                                           CarrierAcquisition's
                                           design_snr, dB.             */
    size_t refine_n_fft;              /**< CarrierAcquisition's block
                                           size.                       */
    size_t refine_zero_pad;           /**< CarrierAcquisition's zero
                                           pad.                        */
    bool   refine_sequential;         /**< CarrierAcquisition's
                                           sequential mode.            */
    size_t refine_max_n_blocks;       /**< Cap on the refine dwell,
                                           blocks.                     */
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
    bool     refine;               /**< Config: a seed refines its Doppler
                                        before tracking; false hands the
                                        seed straight to tracking (see
                                        dp_async_dsss_receiver_create()). */
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
                                 discards search samples to the next
                                 multiple of `coherent_bins * code_bins`
                                 before its next dwell starts.           */

    /* The search's own rate. The engine runs at the rate where one code
     * period is dp_correlator_period_samples(code_len) samples (a power of two:
     * its transforms are that long); refine and tracking run at the
     * caller's. `srch_rs` resamples the input to it while searching and
     * is NULL when the caller already feeds that rate. Search sample j
     * since the resampler last restarted carries input index
     * `srch_origin + j / srch_ratio - dp_resamp_get_execute_delay()` (resamp's own
     * contract), which is how a hit's position is mapped back. */
    resamp_state_t *srch_rs;     /**< Owned search-path resampler, or
                                      NULL at the search rate already.  */
    double          srch_ratio;  /**< Search rate over input rate.      */
    float _Complex *srch_buf;    /**< Owned scratch: one call's input,
                                      resampled.                        */
    size_t          srch_cap;    /**< Capacity of srch_buf, samples.    */
    uint64_t        srch_origin; /**< Running: input index (of
                                      samples_in) the resampler last
                                      restarted at.                     */
    uint64_t        srch_n;      /**< Running: search samples produced
                                      since srch_origin.                */
    /* The hand-off maps a hit to input index t, which the resampler's lag
     * puts a few samples BEHIND the call that reports it. The last
     * srch_hist_cap input samples are kept while searching, so the live
     * chain starts at ceil(t) exactly whatever the chunking (#2042's
     * invariance). Empty at the search rate. */
    float _Complex *srch_hist;     /**< Owned: the last input samples.  */
    size_t          srch_hist_cap; /**< ceil(resampler delay) + 3; 0 at
                                        the search rate.                */
    size_t          srch_hist_len; /**< Running: live samples held.     */
    size_t          srch_warm;     /**< Search samples a restarted
                                        resampler fills with its filter's
                                        start-up: no dwell starts before
                                        them. 0 at the search rate.     */
    size_t   rank;   /**< set_rank(): which surviving peak of a dwell the
                          search takes, 0 = the strongest.               */
    size_t   n_excl; /**< set_exclusions(): code phases held.            */
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; /**< those phases, chips,
                          in [0, code_len), the status() convention.     */
    uint64_t excl_at; /**< samples_in when they were set: the sample they
                          name, which a dwell decided later carries them
                          from.                                          */
    bool     pooled;     /**< set_pooled(): not holding means idle, not
                              searching (§6 of the design page).        */
    size_t   pick_want;  /**< set_search_only(): picks a dwell reports
                              instead of seeding itself; 0 = a normal
                              receiver.                                  */
    size_t   n_picks;    /**< Picks waiting for take_picks().           */
    double   pick_phase[ASYNC_DSSS_RX_MAX_PEAKS]; /**< each pick's code
                              phase, chips, at the sample after the call
                              that found it -- seed()'s convention for
                              a receiver fed the same samples.          */
    double   pick_doppler[ASYNC_DSSS_RX_MAX_PEAKS]; /**< its Doppler, Hz. */
    double   pick_cn0[ASYNC_DSSS_RX_MAX_PEAKS];     /**< its C/N0, dB-Hz. */
    uint64_t pick_at[ASYNC_DSSS_RX_MAX_PEAKS]; /**< its phase's sample,
                              samples_in at the end of that call:
                              take_picks() carries the phase from there. */
    uint64_t refused_hits;         /**< Searching hits the seed domain refused
                                        as seeds, since create or reset. In
                                        the blob with state_samples, so a
                                        resumed receiver's status matches an
                                        unbroken run's.                  */
    int      ho_src;               /**< ASYNC_DSSS_RX_HO_*: which estimate
                                        the live chain was built from,
                                        NONE again at every seed. In the
                                        blob, so a resumed receiver reports
                                        the hand-over it made.           */
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
    dp_carrier_nda_state_t car_held;  /**< The carrier as of the last
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
    double          lock_num;   /**< Running EMA of I^2 - Q^2.          */
    double          lock_den;   /**< Running EMA of I^2 + Q^2.          */
    double          lock_metric; /**< lock_num / lock_den, cos(2*phi). */
    double          lock_alpha; /**< EMA coeff = 1/dwell (dwell >= 30).    */
    dp_lockdet_state_t sym_lockdet; /**< Hysteretic symbol-lock declare
                                         over lock_metric.            */

    /* Telemetry: the attach REQUEST, kept because every seed destroys and
     * rebuilds the tracking chain -- the loops it holds are re-attached
     * from here (dp_async_dsss_receiver_set_telemetry). Borrowed, and never
     * in a blob: a handle to the outside world is not state. */
    dp_tlm_t *tlm;                         /**< NULL = detached.          */
    char      tlm_prefix[DP_TLM_NAME_MAX]; /**< The attach's prefix.      */
    uint32_t  tlm_decim;                   /**< The attach's decimation.  */
    int32_t   tlm_sym_lock;   /**< "<prefix>.sym.lock" id, -1 detached.   */
    int32_t   tlm_sym_locked; /**< "<prefix>.sym.locked" id, -1 detached. */
    int32_t   tlm_sym_num;    /**< "<prefix>.sym.num" id, -1 detached.    */
    int32_t   tlm_sym_den;    /**< "<prefix>.sym.den" id, -1 detached.    */
    int32_t   tlm_state;      /**< "<prefix>.state" id, -1 detached.      */
    int32_t   tlm_handover;   /**< "<prefix>.handover_hz" id, -1 detached.*/
    int32_t   tlm_refused;    /**< "<prefix>.refused" id, -1 detached.    */
    int32_t   tlm_fine_hz;    /**< "<prefix>.ref_fine.hz" id.              */
    int32_t   tlm_fine_snr;   /**< "<prefix>.ref_fine.snr_db" id.          */
    int32_t   tlm_fine_n;     /**< "<prefix>.ref_fine.n" id.               */
    /* The fine hand-over (dp_async_dsss_receiver_fine_design()). Config,
       set and restored by create(): fine_on, fine, fine_cap, fine_ppe.
       Running, in the blob: fine_n and fine_buf. */
    int                        fine_on;  /**< 0 where the design refuses.  */
    async_dsss_receiver_fine_t fine;     /**< the design at create.        */
    dp_ppe_state_t            *fine_ppe; /**< Owned, sized fine_cap.       */
    float _Complex            *fine_buf; /**< Owned: the squared sums.     */
    size_t                     fine_cap; /**< sums the dwell can produce.  */
    size_t                     fine_n;   /**< sums so far this refine.     */
    double fine_hz;     /**< the last fine estimate, Hz (NAN: none).       */
    double fine_snr_db; /**< its snr_db.                                   */
    int32_t   tlm_phase_car;  /**< "<prefix>.phase.car" id, -1 detached.  */
    int32_t   tlm_phase_code; /**< "<prefix>.phase.code" id, -1 detached. */
    int32_t   tlm_cn0;        /**< "<prefix>.cn0" id, -1 detached.        */
    int32_t   tlm_ho_src;     /**< "<prefix>.ho_src" id, -1 detached.     */
    int32_t   tlm_had_lock;   /**< "<prefix>.had_lock" id, -1 detached.   */
    int32_t   tlm_coast;      /**< "<prefix>.coast" id, -1 detached.      */
    int32_t   tlm_hold;       /**< "<prefix>.hold" id, -1 detached.       */
    int32_t   tlm_release;    /**< "<prefix>.release" id, -1 detached.    */

    /* The hand-over starts the live chain at the refine's estimate, and
     * the refine decides up to its chain's lag past it, so the start can
     * lie in input an earlier call already passed (#2974). The last
     * refine_hist_cap refine samples are kept, a ring, so the start is
     * reached at any chunking. The cap is the most the refine decides past
     * its estimate, from the configuration at create: how far the refine
     * RateConverter's output count can trail its input
     * (dp_RateConverter_get_count_lag(), in partials of code_len * spc /
     * refine_segments samples each), plus one partial of Dll framing, one
     * code period of the estimate's round-up and one tsamps block of the
     * refine's own. Running state: it travels in the blob (v17). Held for
     * the receiver's lifetime, up to ~38 KiB (Gold-1023 at 2 samples a
     * chip). Last in the struct, so every field the search and tracking
     * touch keeps the offset it had before the ring. */
    float _Complex *refine_hist;      /**< Owned ring, refine_hist_cap. */
    size_t          refine_hist_cap;  /**< Config: derived at create.   */
    size_t          refine_hist_len;  /**< Running: samples held.       */
    size_t          refine_hist_head; /**< Running: the next write.     */
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
   * @param sample_rate                The rate of the samples pushed,
   *                                   Hz: any ratio to @p chip_rate of
   *                                   at least one, or 0 for two samples
   *                                   a chip (the default). Refine and
   *                                   tracking run at this rate. The
   *                                   search runs at it too when one code
   *                                   period is a whole number of samples
   *                                   (dp_correlator_search_period()); a
   *                                   fractional period is resampled, for
   *                                   the search alone, to a power of
   *                                   two.
   * @param m                          PSK order, 2/4/8; default 2 (BPSK).
   * @param cn0_dbhz                   Design C/N0, dB-Hz; default 55.0 --
   *                                   feeds BOTH the embedded search's
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
   *                                   rate (dp_correlator_carrier_freq_ok()).
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
   *                     async-dsss-receiver.md §2.3), >= 0 and finite. The
   *                     receiver hands its searcher `code_only_epochs =
   *                     floor(code_only_s * chip_rate / code_len) - 1`
   *                     (at least 1), and the searcher's slow-time depth
   *                     follows: `coherent_bins = D`, at most
   *                     `(code_only_epochs + 1)/2`, rounded down to a
   *                     7-smooth length (dp_correlator_create_continuous()).
   *                     Default 0: no window, D = 1.
   * @param doppler_rate  Doppler rate in Hz/s, >= 0 and finite, that the
   *                      slow-time depth is bounded against: at most
   *                      `f_epoch / sqrt(2 * doppler_rate)` epochs
   *                      (dp_correlator_create_continuous()). Default 0: the
   *                      window is the only bound.
   * @param refine  Whether a seed refines its Doppler before tracking. True
   *                (the default): a seed enters refining, CarrierAcquisition
   *                estimates the residual over its dwell, and tracking starts
   *                from the refined Doppler (the `refine_*` parameters).
   *                False: a seed builds the tracking chain at once, at the
   *                seed's own sample, from the seed's code phase and
   *                Doppler; the receiver goes from searching (or idle) to
   *                tracking and never reads refining, and the `refine_*`
   *                parameters are unused. The Doppler handed over is the
   *                search's: the block's mean, within one Doppler row of
   *                it, since the peak is one of the rows bracketing it and
   *                at a near tie noise decides which, not always the nearer
   *                one; so within a row plus `doppler_rate` times half the
   *                dwell (`coherent_bins` code periods) of the Doppler at
   *                the hand-over sample. Past half a row, the carrier loop
   *                meets it outside its budgeted edge, and its lock time is
   *                the pull-in's, not the budget's (the design page, §2.3).
   *                False is sized on the nearer row: it is accepted only
   *                while half the search's Doppler row, `chip_rate /
   *                (code_len * coherent_bins)`, is inside the carrier loop's
   *                acquisition bound, ASYNC_DSSS_RX_CARRIER_PULLIN_HZ: that
   *                is
   *                `coherent_bins >= ASYNC_DSSS_RX_NO_REFINE_MIN_BINS(bn)`,
   *                with `bn` the receiver's carrier bandwidth
   *                (dp_async_dsss_receiver_carrier_bn()): 25 at 0.04, at any
   *                chip rate. A receiver configured outside it (a one-epoch
   *                search, D = 1, among them) is refused, and a blob in the
   *                refining state is refused by set_state(). The name is a
   *                placeholder (async-dsss-receiver.md §4.4).
   * @return A new receiver, searching; NULL on an invalid argument,
   *         including reacquire with a lost_confirm_s under one sample,
   *         refine false with coherent_bins under 25, and a symbol
   *         shorter than a code period (`symbol_rate * code_len >
   *         chip_rate`): the pre-despread carrier loop forms its error,
   *         once a code period, from one coherent sum over the code-period
   *         window the Dll's look-back picks (dp_dll_get_window_sum()),
   *         which is free of data transitions only while a symbol spans a
   *         code period (async-dsss-receiver.md §4.4).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
   * >>> (rx.tracking, rx.refining, rx.lost)       # searching
   * (0, 0, 0)
   *
   * With `refine` false a seed goes straight to tracking; that needs a
   * search at least 25 code periods deep, here a 0.1 s code-only window:
   *
   * >>> fast = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                          sample_rate=6.138e6, doppler_uncertainty=500.0,
   * ...                          code_only_s=0.1, refine=False)
   * >>> fast.coherent_bins
   * 150
   * >>> fast.seed(chip_phase=512.25, doppler_hz_est=-150.0, cn0_dbhz_est=48.0)
   * >>> (fast.tracking, fast.refining, fast.doppler_hz)
   * (1, 0, -150.0)
   * >>> try:                                       # one epoch deep: refused
   * ...     AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                       sample_rate=6.138e6, refine=False)
   * ... except ValueError:
   * ...     print("refused")
   * refused
   *
   * @endcode
   */
  dp_async_dsss_receiver_state_t *dp_async_dsss_receiver_create (
      const uint8_t *code, size_t code_len, double chip_rate,
      double symbol_rate, double sample_rate, int m, double cn0_dbhz,
      double pfa,
      double pd, double doppler_uncertainty, size_t segments, size_t sps,
      int differential, double refine_max_error_db,
      size_t refine_samples_per_symbol, double refine_design_margin_db,
      size_t refine_n_fft, size_t refine_zero_pad, bool refine_sequential,
      size_t refine_max_n_blocks, double carrier_freq_hz,
      double lost_confirm_s, bool reacquire, double code_only_s,
      double doppler_rate, bool refine);

  /** @brief Destroy a receiver and release every child.
   *  @param state May be NULL. */
  void dp_async_dsss_receiver_destroy (dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Return to the searching state.
   * Resets the embedded search and rebuilds the refine-stage chain
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
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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
   *   ASYNC_DSSS_RX_* value: 0 searching, 1 refining, 2 tracking, 3 idle
   *   (a pooled receiver holding nothing), 4 lost
   *   (status().state);
   * - "<prefix>.handover_hz" -- the Doppler the chain was built from, Hz
   *   (dp_async_dsss_receiver_get_doppler_hz()): 0 before a seed, the seed's
   *   while refining, the refined value from the hand-over on, and 0 again after a refused hand-over or a
   *   reset(), which return the receiver to searching (idle in a pool). A
   *   record is a
   *   float, so it resolves about 0.5 Hz near 5 MHz. Not the live carrier:
   *   status().doppler_hz reads that, and ".car.freq" and ".car.nco" are
   *   the carrier loop's own view of it, in cycles a sample at the input
   *   rate, so times chip_rate * spc for Hz;
   * - "<prefix>.refused" -- the hits refused as seeds so far
   *   (status().refused_hits);
   * - "<prefix>.ho_src" -- which estimate the live chain was built from,
   *   an ASYNC_DSSS_RX_HO_* value (0 none since the seed, 1 the seed's,
   *   2 the data template's, 3 the fine estimate's);
   * - "<prefix>.ref_fine.hz", ".snr_db", ".n" -- the fine hand-over's
   *   estimate (Hz, against the seed), its peak-to-mean and its sums, once
   *   at the refine's decision, whether or not its check passed;
   * - "<prefix>.cn0" -- the seed's C/N0 estimate, dB-Hz
   *   (status().cn0_dbhz_est): constant over a tenure, since the receiver
   *   keeps no running estimate while tracking;
   * - "<prefix>.phase.car" -- the pre-despread carrier NCO's phase at the
   *   call's FIRST sample, cycles in [0, 1)
   *   (dp_carrier_nda_get_nco_phase_ahead() past the samples the chain still
   *   carries), exact: the loop steers only at a period's end. The phase
   *   of the tracking chain; while searching or refining it is the
   *   placeholder chain's, and ".state" says which;
   * - "<prefix>.phase.code" -- the code NCO's prompt phase at the call's
   *   first sample, chips (dp_dll_get_code_phase_ahead()), exact to
   *   within a code-loop update inside the carried samples. It is the
   *   code NCO's word, the START of a sample's dwell; the Dll takes its
   *   prompt at the dwell's CENTRE, half an increment later
   *   (dll_tap_base(), nco_dwell_center(): the zero-order-hold rule). So
   *   a prompt on the continuous code phase reads here half a sample
   *   BEFORE it (0.5 chip_rate / fs chips), with no correlation lost --
   *   measured in the pull-in captures, -0.186 chip at 2.69 samples a
   *   chip (0.500 sample) and -0.148 at 3.42 (0.505).
   *
   * The two phases are stamped at the call's first sample, which is the
   * stamp the caller set (dp_tlm_set_now()) for this block, so a caller
   * who knows the transmitted carrier and code phase at that sample
   * subtracts them for the loops' true errors -- the stress the
   * discriminator outputs ".car.e" and ".code.e" only estimate.
   *
   * Feed blocks of about a code period, as the examples do, and these read
   * as a time series of the lifecycle: when the search hit, how long the
   * refine took, what it handed over, when the release came.
   *
   * Each loop reports under the prefix, at its own update rate:
   * - "<prefix>.car" -- the chip-domain carrier loop
   *   (dp_carrier_nda_set_telemetry(): ".lock", ".e", ".freq", ".locked",
   *   ".nco"; ".e" is the raw discriminator, slope 2, so twice the phase
   *   error in radians near lock, and ".lock" the last block's ratio of
   *   sums), once per carrier update: once a code
   *   period, except while it
   *   coasts, when loop 1 is held;
   * - "<prefix>.code" -- the code loop (dp_dll_set_telemetry(): ".e",
   *   ".rate", ".lock", ".locked", ".nco", and the lock statistic's two
   *   terms ".prompt_amp" and ".offset_pwr"), once per code period
   *   (with the symbol aid on, the last steer held);
   * - "<prefix>.code.steer" -- the code loop's filter at each steer of the
   *   symbol aid (dp_dll_set_steer_telemetry(): ".e", ".prop", ".integ",
   *   ".ctrl", ".aid"), the loop's own cadence;
   * - "<prefix>.demod" -- the symbol-domain demodulator
   *   (dp_mpsk_receiver_set_telemetry(): its lock, its carrier and timing
   *   loops, the symbols, its AGC), once per recovered symbol (the AGC's
   *   pair once per gain update), and the timing loop's two filter terms
   *   beside them, ".demod.sync.prop" and ".demod.sync.integ"
   *   (dp_ratesync_loop_set_filter_telemetry());
   * - "<prefix>.acq" -- the acquisition engine
   *   (dp_correlator_set_telemetry()), once per decided dwell;
   * - "<prefix>.rc" -- the rate converter between the code loop and the
   *   demodulator (dp_RateConverter_set_telemetry()). It reports only an
   *   AGC, and this receiver builds it without one, so it registers no
   *   probes;
   * - "<prefix>.sym.lock" and "<prefix>.sym.locked" -- the receiver's own
   *   symbol-lock metric (dp_async_dsss_receiver_get_lock_metric()) and
   *   its decision (dp_async_dsss_receiver_get_locked()), the one its
   *   users act on, once per emitted symbol; "<prefix>.sym.num" and
   *   "<prefix>.sym.den", the metric's two EMAs (their ratio is
   *   ".sym.lock"), on the same symbols;
   * - "<prefix>.ref" -- the refine's estimator, the data template's
   *   (dp_carrier_acq_set_telemetry(): ".stat", ".thr", ".hz", ".n",
   *   ".ready"), once per tested block while refining.
   * - "<prefix>.state", "<prefix>.handover_hz", "<prefix>.refused",
   *   "<prefix>.ho_src", "<prefix>.cn0", "<prefix>.phase.car" and
   *   "<prefix>.phase.code" -- the lifecycle (an async_dsss_rx_state_t),
   *   the Doppler the chain was built from (Hz), the refused hits, the
   *   hand-over's source (ASYNC_DSSS_RX_HO_*), the seed's C/N0 (dB-Hz)
   *   and the loops' NCO phases (cycles, chips) at the call's first
   *   sample, once per steps() call;
   * - "<prefix>.had_lock", "<prefix>.coast" and "<prefix>.hold" -- the
   *   loop control the receiver decides once a code period from the code
   *   and symbol flags, each 0 or 1: whether both have been up together
   *   since the seed (from then on the loops coast rather than run free),
   *   whether this period coasts (both down: the carrier restored to its
   *   hold point and frozen, the code loop dead-reckoning -- one decision
   *   drives both loops), and whether this period re-marked the hold
   *   point (both up); once per code period while tracking;
   * - "<prefix>.release" -- the release clock: how long both flags have
   *   been down (the symbol flag alone with reacquire), as a fraction of
   *   the confirm interval, so the receiver is released once it passes 1;
   *   0 while a flag is up and when there is no release rule. Once per
   *   steps() call.
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
   *         holds); DP_ERR_MEMORY when the attach cannot allocate, its
   *         scratch context or a loop's own (the searcher builds one). Every refusal leaves @p tlm's table untouched and the
   *         receiver as it was: detached, or still on its previous context
   *         under its previous prefix.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.telemetry import Telemetry
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
   * >>> tlm = Telemetry(1 << 12)
   * >>> rx.set_telemetry(tlm, "rx")
   * >>> sorted(p for p in tlm.probe_names if p.startswith(("rx.car",
   * ...                                                    "rx.code")))
   * ['rx.car.e', 'rx.car.freq', 'rx.car.lock', 'rx.car.locked', 'rx.car.nco', 'rx.code.e', 'rx.code.lock', 'rx.code.locked', 'rx.code.nco', 'rx.code.offset_pwr', 'rx.code.prompt_amp', 'rx.code.rate', 'rx.code.steer.aid', 'rx.code.steer.ctrl', 'rx.code.steer.e', 'rx.code.steer.integ', 'rx.code.steer.prop']
   * >>> sorted(p for p in tlm.probe_names if p.startswith(("rx.s", "rx.h",
   * ...                                                    "rx.refu")))
   * ['rx.had_lock', 'rx.handover_hz', 'rx.ho_src', 'rx.hold', 'rx.refused', 'rx.state', 'rx.sym.den', 'rx.sym.lock', 'rx.sym.locked', 'rx.sym.num']
   * >>> sorted(p for p in tlm.probe_names if p.startswith(("rx.ph", "rx.cn")))
   * ['rx.cn0', 'rx.phase.car', 'rx.phase.code']
   * >>> sorted(p for p in tlm.probe_names if p.startswith("rx.ref"))
   * ['rx.ref.hz', 'rx.ref.n', 'rx.ref.ready', 'rx.ref.stat', 'rx.ref.thr', 'rx.ref_fine.hz', 'rx.ref_fine.n', 'rx.ref_fine.snr_db', 'rx.refused']
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
   * carrier loop removes the full carrier dynamics before the code loop, so the
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
   * ...     code, chip_rate=chip, symbol_rate=sym, sample_rate=fs,
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
   * `dp_correlator_handoff_t`'s `chip_phase`, `doppler_hz_est` and `cn0_dbhz_est` --
   * exactly as its own hit would have produced them (`steps()` calls this on
   * its own hit). `chip_phase` is the code's
   * instantaneous phase in chips, Dll's convention, at the FIRST sample of
   * the next `steps()` call; dp_correlator_build_handoff() makes it from a hit's
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
   * ...                        symbol_rate=2700.0, sample_rate=6.138e6)
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
   * reportable (dp_correlator_carrier_freq_ok() relies on this bound), and a seed
   * there sent the hand-off's phase to NaN (doppler#2103).
   *
   * seed() refuses by this predicate, set_state() restores the seed fields
   * only inside it, the searching receiver's hit path does not seed from a
   * hit it fails, and the hand-over holds the REFINED Doppler to it too: a
   * seed in range whose residual carries it to or past fs/2 is refused at
   * the hand-over, counted in refused_hits, and the receiver searches
   * again (idle, in a pool, for the pool's search to re-seed) rather than
   * track a Doppler it cannot report. It reads only the
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
   * holder -- the controller of docs/design/async-dsss-receiver.md §8 --
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
    double chip_phase; /**< Code phase, chips, at the sample after the
                            last one fed: the live Dll's, carried over
                            the partial period the chain holds; while
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
    double car_last_error; /**< Pre-despread loop's residual, rad (slope 1). */
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
   * ...                        symbol_rate=2700.0, sample_rate=6.138e6)
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
   * with `lost_confirm_s = 0`.
   * @param state  Must be non-NULL.
   * @return 1 in the lost state, else 0. */
  int dp_async_dsss_receiver_get_lost (const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Pin the embedded search's grid directly.
   * Forwards to `dp_correlator_configure_search_raw()` -- the escape hatch under
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
   * @return 0 on success; DP_ERR_INVALID on an invalid grid, DP_ERR_MEMORY
   *         on a failed allocation (see dp_correlator_configure_search_raw).
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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
   * ...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
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
   * Forwards to dp_correlator_set_threads() on the receiver's searcher. The
   * search is bit-identical at every count, so this changes the cost of a
   * steps() call and nothing about its output. A pool that steps its
   * receivers in parallel sets each one to 1. A mutator's value, so it
   * travels in the state blob (#2022), in one place: the searcher's own
   * blob, as the request, which dp_correlator_set_state() applies through
   * dp_correlator_set_threads(). A pool's 1 restores as 1 everywhere; a
   * writer's 0 restores as 0, which the reader runs on its own online
   * cores. Setup path; not while another thread is inside steps().
   *
   * @param state Must be non-NULL.
   * @param n     Thread count; 0 = online cores, 1 = serial.
   * @return `DP_OK`. The count actually running is
   *         dp_async_dsss_receiver_get_threads(); a searcher with one
   *         Doppler tile always runs 1.
   * @code
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> rx = AsyncDsssReceiver(code=[1, 0, 1, 1, 0, 0, 1], chip_rate=1e6,
   * ...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
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
   * The sidelobe rule, per cell: a strong signal's list also holds its
   * code's Doppler-offset sidelobes, up to dp_correlator_dsl_db() below it and
   * hundreds of chips from its phase (design acq-surface.md §8). A peak
   * counts as a second signal only if its test_stat exceeds what the
   * stronger signals' sidelobes can read AT ITS CELL by more than the
   * detector's own gate (dp_correlator_gate_stat()). Each stronger signal reads
   * its statistic on its own lag (dp_correlator_hit_t::test_stat_peak) times the
   * code's cross-ambiguity from it to this cell (dp_correlator_sidelobe_ratio()),
   * which reaches dp_correlator_dsl_db() only at the worst cells. The bound is
   * the strongest of these; on a deep block (get_coherent_bins() > 1) it
   * is the sum over the stronger signals on the peak's own Doppler row,
   * modulo the epoch rate, when that is larger, since their fixed sidelobes
   * can stack on one cell, and each is floored at dp_correlator_dsl_db() until a
   * strong signal's data splatter is modelled (doppler#2730). So the range
   * does not narrow with the number of signals on the air (doppler#2554),
   * and a sidelobe passes only when the noise on it alone would have
   * cleared the gate, the rate the search already runs at. A second
   * signal is taken well past dp_correlator_dsl_db() below a stronger one at
   * most cells: on a one-epoch search, measured, in every stream to 10 dB
   * below and in a fifth of them at 20 dB (design async-dsss-receiver.md
   * §7.2, the near-far limit). The model is one clean code period: when a
   * data transition falls inside the stronger signal's epoch -- common,
   * 55% of epochs at the specification's 1.8 epochs a symbol -- its
   * sidelobes read far above it at some cells (20.4 dB at one, measured by
   * native/validation/acq_emitter_floor.c), and only the gate stands
   * between them and a false second signal. Over the operating range the
   * gate held (the near-far sweep, data on both, took no sidelobe); past
   * it, or on a one-epoch search of a much stronger signal, it is not
   * bounded (doppler#2734). Searchers
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
   * ...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
   * >>> rx.set_rank(1)
   * >>> rx.rank
   * 1
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_rank (dp_async_dsss_receiver_state_t *state,
                                       size_t rank);

  /** @brief Which surviving peak of a dwell the search takes; 0 = the
   *  strongest. Set with dp_async_dsss_receiver_set_rank().
   *  @param state  Must be non-NULL.
   *  @return The rank, 0 to ASYNC_DSSS_RX_MAX_PEAKS - 1. */
  size_t dp_async_dsss_receiver_get_rank (
      const dp_async_dsss_receiver_state_t *state);

  /**
   * @brief Code phases the search must not take: signals another receiver
   *        already tracks.
   *
   * Replaces the list. A peak within one chip, circularly, of any of them is
   * dropped from a dwell's list at any Doppler, before ranking
   * (set_rank()). Phases are chips in status().chip_phase's convention, at
   * the sample after the last one fed when they are set, and are folded
   * into [0, code_len); a dwell decided calls later carries them from that
   * sample to its own. An empty list (the default) excludes
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
   * ...                        symbol_rate=1e6 / 28.0, sample_rate=4e6, cn0_dbhz=60.0)
   * >>> rx.set_exclusions(np.array([2.5, 9.0]))   # 9 folds to 2
   * >>> rx.n_exclusions
   * 2
   *
   * @endcode
   */
  int dp_async_dsss_receiver_set_exclusions (
      dp_async_dsss_receiver_state_t *state, const double *phases,
      size_t phases_len);

  /** @brief Code phases set_exclusions() holds.
   *  @param state  Must be non-NULL.
   *  @return Count, 0 to ASYNC_DSSS_RX_MAX_EXCLUSIONS. */
  size_t dp_async_dsss_receiver_get_n_exclusions (
      const dp_async_dsss_receiver_state_t *state);

  /** @brief Workers the searcher runs, the calling thread included; 1 =
   *  serial. Set with dp_async_dsss_receiver_set_threads().
   *  @param state  Must be non-NULL.
   *  @return The embedded search's thread count, >= 1. */
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
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
   * >>> rx.coherent_bins                  # no code-only window
   * 1
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0,
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
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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
   *                  inside dp_ratesync_m_ok() (even, 2..8) and at most
   *                  @p sps -- mpsk_rx_m_out_ok(), the rule create()'s own
   *                  derived `n` satisfies, so get_n() always re-applies
   *                  (doppler#2650). It need not divide @p sps.
   * @return 0 on success, -1 on a grid refused as above.
   * @code
   * >>> import numpy as np
   * >>> from doppler.dsss import AsyncDsssReceiver
   * >>> from doppler.wfm import Gold
   * >>> code = np.asarray(Gold().generate(1023)).astype(np.uint8)
   * >>> rx = AsyncDsssReceiver(code, chip_rate=3.069e6, symbol_rate=2700.0,
   * ...                        sample_rate=6.138e6, doppler_uncertainty=500.0)
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
   *  every other state.
   *  @param state  Must be non-NULL.
   *  @return 1 in the tracking state, else 0. */
  int    dp_async_dsss_receiver_get_tracking (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief 1 while refining: the CarrierAcquisition dwell; 0 in every
   *  other state.
   *  @param state  Must be non-NULL.
   *  @return 1 in the refining state, else 0. */
  int    dp_async_dsss_receiver_get_refining (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The HAND-OVER's Doppler, Hz: what the live chain was built
   *  from, frozen for the track's whole tenure -- the seed's while
   *  refining, the refine's estimate from the hand-over on. It does not
   *  follow the carrier: under a 400 Hz/s ramp it is 400 Hz stale a
   *  second later. Where the emitter is now is status().doppler_hz, a
   *  field of the same name with the other meaning.
   *  @param state  Must be non-NULL.
   *  @return Doppler, Hz; 0 after create, reset() or a refused
   *          hand-over. */
  double dp_async_dsss_receiver_get_doppler_hz (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The seed's C/N0 estimate, dB-Hz, unchanged until reset().
   *  @param state  Must be non-NULL.
   *  @return C/N0, dB-Hz; 0 after create and reset(). */
  double dp_async_dsss_receiver_get_cn0_dbhz_est (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The live-tracking Dll's partial correlations per code period.
   *  @param state  Must be non-NULL.
   *  @return Segments; set at create or by configure_chain_raw(). */
  size_t dp_async_dsss_receiver_get_segments (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief MpskReceiver's samples per symbol.
   *  @param state  Must be non-NULL.
   *  @return Samples per symbol; set at create or by
   *          configure_chain_raw(). */
  size_t dp_async_dsss_receiver_get_sps (const dp_async_dsss_receiver_state_t *state);
  /** @brief MpskReceiver's terminal outputs per symbol (even, 2..8).
   *  @param state  Must be non-NULL.
   *  @return Outputs per symbol; set at create or by
   *          configure_chain_raw(). */
  int    dp_async_dsss_receiver_get_n (const dp_async_dsss_receiver_state_t *state);
  /** @brief The code phase, chips, at the sample after the last one fed,
   *  in the Dll's instantaneous convention -- the mirror of
   *  dp_correlator_hit_t::code_phase's correlation-lag convention
   *  (dp_correlator_build_handoff()). Tracking, it is the live Dll's phase
   *  carried over the partial period the chain holds (it runs whole
   *  periods), so it names the same sample a search's picks are carried to
   *  at any sample rate (#3063). While refining there is no live Dll: it is
   *  the seed's phase advanced by the clock dilation since, the phase the
   *  hand-over starts the live chain from.
   *  @param state  Must be non-NULL.
   *  @return Code phase, chips. */
  double dp_async_dsss_receiver_get_chip_phase (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The Dll's code rate, chips per nominal chip (~1.0 at lock),
   *  without the carrier aid; a coasting Dll reports its held rate.
   *  @param state  Must be non-NULL.
   *  @return Code rate, chips per nominal chip. */
  double dp_async_dsss_receiver_get_code_rate (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The carrier lock statistic of the MpskReceiver this receiver
   *  tracks with: the EMA of its M-th-power lock signal, near 1 when locked
   *  and near 0 on noise.
   *  @param state  Must be non-NULL.
   *  @return The MpskReceiver's lock statistic. */
  double dp_async_dsss_receiver_get_lock (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The post-despread carrier the MpskReceiver tracks: its centre
   *  plus its loop's integrator, cycles/sample at its input rate
   *  (`sps * symbol_rate`). Lags a Doppler ramp by the Type-II ramp
   *  error.
   *  @param state  Must be non-NULL.
   *  @return Carrier frequency, cycles/sample at sps * symbol_rate. */
  double dp_async_dsss_receiver_get_norm_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The MpskReceiver's carrier NCO command, its loop filter's whole
   *  output, cycles/sample at its input rate (`sps * symbol_rate`). Its
   *  mean tracks a Doppler ramp with no lag (unlike get_norm_freq()); its
   *  variance is the carrier loop's stress.
   *  @param state  Must be non-NULL.
   *  @return NCO command, cycles/sample at sps * symbol_rate. */
  double dp_async_dsss_receiver_get_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The receiver's symbol-lock flag: a hysteretic detector over
   *  get_lock_metric(), declared after 30 consecutive symbols strictly
   *  above get_lock_threshold() (0.5) and dropped after 15 consecutive
   *  symbols strictly below 0.3 (ASYNC_DSSS_RX_LOCK_N_UP, _UP, _N_DOWN and
   *  _DOWN, fixed at create). The flag the release rule and a pool's holder
   *  act on.
   *  @param state  Must be non-NULL.
   *  @return 1 while symbol lock is declared, else 0. */
  int dp_async_dsss_receiver_get_locked (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief Binary code-lock flag from the live tracking Dll's own
   * verify-counted (pfa-tuned) lock detector — the fundamental DSSS "am I
   * despreading" lock, de-chattered by up/down hysteresis.
   *
   * This is the flag 415.1-B-1 §7.4.1's "short-code PN lock" names: its
   * acquisition requirement, PN lock with probability at least 0.9 within
   * 20 s of the signal's arrival, anywhere in +/-1,500 Hz of best lock, is
   * read on it (validate_async_dsss_receiver_pullin, whose cells assert the
   * requirement is not refuted at 95%).
   * @param state  Must be non-NULL.
   * @return 1 while code lock is declared, else 0. */
  int dp_async_dsss_receiver_get_code_locked (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The pre-despread carrier loop's last phase error (rad): the
   * residual carrier phase LOOP 1 (which de-rotates before the Dll) is not
   * nulling, as its loop filter takes it -- carrier_nda's discriminator
   * divided by its slope (carrier_nda_loop_error()), so radians near lock.
   * The ".car.e" probe is the raw discriminator, twice this.
   * @param state  Must be non-NULL.
   * @return Phase error, radians. */
  double dp_async_dsss_receiver_get_car_last_error (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief LOOP 1 (the pre-despread carrier loop) output = NCO frequency
   * command, cycles/sample of the front-end (chip_rate*spc) rate.
   * @param state  Must be non-NULL.
   * @return NCO command, cycles/sample at chip_rate * spc. */
  double dp_async_dsss_receiver_get_car_nco_freq (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief MpskReceiver carrier phase discriminator (rad): the residual
   * carrier phase LOOP 2 (post-despread) is not nulling.
   * @param state  Must be non-NULL.
   * @return Phase error, radians. */
  double dp_async_dsss_receiver_get_mpsk_last_error (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief Symbol-lock metric = SNR-weighted EMA of (I^2-Q^2)/(I^2+Q^2)
   * = cos(2*phi) over the emitted symbols (locked -> ~+1). Drives `locked`.
   * @param state  Must be non-NULL.
   * @return The metric, in -1..+1; reset when the track chain is built. */
  double dp_async_dsss_receiver_get_lock_metric (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The lock-metric threshold `locked` is declared strictly above
   * (0.5, ASYNC_DSSS_RX_LOCK_UP); exposed alongside lock_metric for
   * engineering debug.
   * @param state  Must be non-NULL.
   * @return The up threshold, 0.5. */
  double dp_async_dsss_receiver_get_lock_threshold (
      const dp_async_dsss_receiver_state_t *state);

  /** @brief The rules behind the receiver's five lock decisions, each its
   *         levels and verify counts (dp_lockdet_rule_t), for a caller that
   *         draws a lock statistic beside the line it is decided against.
   *
   * - sym: the symbol-lock decision (`locked`) on the lock metric, always
   *   ASYNC_DSSS_RX_LOCK_UP / _DOWN / _N_UP / _N_DOWN; telemetry
   *   ".sym.lock";
   * - car: the pre-despread carrier loop's decision on its lock metric
   *   (dp_carrier_nda_get_lock_rule()); ".car.lock";
   * - code: the Dll's decision on its statistic R
   *   (dp_dll_get_lock_rule()), as configure_lock_raw() last retuned it on
   *   the Dll held now; ".code.lock";
   * - demod, demod_sync: the demodulator's carrier and timing decisions
   *   (dp_mpsk_receiver_get_lock_rule(), _get_sync_lock_rule());
   *   ".demod.lock" and ".demod.sync.lock".
   *
   * The tracking chain is rebuilt at every seed, so read them after the
   * hand-over; each rebuild sets the same rules.
   * @param state  Must be non-NULL.
   * @return The rule. */
  dp_lockdet_rule_t dp_async_dsss_receiver_get_sym_lock_rule (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The carrier loop's lock rule; see
   *         dp_async_dsss_receiver_get_sym_lock_rule().
   * @param state  Must be non-NULL.
   * @return The rule. */
  dp_lockdet_rule_t dp_async_dsss_receiver_get_car_lock_rule (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The code loop's lock rule; see
   *         dp_async_dsss_receiver_get_sym_lock_rule().
   * @param state  Must be non-NULL.
   * @return The rule. */
  dp_lockdet_rule_t dp_async_dsss_receiver_get_code_lock_rule (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The demodulator's carrier lock rule; see
   *         dp_async_dsss_receiver_get_sym_lock_rule().
   * @param state  Must be non-NULL.
   * @return The rule. */
  dp_lockdet_rule_t dp_async_dsss_receiver_get_demod_lock_rule (
      const dp_async_dsss_receiver_state_t *state);
  /** @brief The demodulator's timing lock rule; see
   *         dp_async_dsss_receiver_get_sym_lock_rule().
   * @param state  Must be non-NULL.
   * @return The rule. */
  dp_lockdet_rule_t dp_async_dsss_receiver_get_demod_sync_lock_rule (
      const dp_async_dsss_receiver_state_t *state);

  /** @brief Which estimate the live chain's Doppler came from:
   *         ASYNC_DSSS_RX_HO_NONE until a hand-over, then _SEED, _DATA or
   *         _FINE (the refine's choice), or _SEED at the seed itself with
   *         refine false; NONE again at the next seed that refines.
   * @param state  Must be non-NULL.
   * @return One of the ASYNC_DSSS_RX_HO_* values. */
  int dp_async_dsss_receiver_get_handover_source (
      const dp_async_dsss_receiver_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────
   * Composition: acq + car_frozen + refine_dll + refine_rc + ca + car +
   * car_held + dll + rc + rx, a fixed shape; segments/sps/n/
   * refine_segments are the layout key. */

  /**
   * @brief The receiver's own fields in its state blob, after the
   *        envelope and before the children.
   *
   * Written whole by dp_async_dsss_receiver_get_state(); every field
   * mirrors the dp_async_dsss_receiver_state_t field of the same name.
   */
  typedef struct
  {
    uint8_t  state;        /**< async_dsss_rx_state_t value.             */
    uint8_t  had_lock;     /**< v4: both flags have been up; the loops
                                coast. */
    uint8_t  car_coasting; /**< v4: the carrier loop is held.            */
    uint8_t  _pad[5]; /**< eight NAMED bytes with the three above:
                           unnamed padding is indeterminate in a
                           designated initialiser, and a blob that
                           carried it would not round-trip byte for byte
                           under every compiler. */
    double   seed_chip_phase;     /**< The seed's chip phase, chips.     */
    double   seed_doppler_hz_est; /**< The seed's Doppler, Hz.           */
    double   doppler_hz_est;      /**< Seed or refined Doppler, Hz.      */
    double   cn0_dbhz_est;        /**< The seed's C/N0, dB-Hz.           */
    uint64_t segments;        /**< Layout key: must match the receiver. */
    uint64_t sps;             /**< Layout key: must match the receiver. */
    uint64_t n;               /**< Layout key: must match the receiver. */
    uint64_t refine_segments; /**< Layout key: must match the receiver. */
    uint64_t refine_samples_fed; /**< Raw samples fed to the refine chain. */
    uint64_t car_carry_len;   /**< Live carry samples, < tsamps.        */
    uint64_t state_samples;   /**< Samples since the state was entered. */
    uint64_t both_down_samples; /**< The release clock, samples.        */
    double   lock_num;  /**< symbol-lock EMAs + hysteretic detector: the */
    double   lock_den;  /**< running state that survives a checkpoint    */
    double   lock_metric;         /**< (config -- alpha, thresholds -- is */
    dp_lockdet_state_t sym_lockdet;  /**< restored by create()).             */
    uint64_t refused_hits; /**< v8: the searching hits refused as seeds. */
    uint64_t refine_min_blocks; /**< v9: set_refine_min_blocks()'s value, a
                                     mutator's, so it travels (#2022). */
    uint64_t samples_in;    /**< v12: input samples since create.        */
    uint64_t align_pending; /**< v12: 0 or 1, a dwell alignment owed.    */
    uint64_t rank;          /**< v13: set_rank()'s value (#2022).        */
    uint64_t n_excl;        /**< v13: set_exclusions()'s count ...       */
    double   excl[ASYNC_DSSS_RX_MAX_EXCLUSIONS]; /**< ... and phases, the
                                 unused tail zeroed.                     */
    uint64_t srch_origin;   /**< v14: the search resampler's origin, an
                                 input index.                            */
    uint64_t srch_n;        /**< v14: search samples since it.           */
    uint64_t srch_hist_len; /**< v14: the hand-off history's live count. */
    uint64_t pooled;        /**< v15: set_pooled()'s value (#2022).      */
    uint64_t pick_want;     /**< v15: set_search_only()'s value.         */
    uint64_t n_picks;       /**< v15: picks not yet taken ...            */
    double   pick_phase[ASYNC_DSSS_RX_MAX_PEAKS];   /**< ... their phases, */
    double   pick_doppler[ASYNC_DSSS_RX_MAX_PEAKS]; /**< Dopplers and    */
    double   pick_cn0[ASYNC_DSSS_RX_MAX_PEAKS];     /**< C/N0s, the unused
                                 tails zeroed.                           */
    uint64_t refine_hist_cap; /**< v17: layout key, the hand-over ring's
                                   size; must match the receiver.       */
    uint64_t refine_hist_len; /**< v17: the ring's live count, always
                                   min(refine_samples_fed, cap); packed
                                   oldest first after the carry.        */
    uint64_t excl_at; /**< v18: the sample the exclusions name.          */
    uint64_t pick_at[ASYNC_DSSS_RX_MAX_PEAKS]; /**< v18: each pick's
                                   sample, the unused tail zeroed.      */
    uint64_t ho_src; /**< v19: ASYNC_DSSS_RX_HO_*, refused past 3 (v23). */
    uint64_t fine_n; /**< v23: the fine hand-over's sums so far, at most
                          fine_cap; the buffer follows the refine ring. */
  } async_dsss_receiver_extra_t;

/** @brief Envelope magic ("ADRX") identifying an AsyncDsssReceiver blob. */
#define ASYNC_DSSS_RECEIVER_STATE_MAGIC DP_FOURCC ('A', 'D', 'R', 'X')
/** @brief Layout version of the AsyncDsssReceiver blob (history below). */
#define ASYNC_DSSS_RECEIVER_STATE_VERSION 23u /* v23: the fine hand-over's buffer and count (ho_src 3); v22: the pre-despread carrier loop is carrier_nda ('CNDA' children for car and car_held); v21: the squared-stream refine estimator reverted, its fields gone (79fa50cfe); v20: no threads, the search's own blob carries set_threads()'s request (#3055); v19: the squared-stream refine estimator (§4.7); v18: the exclusions' and picks' samples; v17: the refine hand-over's ring (#2974); v16: the search child is the correlator's search flavor ('CORL', #2752 3.4b); v15: pooled, search-only picks; v14: the search resampler and its origin; v13: rank, exclusions; v12: samples_in, the dwell alignment, threads; v11: no cell mode; v10: the dll children grew (output level, #2495); v9: refine_min_blocks; v8: refused_hits; v7: no hand-off flavor; v6: the cell pull-in */

  /**
   * @brief Size in bytes of the blob dp_async_dsss_receiver_get_state()
   *        writes.
   *
   * Envelope + async_dsss_receiver_extra_t + every child's blob + a fixed
   * tsamps-sample carry region + the hand-over ring (refine_hist_cap
   * samples); fixed for a given configuration.
   *
   * @param state  Must be non-NULL.
   * @return Blob size, bytes.
   */
  size_t dp_async_dsss_receiver_state_bytes (
      const dp_async_dsss_receiver_state_t *state);
  /**
   * @brief Serialize the receiver: its own fields, then each child, then
   *        the carrier carry (live samples, zero-padded to tsamps).
   *
   * Scratch buffers, the telemetry attachment and the refine_periods
   * diagnostic are not carried.
   *
   * @param state  Must be non-NULL.
   * @param[out] blob  Caller-owned buffer of at least
   *                   dp_async_dsss_receiver_state_bytes() bytes.
   */
  void   dp_async_dsss_receiver_get_state (
      const dp_async_dsss_receiver_state_t *state, void *blob);
  /**
   * @brief Restore a blob written by dp_async_dsss_receiver_get_state().
   *
   * The envelope, the layout keys (segments, sps, n, refine_segments),
   * the state value, the seed, rank, exclusions and thread count are all
   * checked before any child is decoded; a refusal there changes nothing.
   * The children are then restored in order, so a child that rejects its
   * sub-blob leaves the children before it already restored.  Restores
   * set_threads()'s value by calling dp_correlator_set_threads().
   *
   * @param state  Must be non-NULL; built with the same configuration.
   * @param blob   A blob produced by dp_async_dsss_receiver_get_state().
   * @return `DP_OK`, or `DP_ERR_INVALID` if the blob is refused.
   */
  int dp_async_dsss_receiver_set_state (dp_async_dsss_receiver_state_t *state,
                                     const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_DSSS_RECEIVER_CORE_H */
