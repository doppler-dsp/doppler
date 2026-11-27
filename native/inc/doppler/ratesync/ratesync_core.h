/**
 * @file ratesync_core.h
 * @brief RateSync — symbol-timing recovery on a matched-filter rate cascade.
 *
 * RateSync owns a `RateConverter` whose **terminal stage carries the pulse**
 * (`dp_RateConverter_create_matched`) and closes a timing loop around that
 * stage's control port. It builds no filters of its own: the matched filter
 * IS the cascade's last dot product, and the polyphase arm that dot product
 * selects IS the fractional timing delay. One filter, no Farrow, no separate
 * matched-filter pass.
 *
 * Where SymbolSync separates the jobs (a matched FIR, then a Farrow
 * interpolator steered by a timing NCO), this fuses them — and because the
 * cascade in front is a full `RateConverter`, the fusion inherits its
 * planning: HB/CIC stages do the bulk decimation for free, so the matched
 * filter is sized by the POST-decimation rate. A matched filter at 256
 * input samples per symbol costs the same bank as one at 4.
 *
 * **Arbitrary rate, by construction.** `sps` is a double — 4, 17.33389, an
 * irrational ratio, or a slowly drifting clock — because the terminal stage
 * (resamp) takes its rate as a double and keeps its place in a uint32_t phase
 * word (a 2^-32 fraction of one input interval), and the loop only has to
 * steer the strobe. That is the real-world case whenever the ADC clock is
 * free-running against the symbol clock.
 *
 * ## Two things this object gets right that are easy to get wrong
 *
 * **1. Nothing in the error path can vanish.** The TED error is the raw
 * detector output times `ted_scale` — a CONSTRUCT-TIME reciprocal of the
 * detector's own slope against this pulse (dp_symsync_ted_slope()), not a
 * running power estimate. A running normaliser is what the error path used
 * to carry, and getting it wrong killed the loop outright: normalising by
 * the on-time energy `|on|^2` alone divides by zero exactly when the strobe
 * sits on the symbol transitions, which is precisely the state the loop must
 * recover FROM. Measured, the error reached -91, the control drove the
 * terminal stage's effective rate NEGATIVE, its accumulator stopped
 * advancing, and the cascade emitted nothing ever again (2 symbols where
 * 4000 were expected — a permanent death, not a transient). The fix at the
 * time was to normalise by `|on|^2 + |mid|^2`: the same signal half a symbol
 * apart, so the sum is bounded away from zero at every timing phase. That
 * sum survives as the LOCK STATISTIC's normaliser, which is the only place
 * it is still computed; the error path moved to the construct-time constant
 * (RATESYNC_LOOP_STATE_VERSION 2 records the change), which settles the
 * question rather than answering it — a constant cannot vanish.
 *
 * Either way the conclusion holds and is what a caller cares about: RateSync
 * needs no clamp on the control anywhere, and there is none in the source.
 * Measured, `ctrl` stays inside a few hundredths driven from the worst
 * initial offset at the widest recommended `bn`, bounded by the detector's
 * own S-curve, which is bounded by construction (report §2.7).
 *
 * **2. The loop stays open until the cascade is primed.** A cascade's first
 * outputs are its delay lines filling, not signal (the eye statistic swings
 * over its whole +-2 range through them). Steering on them is meaningless and
 * was worth one lost acquisition in sixteen. dp_ratesync_create() computes the
 * prime length from the terminal bank's own geometry.
 *
 * ## The T/2 role ambiguity resolves itself
 *
 * Gardner needs an on-time strobe and a transition gate half a symbol earlier.
 * Running at `rate = m/sps` and taking every m-th output as on-time makes that
 * a parity count, which looks like it should be ambiguous — and a half-symbol
 * error is indeed an equilibrium of the detector. It is an **unstable** one:
 * measured over a fine sweep, each parity's S-curve has exactly two zeros per
 * symbol, one at the eye centre with negative slope (stable) and one at the
 * T/2 point with positive slope (unstable). The loop runs away from the wrong
 * one on its own, so **the parity does not matter** and no eye-sign detector
 * or counter flip is needed. (An earlier prototype ran two displaced banks to
 * pin the roles structurally; measurement showed that buys nothing and costs
 * double the multiplies.)
 *
 * ## Measured
 *
 * RRC-BPSK, noiseless, eight initial timing offsets each: every offset
 * acquires, on every cascade the planner builds — HB + Resampler(1,rrc) at
 * `sps = 4`, CIC(8) + Resampler(0.923,rrc) at 17.333, CIC(32) +
 * Resampler(1,rrc) at 64.
 *
 * **The EVM those runs reach is deliberately not quoted here.** A table of
 * literals in a header is the documentation form of a snapshot nothing
 * re-runs, and this one had drifted 3 to 4 dB optimistic by the time anything
 * re-measured it. The live figures are regenerated on every push into
 * `src/doppler/track/tests/validation/ratesync/results.md` — §2.4 for
 * acquisition against the settling budget, §2.5 for the `bn` sweep — and that
 * report's Limits section is executed by `test_validation_limits.py`.
 *
 * What belongs here is the shape, which is stable. `bn` means the same thing
 * on every planned cascade, and that is the point of referencing the control
 * to the TERMINAL stage's rate rather than the cascade rate: the alternative
 * is measured costing 18 dB. How closely depends on how narrow the loop is —
 * the spread across the three cascades widens monotonically as `bn` narrows,
 * from a few tenths of a dB at `bn = 0.02` to several dB at 0.002, so read it
 * as "within ~1 dB at the recommended settings" rather than as a universal.
 * What that measurement does NOT establish is the mechanism: the cascades
 * differ in front-end group delay and in residual ISI, and which of those a
 * narrow loop stops averaging over is not determined. `bn = 0.005` is the
 * best of the recommended settings and `bn = 0.01` the safe default; lower
 * settings acquire too slowly to have settled inside a fixed record, so a
 * record length that does not scale with `1/bn` measures nothing there.
 *
 * Lifecycle: `create -> (step / steps / reset)* -> destroy`
 *
 * @code
 * dp_ratesync_state_t *rx = dp_ratesync_create (17.33389, RATESYNC_PULSE_RRC, 0.35,
 *                                         8, 2, 1024, 0.01, 0.707,
 *                                         RATESYNC_TED_GARDNER);
 * float _Complex sym;
 * if (ratesync_step (rx, x, &sym))
 *   consume (sym);
 * dp_ratesync_destroy (rx);
 * @endcode
 */
#ifndef DP_RATESYNC_CORE_H
#define DP_RATESYNC_CORE_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/cic/cic_core.h"
#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/fir/fir_core.h"
#include "doppler/hbdecim/hbdecim_core.h"
#include "doppler/jm_perf.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/resamp/resamp_core.h"
#include "doppler/resample/resample_core.h"
#include "doppler/timing_est/timing_est_core.h"
#include "doppler/symsync/symsync_core.h" /* gardner_ted / dttl_ted — one TED, reused */
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "doppler/telemetry/telemetry_core.h"
#include "doppler/ber/ber_core.h"
#include "doppler/pn/pn_core.h"
#include "doppler/nco/nco_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Timing-error-detector selection for dp_ratesync_state_t::ted. */
  enum
  {
    RATESYNC_TED_GARDNER = 0, /**< blind Gardner TED (mid * conj diff).   */
    RATESYNC_TED_DTTL    = 1  /**< decision-directed sign-sign DTTL.      */
  };

  /**
   * @brief Matched-filter pulse shape.
   *
   * Aliases of the cascade's own vocabulary (`rc_pulse_t`) so one set of
   * names covers the family; the pulse is built by the RateConverter, which
   * is the only party that knows its own CIC geometry.
   */
  enum
  {
    RATESYNC_PULSE_IANDD = RC_PULSE_IANDD, /**< rectangular / NRZ.        */
    RATESYNC_PULSE_RRC   = RC_PULSE_RRC    /**< root-raised cosine.       */
  };

/** Largest supported outputs-per-symbol (bounds the strobe ring in-struct). */
#define RATESYNC_MAX_M 8

/** @brief Numerical guard on the on-time+mid energy sum feeding the lock
 * statistic (not tunable) — mirrors SYMSYNC_LOCK_EPS. */
#define RATESYNC_LOCK_EPS 1e-12

/** @brief The lock detector's sizing rates per decision: the false-alarm
 *  probability and detection probability the 0.311 threshold is sized for
 *  (symsync's sizing, rolloff 0.35, Es/N0 10 dB). */
#define RATESYNC_LOCK_PFA 1e-3
/** @brief See RATESYNC_LOCK_PFA. */
#define RATESYNC_LOCK_PD 0.9
/** @brief The per-decision false-transition budget the default verify
 *  counts are derived from: n_up = dp_det_verify_count(RATESYNC_LOCK_PFA,
 *  budget), n_down = dp_det_verify_count(1 - RATESYNC_LOCK_PD, budget). 1e-6
 *  is the library's convention for a detector's own default, not a new
 *  number: the Dll's declare budget at its default pfa, the async receiver's
 *  code-lock drop, and symsync's SYMSYNC_LOCK_BUDGET. */
#define RATESYNC_LOCK_BUDGET 1e-6

  /**
   * @brief Telemetry attachment: a borrowed context + this object's probe
   *        ids. NULL ctx (the default) means detached — every probe site is
   *        then one predicted-not-taken branch per recovered symbol.
   */
  typedef struct
  {
    dp_tlm_t *ctx;       /**< NULL = detached                          */
    int32_t   id_e;      /**< "<prefix>.e"      — normalised TED error */
    int32_t   id_ctrl;   /**< "<prefix>.ctrl"   — loop control         */
    int32_t   id_rate;   /**< "<prefix>.rate"   — tracked samples/sym  */
    int32_t   id_lock;   /**< "<prefix>.lock"   — lock_signal mean     */
    int32_t   id_locked; /**< "<prefix>.locked" — lockdet flag         */
    int32_t   id_mu;     /**< "<prefix>.mu"     — timing NCO phase     */
    /* The loop-filter set (dp_ratesync_loop_set_filter_telemetry()), its
       own context, emitted beside the set above. NULL = detached. */
    dp_tlm_t *filter_ctx; /**< NULL = the filter set detached          */
    int32_t   id_prop;    /**< "<prefix>.prop"  — kp * e               */
    int32_t   id_integ;   /**< "<prefix>.integ" — the filter's integrator */
  } ratesync_tlm_t;

  /** The timing loop's states (docs/design/ratesync-timing.md, "Acquisition,
   * then tracking"). A loop with no estimator beside it
   * (dp_ratesync_loop_set_acquisition() never called) is in tracking from
   * reset and never leaves it. */
  /* SSOT: enum=ratesync_lifecycle */
  typedef enum
  {
    RATESYNC_ACQUIRING = 0, /**< the loop is open and its rate state is
                                 zero; its owner's estimator is running.  */
    RATESYNC_TRACKING  = 1, /**< the loop is closed.                      */
    RATESYNC_COASTING  = 2, /**< the loop is held at the rate it had; its
                                 owner's estimator is running.            */
  } ratesync_lifecycle_t;

  /**
   * @brief The symbol-timing loop, independent of what feeds it.
   *
   * Everything RateSync does *after* the cascade emits an output: the strobe
   * ring, the TED, the PI loop, the lock detector and the telemetry. It holds
   * no filter and no cascade — it consumes a stream of terminal-stage outputs
   * and produces a per-input rate deviation (`ctrl`) for whoever owns the
   * accumulator those outputs came from.
   *
   * That split is what lets a receiver reuse this loop verbatim. RateSync
   * owns a `RateConverter` and steers it directly; MpskReceiver owns a
   * `Ddc`/`Ddcr` (mix + the same cascade) and steers the *same* accumulator
   * through the DDC's `rate_ctrl` port. Both drive one implementation of the
   * timing loop, so a fix to the TED or the normaliser reaches both — the two
   * are not peers that can drift apart.
   *
   * The loop must be told the geometry of the accumulator it is steering
   * (dp_ratesync_loop_set_cascade()): the terminal stage's own rate, because
   * that is the scale `ctrl` is referenced to, and the terminal bank's tap
   * count, because that is how many outputs are delay-line fill rather than
   * signal.
   */
  typedef struct
  {
    dp_loop_filter_state_t lf; /**< 2nd-order timing PI loop.               */

    /* ── config (restored by the owner's create(), never packed) ────── */
    double sps;        /**< nominal samples per symbol (any double).      */
    size_t m;          /**< terminal outputs per symbol (>= 2, even).     */
    double term_rate;  /**< terminal stage's own rate; the ctrl scale.    */
    size_t prime_taps; /**< terminal bank taps; sets the prime length.    */
    /** The terminal stage itself, borrowed for TELEMETRY ONLY: the loop
     *  steers this accumulator but does not own it, and `mu` — the sampling
     *  phase the steering produces — is otherwise unobservable from outside
     *  the cascade. NULL when the owner bound the geometry by hand
     *  (dp_ratesync_loop_set_cascade()) rather than from a cascade; the probe
     *  then reports 0. Never dereferenced on the hot path. */
    const resamp_state_t *term;
    double bn;         /**< loop noise bandwidth (retained).              */
    double zeta;       /**< damping factor (retained).                    */
    int    ted;        /**< RATESYNC_TED_GARDNER / _DTTL.                 */
    /** Reciprocal of the detector's own slope against this pulse
     *  (dp_symsync_ted_slope()), computed once by
     *  dp_ratesync_loop_bind_cascade(). The hot path MULTIPLIES by it: a
     *  divide, and the running power estimate it would have divided by, are
     *  both construct-time work masquerading as per-symbol work. */
    double ted_scale;
    int    acquires;   /**< 1: the owner has an estimator and seeds this
                            loop (dp_ratesync_loop_set_acquisition()), so
                            the loop starts in acquiring and leaves
                            tracking on a lost lock; 0: tracking only.    */

    /* ── running state ──────────────────────────────────────────────── */
    int      state;     /**< ratesync_lifecycle_t.                         */
    int      present;   /**< the owner's presence input, as given
                             (dp_ratesync_loop_set_presence()): non-zero
                             is present.                                   */
    int      had_lock;  /**< lock has declared since tracking was entered. */
    uint32_t miss_run;  /**< decisions in a row that were not hits, while
                             lock has not declared.                        */
    size_t   seed_left; /**< outputs until a seed is behind the loop: the
                             seed's control is applied while this is above
                             `m`, and the loop stays open until it is 0.   */
    double ctrl;       /**< per-input rate deviation now applied.         */
    double last_error; /**< last normalised TED error.                    */
    double rate_est;   /**< smoothed tracked samples/symbol.              */
    int    have_prev;  /**< a previous on-time strobe exists.             */
    size_t prime_left; /**< strobes still to discard (cascade filling).   */
    size_t out_count;  /**< terminal outputs seen (mod m: strobe phase).  */

    /** Newest-first ring of the last m/2+1 outputs: the transition gate is
     *  m/2 outputs behind the on-time strobe, so it is simply the element at
     *  index m/2. (Written without brackets on purpose: mkdoxy renders this
     *  comment into markdown, where a bare `name[i]` parses as a link
     *  reference and fails the --strict docs build.) */
    float _Complex ring[RATESYNC_MAX_M / 2 + 1];
    size_t        ring_n; /**< valid entries in the ring (saturates at
                               m/2 + 1); 0 after reset. */
    float _Complex prev_on; /**< previous on-time strobe.                  */

    /* ── lock detector (always on): tumbling-window block average ───── */
    double lock_sum;      /**< running sum over the current avgs block.      */
    size_t lock_count;    /**< looks accumulated in the current block.       */
    size_t avgs;          /**< non-coherent block size (looks/decision).     */
    double lock_stat;     /**< last block-averaged lock_signal.              */
    dp_lockdet_state_t lock; /**< declare/drop rule stepped on lock_stat.    */

    ratesync_tlm_t tlm; /**< live telemetry attachment; zeroed in blobs.  */
  } ratesync_loop_t;

  /**
   * @brief RateSync state: a matched-filter cascade and the timing loop.
   *
   * The matched filter is a heap `RateConverter` child (it owns the cascade,
   * the banks and every delay line); the loop is embedded by value.
   */
  typedef struct
  {
    dp_RateConverter_state_t *mf;   /**< cascade; terminal stage is the MF.  */
    ratesync_loop_t        loop; /**< timing loop closed around it.       */

    /* ── config (restored by create(), never packed in a state blob) ── */
    int    pulse;      /**< RATESYNC_PULSE_IANDD / _RRC.                  */
    double beta;       /**< RRC roll-off.                                 */
    size_t span;       /**< one-sided RRC span, symbols.                  */
    size_t num_phases; /**< bank arms (power of two).                     */
    double delay;      /**< the cascade's group delay, input samples
                            (dp_RateConverter_get_group_delay()).         */
    double decim;      /**< inputs to one terminal-stage input: the whole
                            decimation ahead of the terminal stage.       */
    dp_timing_est_state_t *est; /**< the feedforward estimator, matched,
                            owned; NULL when `sps` is under
                            TIMING_EST_MATCHED_SPS, where there is no
                            built-in acquisition.                         */

    /* ── acquisition (dp_ratesync_configure_acquisition(); in the blob) */
    size_t acq_symbols; /**< symbols an attempt estimates over; 0: off.   */
    double acq_quality; /**< an estimate at or above this seeds the loop. */

    /* ── running state ──────────────────────────────────────────────── */
    uint64_t in_count;   /**< inputs taken since create or reset.         */
    uint64_t term_in;    /**< inputs the terminal stage has been handed.  */
    uint64_t acq_origin; /**< `in_count` at the estimator's first sample
                              of this acquisition.                        */
    int      acq_run;    /**< the estimator is on an acquisition.         */
  } dp_ratesync_state_t;

  /* ------------------------------------------------------------------
   * The timing loop on its own (shared with the receivers)
   * ------------------------------------------------------------------ */

  /**
   * @brief Initialise a standalone timing loop.
   *
   * Sets the loop filter (update period = one symbol, so @p bn is normalised
   * to the symbol rate) and the default lock-detector geometry, then seeds
   * every running field. The caller must still describe the accumulator being
   * steered with dp_ratesync_loop_set_cascade() before pushing outputs through.
   *
   * @param l     Loop to initialise. Must be non-NULL.
   * @param sps   Nominal samples per symbol (any double).
   * @param m     Terminal outputs per symbol; even, 2..RATESYNC_MAX_M.
   * @param bn    Loop noise bandwidth, normalised to the symbol rate.
   * @param zeta  Damping factor.
   * @param ted   RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL.
   */
  void dp_ratesync_loop_init (ratesync_loop_t *l, double sps, size_t m, double bn,
                           double zeta, int ted);

  /**
   * @brief Tell the loop the geometry of the accumulator it steers.
   *
   * @param l          Loop. Must be non-NULL.
   * @param term_rate  The terminal stage's own rate. `ctrl` is referenced to
   *                   this, not to the overall cascade rate — they differ by
   *                   the whole integer decimation in front, which would
   *                   under-drive the loop by exactly that factor.
   * @param prime_taps The terminal bank's tap count; the loop discards
   *                   `prime_taps + 1` outputs before closing, because those
   *                   are the delay lines filling rather than signal.
   */
  void dp_ratesync_loop_set_cascade (ratesync_loop_t *l, double term_rate,
                                  size_t prime_taps);

  /**
   * @brief Read that geometry straight off a cascade.
   *
   * Walks @p rc to its terminal stage and forwards its rate and tap count to
   * dp_ratesync_loop_set_cascade(). Every owner of this loop owns a
   * `RateConverter` somewhere — RateSync directly, the receivers inside their
   * DDC — so the walk lives here once rather than in each of them.
   *
   * @param l   Must be non-NULL.
   * @param rc  The cascade whose terminal stage the loop steers.
   */
  void dp_ratesync_loop_bind_cascade (ratesync_loop_t             *l,
                                   const dp_RateConverter_state_t *rc);

  /** @brief Re-seed the loop: integrator, strobe ring, lock detector and the
   *         prime countdown. Configuration and cascade geometry are kept.
   *  @param l  Must be non-NULL. */
  void dp_ratesync_loop_reset (ratesync_loop_t *l);

  /**
   * @brief Say whether the loop's owner acquires for it.
   *
   * With @p on the loop has the three states of the design page: it is put
   * in acquiring now unless its lock is declared, a reset returns it there,
   * and tracking is left when a declared lock drops (to coasting with the
   * presence input up, to acquiring without) or when `n_down` decisions in
   * a row are not hits on a loop that has not declared. The owner runs its
   * estimator while the state is not tracking and hands the result to
   * dp_ratesync_loop_seed(). With @p on 0 the loop is in tracking and stays
   * there, which is the loop every owner had before.
   *
   * @param l   Must be non-NULL.
   * @param on  Non-zero: the owner acquires.
   */
  void dp_ratesync_loop_set_acquisition (ratesync_loop_t *l, int on);

  /**
   * @brief The owner's presence input: is the signal known to be there by
   *        evidence other than the symbol lock?
   *
   * A DSSS receiver raises it while its code lock and its carrier lock are
   * both up. It decides where a lost symbol lock goes: with presence the
   * loop coasts, held at its rate, and without it the loop acquires from a
   * zero rate. Lowering it during a coast ends the coast in acquiring.
   * It has no effect on a loop whose owner does not acquire.
   *
   * @param l        Must be non-NULL.
   * @param present  Non-zero: present.
   */
  void dp_ratesync_loop_set_presence (ratesync_loop_t *l, int present);

  /**
   * @brief Seed the loop: say where the symbol clock is and close on it.
   *
   * @p ahead is how far past the terminal stage's NEXT output an on-time
   * instant sits, in output periods at the rate the loop holds, 0 or more
   * and not whole. Its whole part picks the on-time outputs, that one and
   * every `m`-th after it. Its fraction `r` is moved by one symbol of
   * control: the terminal stage sets the interval to its next output as
   * each output is emitted, `1 / (R + ctrl)` of its inputs, so the control
   * that stretches `m` intervals by `r / m` of themselves is
   *
   *     ctrl = (R + held) m / (m + r) - R,   held = integrator * R
   *
   * held while `m` outputs are emitted. It is always a stretch. The loop
   * enters tracking, its lock detector is reset, and it stays open for the
   * `2 m` outputs the stretch and the gate behind it take; the symbols of
   * those outputs are emitted and are not valid.
   *
   * @param l         Must be non-NULL. A loop still in its prime countdown
   *                  takes the seed too: the outputs the fill drops are
   *                  counted, so the on-time outputs are the same ones.
   * @param ahead     Output periods from the next output to an on-time
   *                  instant; finite, 0 or more.
   * @param rate      The loop filter's integrator to start from, symbols a
   *                  symbol: `sps / true_sps - 1`. Read only with
   *                  @p set_rate; magnitude under 0.5.
   * @param set_rate  0 keeps the rate the loop holds: zero after acquiring,
   *                  the held rate after a coast.
   * @return DP_OK, or DP_ERR_INVALID (a non-finite or negative @p ahead,
   *         a @p rate out of range) with nothing changed.
   */
  int dp_ratesync_loop_seed (ratesync_loop_t *l, double ahead, double rate,
                          int set_rate);

  /** @brief A lock decision's consequence for the state; called from
   *         ratesync_loop_take_output() on a loop whose owner acquires.
   *  @param l    Must be non-NULL.
   *  @param was  The lock flag before the decision.
   *  @param now  The lock flag after it. */
  void dp_ratesync_loop_decide (ratesync_loop_t *l, int was, int now);

  /** @brief Is the symbol just emitted valid: the loop tracking, no seed in
   *         progress and the lock declared. On a loop whose owner does not
   *         acquire it is the lock flag.
   *  @param l  Must be non-NULL.
   *  @return 1 or 0. */
  static inline int
  dp_ratesync_loop_valid (const ratesync_loop_t *l)
  {
    return l->state == RATESYNC_TRACKING && !l->seed_left && l->lock.locked;
  }

  /** @brief Symbols the loop emits before its timing has settled.
   *
   *  dp_loop_filter_settle_time() of the loop's own `bn` and `zeta` at
   *  DP_LOOP_FILTER_SETTLE_EPS: the timing error left from the start is
   *  under 2% of it from that symbol on. An index into the symbols the loop
   *  emits, so the outputs dropped while the cascade fills are not in it.
   *  It follows dp_ratesync_loop_configure(). It is the linear loop's
   *  figure: the detector's slope at lock is taken as 1 (what `ted_scale`
   *  delivers for the bound pulse), and a clock offset's own transient is
   *  not counted.
   *  @param l  Must be non-NULL.
   *  @return Symbols; infinity at `bn = 0`, a loop that does not steer. */
  double dp_ratesync_loop_settle_time (const ratesync_loop_t *l);

  /** @brief Retune the loop; preserves the integrator (and so the lock).
   *
   *  A (@p bn, @p zeta) outside dp_loop_filter_params_ok() is refused:
   *  nothing changes.
   *
   *  @param l     Must be non-NULL.
   *  @param bn    Loop noise bandwidth, normalized to the symbol rate.
   *  @param zeta  Damping factor.
   *  @return DP_OK, or DP_ERR_INVALID when (@p bn, @p zeta) is refused. */
  int dp_ratesync_loop_configure (ratesync_loop_t *l, double bn, double zeta);

  /** @brief Set the lock detector's geometry; see
   *         dp_ratesync_configure_lock_raw(), which forwards here.
   *  @param l            Must be non-NULL.
   *  @param avgs         Looks per decision; clamped >= 1.
   *  @param up_thresh    Declare threshold on lock_stat.
   *  @param down_thresh  Drop threshold; <= up_thresh for hysteresis.
   *  @param n_up         Consecutive above-threshold decisions to declare.
   *  @param n_down       Consecutive below-threshold decisions to drop. */
  void dp_ratesync_loop_configure_lock_raw (ratesync_loop_t *l, size_t avgs,
                                         double up_thresh, double down_thresh,
                                         uint32_t n_up, uint32_t n_down);

  /** @brief Register the six timing probes; see dp_ratesync_set_telemetry(),
   *         which forwards here. NULL @p tlm detaches.
   *  @param l       Must be non-NULL.
   *  @param tlm     Telemetry context to attach (borrowed), or NULL.
   *  @param prefix  Probe-name prefix; NULL means "sync".
   *  @param decim   Emit every decim-th symbol.
   *  @return DP_OK, or DP_ERR_INVALID if the prefix is too long or the
   *          probe table cannot take all six (nothing is attached). */
  int dp_ratesync_loop_set_telemetry (ratesync_loop_t *l, dp_tlm_t *tlm,
                                   const char *prefix, uint32_t decim);

  /** @brief Attach (or detach) the timing loop-filter set: "<prefix>.prop"
   *         (kp * e) and "<prefix>.integ" (the filter's integrator), once a
   *         recovered symbol, beside the six probes.
   *
   *  ".ctrl" is the filter's output times the terminal stage's rate and
   *  ".rate" is `sps / (1 + integ)`, clamped, so neither shows the two
   *  terms apart; this set does. It is emitted from
   *  dp_ratesync_loop_tlm_flush(), so it reports only while the six-probe
   *  set is attached too. Its own context, so a composer that does not ask
   *  for it (MpskReceiver's own attach) registers nothing more.
   *  Observation only: no decision reads it.
   *  @param l       Must be non-NULL.
   *  @param tlm     Telemetry context (borrowed), or NULL to detach this set.
   *  @param prefix  Probe-name prefix; NULL means "sync".
   *  @param decim   Emit every decim-th symbol.
   *  @return DP_OK, or DP_ERR_INVALID (an overlong prefix or a full probe
   *          table), the set then detached. */
  int dp_ratesync_loop_set_filter_telemetry (ratesync_loop_t *l,
                                             dp_tlm_t *tlm,
                                             const char *prefix,
                                             uint32_t decim);

  /** @brief Emit the timing loop's telemetry for the symbol just recovered.
   *
   *  Out-of-line on purpose: the emit machinery must not inline into the
   *  per-sample hot loop (the same body-growth cost symsync measured).
   *  Callers gate on `l->tlm.ctx`, so the detached cost is one
   *  predicted-not-taken branch per symbol.
   *
   *  @param l  Loop with a non-NULL tlm.ctx (caller-checked). */
  void dp_ratesync_loop_tlm_flush (const ratesync_loop_t *l);

/* ── Serializable state — the loop alone (nested by every owner) ───────────
 * Envelope, this loop's running scalars, then the loop filter's own
 * self-validating sub-blob. Config (sps/m/term_rate/prime_taps/bn/zeta/ted)
 * is restored by the owner's create() and never packed. */
/** @brief State-blob magic for the timing loop (FourCC "RSLP"). */
#define RATESYNC_LOOP_STATE_MAGIC DP_FOURCC ('R', 'S', 'L', 'P')
/** @brief Timing-loop blob layout version. */
#define RATESYNC_LOOP_STATE_VERSION 3u /* v3: the lifecycle (state,
                                        * presence, had_lock, miss_run,
                                        * seed_left); v2: the TED
                                        * normaliser is a construct-time
                                        * constant, so pwr_avg/pwr_seeded
                                        * are gone */

  /** @brief Bytes dp_ratesync_loop_get_state() writes (envelope + payload +
   *         the loop filter's child blob).
   *  @param l  Must be non-NULL.
   *  @return Blob size in bytes. */
  size_t dp_ratesync_loop_state_bytes (const ratesync_loop_t *l);
  /** @brief Serialize the loop's mutable state into @p blob.
   *  @param l     Must be non-NULL.
   *  @param blob  Caller-owned buffer of at least
   *               dp_ratesync_loop_state_bytes() bytes. */
  void dp_ratesync_loop_get_state (const ratesync_loop_t *l, void *blob);
  /** @brief Restore the loop's mutable state from @p blob.
   *
   *  Config is not in the blob: restore into a loop built with the same
   *  geometry. A refusal changes nothing.
   *
   *  @param l     Must be non-NULL.
   *  @param blob  Blob from dp_ratesync_loop_get_state().
   *  @return DP_OK, or DP_ERR_INVALID if any envelope rejects. */
  int dp_ratesync_loop_set_state (ratesync_loop_t *l, const void *blob);

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief The domain of `m`, the terminal outputs per symbol: even, and
   *        `2 <= m <= RATESYNC_MAX_M`.
   *
   * The ONE predicate. Gardner needs the half-symbol gate, so `m` is even
   * and at least 2, and RATESYNC_MAX_M bounds the in-struct strobe ring.
   * dp_ratesync_create() and dp_mpsk_receiver_create() refuse outside it,
   * and a composition that takes `m` from a caller and builds the chain
   * later checks it first, so the build never meets the refusal. That is
   * dp_async_dsss_receiver_configure_chain_raw(), whose rebuild runs under
   * dp_xnn (doppler#2421).
   *
   * @param m  Terminal outputs per symbol.
   * @return   1 inside the domain, 0 outside it.
   */
  int dp_ratesync_m_ok (size_t m);

  /**
   * @brief Create a RateSync instance.
   *
   * Builds a `RateConverter(rate = m/sps, pulse, ..., pulse_sps = m)` with CIC
   * droop compensation on — folded into the bank, so it costs six taps per arm
   * and no extra stage, and is worth ~28 dB of EVM on any cascade that plans a
   * CIC. See dp_RateConverter_create_matched().
   *
   * @par The caller owns the input level
   * Present **unit-amplitude symbols**. This object carries no AGC, and that
   * is deliberate: a receiver composing it already levels in its own
   * front-end cascade (dp_RateConverter_enable_agc(), one per receiver), so an
   * AGC here would be a second one integrating against the first. The level
   * to hit is not a tuned number — the TED normalises by its own
   * construct-time slope, and that slope is computed for the reference the
   * bank already defines, `10*log10(bank_e0 / bank_sps)`, which is ~0 dB
   * because the bank normalises by its own pulse energy. See
   * dp_RateConverter_agc_ref_db(), which is defined for any matched cascade
   * whether or not an AGC is enabled.
   *
   * **The level axis is two-sided and it is not monotone**, so "as long as
   * it is not clipping" is not a level check. Both ends cost EVM for one
   * reason: the Gardner error carries an `A^2` factor, so the input level
   * multiplies the loop gain and the level axis IS the `bn` axis — too hot
   * tracks noisily, too cold has not settled. Measured, EVM is flat to
   * within a few tenths of a dB either side of the contracted level and
   * falls off sharply outside that, by well over 15 dB at twice amplitude
   * and again at a quarter of it (report §2.6, regenerated every push).
   *
   * **Nothing here reports either end reliably.** Over-drive is reported
   * only on the subset of plans that happen to contain a CIC:
   * dp_ratesync_get_clipped() is a CIC quantiser flag, and whether the plan HAS
   * a CIC is the planner's decision, not the caller's — a CIC-free cascade
   * (which is what `sps = 8` plans) reads 0 however hard it is driven.
   * Under-drive has no flag on any plan at all; that gap is tracked as
   * gh-661. And `locked` is not a substitute for either: it answers "is the
   * eye open", which even a badly mis-levelled loop eventually manages, so
   * it declares lock while demodulating far worse. Judge the input level by
   * measuring it, not by reading a flag off this object.
   *
   * @param sps         Nominal samples per symbol; any double >= @p m
   *                    (17.33389 is as valid as 4). The bound is `m`, not 2,
   *                    because the terminal stage must not be asked to
   *                    interpolate: `rate = m/sps <= 1`.
   * @param pulse       RATESYNC_PULSE_IANDD (rectangular/NRZ) or _RRC.
   * @param beta        RRC roll-off in `[0, 1]` (ignored for the rectangle).
   * @param span        One-sided RRC span in symbols (ignored for the
   *                    rectangle, whose support is always one symbol).
   * @param m           Terminal outputs per symbol: even, `2 <= m <=
   *                    RATESYNC_MAX_M`. Gardner needs the half-symbol gate,
   *                    so m must be even and at least 2. The oversampled
   *                    stream is a by-product, not an extra cost.
   *                    Both pulses lock at every m. With
   *                    RATESYNC_PULSE_IANDD a small m costs EVM, not lock:
   *                    on a noiseless NRZ stream at 8 samples a symbol the
   *                    error vector is largest at m = 2, smaller at m = 4
   *                    and gone at m = 6 and 8; report §2.7 sweeps it. The
   *                    RRC reads the same at every m.
   * @param num_phases  Matched-filter arms; power of two (1024 is a good
   *                    default). Sets the fractional-timing resolution to
   *                    `1/num_phases` of an output period.
   * @param bn          Loop noise bandwidth, normalised to the symbol rate,
 *                    of the closed loop given a unit-slope detector (see
 *                    LoopFilter). The detector is normalised by its own
 *                    slope against the pulse, which makes the slope 1 at
 *                    UNIT symbol amplitude only: Gardner's goes as the
 *                    amplitude squared and DTTL's as the amplitude, so a
 *                    quarter-amplitude stream runs a Gardner loop at 1/16
 *                    of `bn`.
   * @param zeta        Damping factor (1.0 = critical; 0.707 usual).
   * @param ted         RATESYNC_TED_GARDNER (blind) or RATESYNC_TED_DTTL
   *                    (decision-directed; BPSK/QPSK only).
   * @return Heap-allocated state, or NULL if a parameter is out of range or
   *         allocation fails.
   * @note Caller must call dp_ratesync_destroy() when done.
   */
  dp_ratesync_state_t *dp_ratesync_create (double sps, int pulse, double beta,
                                     size_t span, size_t m, size_t num_phases,
                                     double bn, double zeta, int ted);

  /** @brief Destroy a RateSync instance and release all memory.
   *  @param state  May be NULL. */
  void dp_ratesync_destroy (dp_ratesync_state_t *state);

  /**
   * @brief Reset to the post-create state: the cascade, the loop integrator,
   *        the lock detector, the strobe ring and the prime countdown.
   *
   * Configuration (sps, pulse, bank, bn, zeta, ted, lock geometry) is kept;
   * only the running state is cleared, so a re-run of the same stream from a
   * reset object reproduces its first-run symbols bit for bit.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> first = np.array(rs.steps(x))
   * >>> rs.reset()
   * >>> rs.ctrl, rs.locked           # back to the post-create state
   * (0.0, False)
   * >>> bool(np.array_equal(first, np.array(rs.steps(x))))  # reproducible
   * True
   *
   * @endcode
   */
  void dp_ratesync_reset (dp_ratesync_state_t *state);

  /** @brief Hand one input to the estimator while the loop is not tracking,
   *         and seed the loop from the block that passes.
   *
   *  Called by ratesync_step_ted() for every input taken outside tracking.
   *  The first input of an acquisition resets the estimator, so its `eye`
   *  counts from there; a block of the configured length is closed, and one
   *  whose quality is at or above the configured figure goes to
   *  dp_ratesync_seed()'s path with the loop's held rate kept. A block that
   *  does not pass is dropped and the next one starts.
   *
   *  @param state  Must be non-NULL, with acquisition configured.
   *  @param x      The input just taken. */
  void dp_ratesync_acquire_push (dp_ratesync_state_t *state, float _Complex x);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  /** @brief Fold one terminal-stage output into the timing loop.
   *
   *  The whole of the loop's per-output work, and the reason the loop is a
   *  struct of its own: it never touches the cascade, so a receiver that owns
   *  its cascade inside a DDC drives this with exactly the same call RateSync
   *  makes.
   *
   *  @param s      Loop state. Must be non-NULL.
   *  @param y      One terminal-stage output.
   *  @param y_out  Receives the symbol when the return is 1.
   *  @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *                for a specialised (branch-free) instantiation.
   *  @return 1 if this output was an on-time strobe that produced a symbol. */
#ifndef __cplusplus
  /* C only: the body uses the C99 complex library, which C++ does not have
     (dp_complex.h, "From C++"). A C++ caller uses dp_ratesync_steps(). */
  JM_FORCEINLINE JM_HOT int
  ratesync_loop_take_output (ratesync_loop_t *s, float _Complex y,
                             float _Complex *y_out, int ted)
  {
    /* Newest-first ring: ring[0] is this output, ring[m/2] the gate. */
    const size_t half = s->m >> 1;
    for (size_t i = s->ring_n < half ? s->ring_n : half; i > 0; i--)
      s->ring[i] = s->ring[i - 1];
    s->ring[0] = y;
    if (s->ring_n <= half)
      s->ring_n++;

    /* A seed in progress. Its control is on the accumulator while the
       first m outputs after it are emitted (each sets the interval to the
       next), then the held rate returns; the loop stays open for m more,
       until the gate a strobe reads is past the stretch. Counted on every
       output, the ones the fill drops included: the terminal stage emitted
       them under the same control. */
    if (JM_UNLIKELY (s->seed_left) && --s->seed_left == s->m)
      s->ctrl = s->lf.integ * s->term_rate;

    if (s->prime_left)
      {
        /* The cascade is still filling: these outputs are the delay lines,
           not the signal. Drop them without advancing the strobe phase, so
           the first real strobe starts a clean count. */
        s->prime_left--;
        return 0;
      }

    size_t phase = s->out_count++ % s->m;
    if (phase != 0 || s->ring_n <= half)
      return 0; /* not a strobe, or the gate is not yet in the ring */

    const float _Complex on  = y;
    const float _Complex mid = s->ring[half];
    if (JM_UNLIKELY (s->state != RATESYNC_TRACKING || s->seed_left))
      {
        /* Open: the symbol clock runs on at the rate the loop holds and
           the symbol is emitted, one a symbol period in every state. */
        s->have_prev = 1;
        s->prev_on   = on;
        *y_out       = on;
        return 1;
      }
    if (!s->have_prev)
      {
        s->have_prev = 1;
        s->prev_on   = on;
        return 0;
      }

    double num;
    if (ted == RATESYNC_TED_DTTL)
      num = dttl_ted (mid, on, s->prev_on);
    else
      num = gardner_ted (mid, on - s->prev_on);

    double on_pwr
        = (double)(crealf (on) * crealf (on) + cimagf (on) * cimagf (on));
    double mid_pwr
        = (double)(crealf (mid) * crealf (mid) + cimagf (mid) * cimagf (mid));
    /* `ref` is the lock statistic's normaliser, and ONLY that — it is an
       instantaneous ratio, so it needs no averaging and cannot go stale. */
    double ref = on_pwr + mid_pwr;

    /* The detector's own slope, divided out by a construct-time reciprocal.
       Amplitude does not appear: it enters the raw error as A^2 (Gardner) or
       A^1 (DTTL), and a unity-gain matched cascade delivers the amplitude it
       was sent, so levelling the signal is an AGC's job upstream — not a
       running estimate inside the detector. Transition density does not
       appear either; it is data, and whatever slope it yields is the honest
       slope.
         What this replaces was a 1%-per-symbol average of |on|^2+|mid|^2.
       Two things were wrong with it. It is an A^2 quantity, so it was right
       for Gardner's amplitude law and left DTTL's gain proportional to 1/A —
       a 4x swing over a 4x level change, in the detector BPSK selects. And
       being an average, it lagged: seeded on the first post-prime strobe,
       which lands in the cascade's amplitude ramp, it ran the loop at up to
       thousands of times its designed gain for exactly the interval that
       decides acquisition. Measured, that wound the integrator past pull-in
       and cost 7000-25000 symbols to recover across a 0.3-symbol-wide band
       of initial offsets; with the lag gone that band acquires like every
       other offset, inside the first few lock decisions (the validation
       report's §2.4 measures the declare at each offset). */
    double e      = num * s->ted_scale;
    s->last_error = e;

    /* dp_loop_filter_step returns a correction in symbols per symbol; `ctrl` is
       a rate deviation the TERMINAL stage adds to its accumulator once per
       one of ITS OWN inputs — not once per cascade input. Those differ by the
       whole integer decimation in front, so scaling by the cascade rate m/sps
       under-drives the loop by exactly that factor (32x at sps=64 behind a
       CIC(32), which is why it could barely track).
         Over one symbol the terminal stage sees N = m/rate_term inputs, so
       the accumulator gains N*ctrl output periods = N*ctrl/m symbols; setting
       that equal to the requested correction gives ctrl = correction *
       rate_term, with no reference to sps or the decimation at all.
       e > 0 means the strobe is LATE and a positive ctrl advances it — the
       classic Gardner polarity. */
    s->ctrl = dp_loop_filter_step (&s->lf, e) * s->term_rate;

    /* Tracked samples/symbol from the loop INTEGRATOR, not the instantaneous
       control. The integrator is the rate memory (loop_filter_core.h: "kp*e
       is the instantaneous phase nudge"); feeding the noisy total through the
       convex 1/(1+integ) instead biases the estimate high by Jensen. Exact
       here: a loop tracking true sps settles at integ = sps/true - 1, so
       sps/(1 + integ) == true. */
    double inst = s->sps / (1.0 + s->lf.integ);
    double lo_r = 0.5 * s->sps, hi_r = 1.5 * s->sps;
    if (!(inst > lo_r))
      inst = lo_r;
    else if (inst > hi_r)
      inst = hi_r;
    s->rate_est = inst;

    /* Lock statistic: the Gardner eye-opening ratio
       2*(|on|^2 - |mid|^2)/(|on|^2 + |mid|^2) — an open eye means the on-time
       strobe carries the energy and the transition gate sits near zero.
       Block-averaged over `avgs` looks before the decision (a tumbling
       window, so verify counts stay independent), mirroring symsync/dll. */
    double lock_signal = 2.0 * (on_pwr - mid_pwr) / (ref + RATESYNC_LOCK_EPS);
    s->lock_sum += lock_signal;
    if (++s->lock_count >= s->avgs)
      {
        s->lock_stat  = s->lock_sum / (double)s->avgs;
        const int was = s->lock.locked;
        const int now = dp_lockdet_step (&s->lock, s->lock_stat);
        s->lock_sum   = 0.0;
        s->lock_count = 0;
        if (JM_UNLIKELY (s->acquires))
          dp_ratesync_loop_decide (s, was, now);
      }

    s->prev_on = on;
    *y_out     = on;
    return 1;
  }
#endif /* !__cplusplus */

  /**
   * @brief Per-input timing step with the TED selection as a parameter.
   *
   * The workhorse behind ratesync_step()/dp_ratesync_steps(). Pushes one input
   * through the cascade at the current control deviation, which emits **up
   * to two** terminal-stage outputs for that one input: `rate = m/sps <= 1`
   * bounds it at two, and a terminal rate at or near 1.0 — what an integer
   * `sps` plans — reaches that bound whenever the control has pushed the
   * accumulator over. Taking only the first would permanently shift the
   * strobe parity, which is why the output buffer holds several and this
   * function drains all of them.
   *
   * Every m-th output is an on-time strobe and the output m/2 back is the
   * transition gate. On a strobe the TED compares the two, the PI loop
   * steers the next control, and the on-time sample is the recovered symbol.
   * With m >= 2 at most one strobe can fall among a single input's outputs,
   * so returning one symbol per input is still correct.
   *
   * Passing a literal @p ted lets the force-inlined body constant-fold the
   * detector branch away, exactly as symsync_step_ted() does.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @param ted    RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal
   *               for a specialised (branch-free) instantiation.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
#ifndef __cplusplus
  /* C only: calls ratesync_loop_take_output(), which is C only (dp_complex.h,
     "From C++"). A C++ caller uses dp_ratesync_steps(). */
  JM_FORCEINLINE JM_HOT int
  ratesync_step_ted (dp_ratesync_state_t *s, float _Complex x,
                     float _Complex *y_out, int ted)
  {
    /* One input can complete MORE THAN ONE output period. It happens
       whenever the terminal stage's own rate is at or near 1.0 (a cascade
       like HB + Resampler(1.0), which is what an integer sps plans) and the
       control has pushed the accumulator over: that input emits two. Asking
       for only one silently DROPS the second, which permanently shifts the
       strobe parity and leaves the loop sliding — measured as `rate_est`
       walking monotonically away while the eye never opens. The cascade rate
       is m/sps <= 1, so an input can complete at most two output periods, and
       with m >= 2 those can contain at most one on-time strobe: the
       single-symbol return of this function is still correct. */
    float _Complex ys[4];
    int            n_pre = 0;
    size_t         n     = dp_RateConverter_execute_ctrl_push_tap (
        s->mf, x, s->loop.ctrl, ys, sizeof (ys) / sizeof (ys[0]), NULL,
        &n_pre);
    /* Where the stream is, for a seed: the inputs taken and the ones the
       terminal stage has been handed (dp_ratesync_seed()). */
    s->in_count++;
    s->term_in += (uint64_t)n_pre;
    int emitted = 0;
    for (size_t oi = 0; oi < n; oi++)
      emitted |= ratesync_loop_take_output (&s->loop, ys[oi], y_out, ted);
    if (JM_UNLIKELY (s->loop.state != RATESYNC_TRACKING))
      dp_ratesync_acquire_push (s, x);
    return emitted;
  }
#endif /* !__cplusplus */

  /**
   * @brief Per-input timing step (the inline composition API).
   *
   * The public form of ratesync_step_ted(): dispatches on the configured
   * detector and flushes telemetry when attached.
   *
   * @param s      State. Must be non-NULL.
   * @param x      One input sample.
   * @param y_out  Receives the symbol when the return is 1.
   * @return 1 if a symbol was emitted (into @p y_out), 0 otherwise.
   */
#ifndef __cplusplus
  /* C only: calls ratesync_loop_take_output(), which is C only (dp_complex.h,
     "From C++"). A C++ caller uses dp_ratesync_steps(). */
  JM_FORCEINLINE JM_HOT int
  ratesync_step (dp_ratesync_state_t *s, float _Complex x, float _Complex *y_out)
  {
    int r = ratesync_step_ted (s, x, y_out, s->loop.ted);
    if (r && s->loop.tlm.ctx)
      dp_ratesync_loop_tlm_flush (&s->loop);
    return r;
  }
#endif /* !__cplusplus */

  /** @brief Output-buffer hint for the generated binding; 0 means "the input
   *  length is already a safe bound" — with `sps >= m >= 2` a block can never
   *  yield more symbols than it has samples (mirrors symsync).
   *  @param state  Unused.
   *  @return 0. */
  size_t dp_ratesync_steps_max_out (dp_ratesync_state_t *state);

  /**
   * @brief Recover symbols from a block of oversampled cf32 baseband.
   *
   * ratesync_step() in a loop, with the TED specialised per detector; state
   * carries across calls, so contiguous blocks give the same symbols as one
   * large block.
   *
   * @param state    Must be non-NULL.
   * @param x        Input samples.
   * @param x_len    Number of inputs.
   * @param out      Recovered symbols.
   * @param max_out  Capacity of @p out.
   * @return Symbols written to @p out.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> y = rs.steps(x)             # one symbol per transmitted symbol
   * >>> round(rs.rate, 2)           # tracked samples per symbol
   * 8.0
   * >>> bool(rs.lock_stat > 0.55)   # the timing loop has locked
   * True
   *
   * @endcode
   */
  size_t dp_ratesync_steps (dp_ratesync_state_t *state, const float _Complex *x,
                         size_t x_len, float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Properties / configuration
   * ------------------------------------------------------------------ */

  /**
   * @brief Recompute the loop gains for a new bandwidth/damping, keeping the
   *        timing estimate.
   *
   * Only the PI coefficients change; the integrator, and therefore the tracked
   * rate and the lock, carries through untouched. Use it to narrow the loop
   * after acquisition (a wide @p bn pulls in fast, a narrow one tracks with
   * less jitter) without forcing a re-acquire.
   *
   * @param state  Must be non-NULL.
   * @param bn     Loop noise bandwidth, normalised to the symbol rate.
   * @param zeta   Damping factor (1.0 = critical; 0.707 usual).
   * @return DP_OK, or DP_ERR_INVALID when (@p bn, @p zeta) is outside
   *         dp_loop_filter_params_ok(); nothing changes then (Python
   *         raises ValueError).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)  # 8 samp/sym
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> _ = rs.steps(x)              # acquire and lock
   * >>> rs.locked
   * True
   * >>> rs.configure(0.002, 0.707)   # narrow the loop; lock is kept
   * >>> round(rs.bn, 3)
   * 0.002
   * >>> rs.locked
   * True
   *
   * @endcode
   */
  int    dp_ratesync_configure (dp_ratesync_state_t *state, double bn, double zeta);
  /** @brief Timing-loop noise bandwidth, normalised to the symbol rate.
   *  Writing it re-derives the loop gains at the damping already in use,
   *  exactly as configure() does.
   *  @param state  Must be non-NULL.
   *  @return bn: the create() value or the last one configure() accepted. */
  double dp_ratesync_get_bn (const dp_ratesync_state_t *state);
  /** @brief Symbols emitted before the timing has settled: where a
   *  steady-state measurement of the output may start.
   *
   *  The timing loop's own figure, dp_ratesync_loop_settle_time(): the time
   *  the closed loop takes to bring a starting timing error under 2% of
   *  itself, from the bandwidth and damping in use, 2.59 / bn symbols at
   *  the default damping. It is known at construction and follows
   *  configure(). `locked` says a statistic crossed a threshold, which
   *  happens while the timing is still moving; this says when it has
   *  stopped. With the acquisition on
   *  (dp_ratesync_configure_acquisition()) one attempt is added ahead of
   *  it: the estimator's lead-in, the block, and the two symbols a seed
   *  keeps the loop open.
   *
   *  Example (Python):
   *  >>> from doppler.track import RateSync
   *  >>> rs = RateSync(sps=8.0, bn=0.01)
   *  >>> round(rs.settle_time)
   *  259
   *  >>> rs.bn = 0.005
   *  >>> round(rs.settle_time)
   *  519
   *
   *  @param state  Must be non-NULL.
   *  @return Symbols, from the first one emitted; infinity at `bn = 0`. */
  double dp_ratesync_get_settle_time (const dp_ratesync_state_t *state);
  /** @brief Retune bn at the current zeta, through dp_ratesync_configure().
   *
   *  An out-of-domain value is refused and nothing changes, so
   *  dp_ratesync_get_bn() keeps reporting the bandwidth in use (Python
   *  raises ValueError).
   *
   *  @param state  Must be non-NULL.
   *  @param val    New loop noise bandwidth, normalized to the symbol
   *                rate.
   *  @return DP_OK, or DP_ERR_INVALID when @p val is refused. */
  int    dp_ratesync_set_bn (dp_ratesync_state_t *state, double val);

  /** @brief Last normalised TED error — the loop stress.
   *  @param state  Must be non-NULL.
   *  @return The detector output scaled by its own slope (dimensionless);
   *          0 before the first steered symbol. */
  double dp_ratesync_get_timing_error (const dp_ratesync_state_t *state);

  /** @brief Smoothed tracked samples per symbol. Departs from the nominal
   *         `sps` by exactly the sample-clock offset being tracked, so it is
   *         the estimator a rate-disciplining caller reads.
   *  @param state  Must be non-NULL.
   *  @return Samples per symbol, sps / (1 + loop integrator), clamped to
   *          0.5 sps to 1.5 sps; the nominal sps after create/reset. */
  double dp_ratesync_get_rate (const dp_ratesync_state_t *state);

  /** @brief Current per-input control deviation steering the strobe.
   *  @param state  Must be non-NULL.
   *  @return Rate deviation added to the terminal stage's rate (same
   *          units as that rate); 0 after create/reset. */
  double dp_ratesync_get_ctrl (const dp_ratesync_state_t *state);

  /** @brief Last block-averaged lock statistic (the eye-opening ratio).
   *
   *  This, not an error-vector magnitude, is the honest lock indicator: a
   *  single cycle slip during acquisition drags a windowed EVM by 20 dB while
   *  the eye stays wide open at +0.75. Judge lock here.
   *  @param state  Must be non-NULL.
   *  @return Mean lock_signal over the last completed block of avgs
   *          looks; 0.0 until the first block completes. */
  double dp_ratesync_get_lock_stat (const dp_ratesync_state_t *state);

  /** @brief Current lock decision (1 = locked), verify-counted.
   *
   *  One decision per block of 133 looks: the block's mean lock statistic
   *  against 0.311, the threshold symsync sizes at a per-decision pfa of
   *  1e-3 and pd of 0.9 (rolloff 0.35, Es/N0 10 dB). The lock declares after
   *  2 consecutive decisions above it and drops after 6 below it: the counts
   *  dp_det_verify_count() gives for those rates at a per-decision
   *  false-transition budget of 1e-6. Those rates are the sizing point's;
   *  at another Es/N0 the counts meet the budget only if the statistic's
   *  rates do, which a caller measures, and dp_ratesync_configure_lock_raw()
   *  sets other counts.
   *  @param state  Must be non-NULL.
   *  @return 1 when locked, 0 otherwise (0 after create/reset). */
  int dp_ratesync_get_locked (const dp_ratesync_state_t *state);

  /** @brief Is the symbol just emitted valid?
   *
   *  The lock flag, while the loop is tracking and no seed is in progress.
   *  With no acquisition configured it is dp_ratesync_get_locked(). Every
   *  symbol period emits a symbol in every state, and this is the flag
   *  beside it: read it after each ratesync_step() that returns 1.
   *
   *  @param state  Must be non-NULL.
   *  @return 1 or 0. */
  int dp_ratesync_get_valid (const dp_ratesync_state_t *state);

  /** @brief Where the timing loop is: acquiring, tracking or coasting.
   *  @param state  Must be non-NULL.
   *  @return A ratesync_lifecycle_t; RATESYNC_TRACKING always, with no
   *          acquisition configured. */
  int dp_ratesync_get_lifecycle (const dp_ratesync_state_t *state);

  /**
   * @brief Turn the built-in acquisition on, or off.
   *
   * On, the object starts by estimating: a feedforward timing estimate
   * over @p symbols symbols on a matched filter of its own
   * (dp_timing_est_create_matched()), repeated until one has a quality of
   * @p quality or more, then handed to the loop as a seed. The loop closes
   * on it, so the time to a closed loop is a count: the estimator's
   * lead-in, then @p symbols symbols an attempt. A declared lock that
   * drops starts it again (dp_ratesync_set_presence() says from which
   * rate). dp_ratesync_get_settle_time() counts one attempt.
   *
   * Both figures are the caller's. The block's length sets the estimate's
   * error and the threshold its false-alarm probability, and neither has a
   * derived default yet: the estimator's validation report
   * (`src/doppler/estimation/tests/validation/timing_est/results.md`)
   * gives the error and the pass rate against both.
   *
   * If the lock is not declared when this is called the loop is put in
   * acquiring at once. Off (@p symbols 0), the loop is in tracking and
   * stays there, which is the object as it was built.
   *
   * @param state    Must be non-NULL.
   * @param symbols  Symbols an attempt estimates over; 0 turns it off.
   * @param quality  The estimate's `quality` at or above which it seeds;
   *                 over 0 and under 1. Ignored when @p symbols is 0.
   * @return DP_OK, or DP_ERR_INVALID with nothing changed: @p quality out
   *         of range, or an `sps` under TIMING_EST_MATCHED_SPS, where the
   *         estimator's matched filter cannot be built.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> rs.configure_acquisition(128, 0.05)
   * >>> rs.lifecycle
   * 'acquiring'
   * >>> y = rs.steps(x)
   * >>> rs.lifecycle, rs.valid
   * ('tracking', True)
   *
   * @endcode
   */
  int dp_ratesync_configure_acquisition (dp_ratesync_state_t *state,
                                      size_t symbols, double quality);

  /**
   * @brief Seed the loop from a timing a caller already has.
   *
   * @p eye is an eye center on the input's timeline: the index, counted
   * from create or reset and not whole, of a sample instant where a symbol
   * is to be read. Other eye centers are whole symbols either side, so any
   * one of them will do, past or future. The object finds how far the
   * terminal stage's next output is from one (the stage's accumulator and
   * the cascade's group delay say where that output reads) and seeds the
   * loop with it, dp_ratesync_loop_seed(). The built-in acquisition makes
   * this call with its estimate.
   *
   * @param state  Must be non-NULL.
   * @param eye    An eye center, input samples since create or reset.
   * @param rate   The symbol clock's offset to start the loop's rate state
   *               from, symbols a symbol: `sps / true_sps - 1`, 0 for the
   *               nominal clock. Magnitude under 0.5.
   * @return DP_OK, or DP_ERR_INVALID with nothing changed (a non-finite
   *         @p eye, a @p rate out of range).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = np.repeat(syms, 8)[5:].astype(np.complex64)  # edges at 3, 11, ...
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.0)  # does not steer
   * >>> rs.seed(6.5)                         # a symbol's center
   * >>> v = np.array(rs.steps(x)).real[4:-4]
   * >>> bool(np.mean(np.abs(v)) ** 2 / np.mean(v ** 2) > 0.99)  # in the eye
   * True
   *
   * @endcode
   */
  int dp_ratesync_seed (dp_ratesync_state_t *state, double eye, double rate);

  /** @brief The presence input as it stands.
   *  @param state  Must be non-NULL.
   *  @return The value last set, non-zero when the owner has said the
   *          signal is there; 0 after create or reset. */
  int dp_ratesync_get_presence (const dp_ratesync_state_t *state);

  /** @brief Set the presence input: dp_ratesync_loop_set_presence() on this
   *         object's loop.
   *
   *  Is the signal known to be there by evidence other than the symbol
   *  lock (a code lock and a carrier lock, in a DSSS receiver)? It decides
   *  where a lost symbol lock goes: coasting at the held rate with it,
   *  acquiring from a zero rate without. Lowering it during a coast ends
   *  the coast in acquiring.
   *
   *  @param state  Must be non-NULL.
   *  @param val    Non-zero: present. */
  void dp_ratesync_set_presence (dp_ratesync_state_t *state, int val);

  /** @brief Has the cascade's CIC stage clipped its input since the last
   *         reset? Forwarded from the RateConverter: a CIC bounds its input
   *         to +-2.0 and clips silently past that, which no timing metric
   *         reveals. Always 0 when the plan has no CIC stage.
   *  @param state  Must be non-NULL.
   *  @return 1 if the CIC has clipped since the last reset, else 0. */
  int dp_ratesync_get_clipped (const dp_ratesync_state_t *state);

  /**
   * @brief Set the lock detector's geometry directly.
   *
   * The block size (avgs), a split declare/drop threshold pair on lock_stat
   * (level hysteresis) and both verify counts (time hysteresis). Re-tuning
   * clears the in-flight block sum and drops the lock, so the next decision
   * uses only looks gathered under the new config.
   *
   * @param state        Must be non-NULL.
   * @param avgs         Looks per decision; clamped >= 1.
   * @param up_thresh    Declare threshold on lock_stat.
   * @param down_thresh  Drop threshold; <= up_thresh for level hysteresis.
   * @param n_up         Consecutive above-threshold decisions to declare.
   * @param n_down       Consecutive below-threshold decisions to drop.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import RateSync
   * >>> syms = np.where(np.random.default_rng(3).integers(0, 2, 3000) > 0,
   * ...                 1.0, -1.0)
   * >>> x = (0.25 * np.repeat(syms, 8)).astype(np.complex64)
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> _ = rs.steps(x)
   * >>> rs.locked
   * True
   * >>> rs.configure_lock_raw(64, 0.5, 0.4, 2, 4)  # drops the lock
   * >>> rs.locked
   * False
   * >>> rs.lock_stat                 # the in-flight block was cleared
   * 0.0
   *
   * @endcode
   */
  void dp_ratesync_configure_lock_raw (dp_ratesync_state_t *state, size_t avgs,
                                    double up_thresh, double down_thresh,
                                    uint32_t n_up, uint32_t n_down);

  /**
   * @brief Attach (or detach) a telemetry context and register the probes.
   *
   * Registers six probes, emitted once per recovered symbol and further
   * thinned by @p decim: "<prefix>.e" (normalised TED error),
   * "<prefix>.ctrl" (the per-input control steering the strobe),
   * "<prefix>.rate" (tracked samples/symbol), "<prefix>.lock" (last
   * block-averaged lock_signal), "<prefix>.locked" (0/1) and "<prefix>.mu"
   * (the timing NCO's fractional phase — see dp_resamp_get_ctrl_acc()). Passing
   * NULL detaches. Setup path, never hot: the context is borrowed and must
   * outlive the attachment (SPSC rules in dp_tlm/dp_tlm_core.h).
   *
   * The three form one readable picture of the loop: `e` is what the detector
   * saw, `ctrl` is what the filter did about it, and `mu` is where the
   * sampling instant ended up as a result — the only one of the three that is
   * a physical position rather than a correction.
   *
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "sync".
   * @param decim  Emit every decim-th symbol; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         six probes (the attach fails whole; the object stays detached).
   * @code
   * >>> from doppler.track import RateSync
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)
   * >>> rs = RateSync(sps=8.0, pulse="iandd", m=4, bn=0.01)
   * >>> rs.set_telemetry(tlm, "sync")   # register the six timing probes
   * >>> tlm.probe_count
   * 6
   * >>> "sync.rate" in tlm.probe_names   # tracked samples/symbol
   * True
   *
   * @endcode
   */
  int dp_ratesync_set_telemetry (dp_ratesync_state_t *state, dp_tlm_t *tlm,
                              const char *prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ─────────
 * A composition of exactly two children now that the timing loop is its own
 * struct: `[hdr][RateConverter][ratesync_loop]`, each self-validating. All of
 * this object's own running state moved into the loop's blob, so it packs no
 * scalars of its own; config (sps/pulse/beta/span/m/num_phases/bn/zeta/ted) is
 * restored by create() and never packed. */
/** @brief State-blob magic for RateSync (FourCC "RATS"). */
#define RATESYNC_STATE_MAGIC DP_FOURCC ('R', 'A', 'T', 'S')
/** @brief RateSync blob layout version. */
#define RATESYNC_STATE_VERSION 3u /* v3: the acquisition (its two settings, the stream counts, the estimator's child blob); v2: running state moved into the loop */

  /** @brief Bytes dp_ratesync_get_state() writes (envelope + payload + child).
   *  @param state  Must be non-NULL.
   *  @return Blob size in bytes: envelope + RateConverter blob + loop
   *          blob. */
  size_t dp_ratesync_state_bytes (const dp_ratesync_state_t *state);
  /** @brief Serialize the mutable state into @p blob.
   *  @param state  Must be non-NULL.
   *  @param blob   Caller-owned buffer of at least
   *                dp_ratesync_state_bytes() bytes. */
  void dp_ratesync_get_state (const dp_ratesync_state_t *state, void *blob);
  /** @brief Restore mutable state from @p blob into an identically built
   *  instance.
   *  @param state  Must be non-NULL.
   *  @param blob   Blob from dp_ratesync_get_state().
   *  @return DP_OK, or DP_ERR_INVALID if any envelope rejects. */
  int dp_ratesync_set_state (dp_ratesync_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* RATESYNC_CORE_H */
