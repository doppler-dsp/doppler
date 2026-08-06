/**
 * @file mpsk_rx_loops.h
 * @brief The two loops an M-PSK receiver closes, independent of its front end.
 *
 * Everything a receiver does *after* its down-converter emits a terminal-stage
 * output: the symbol-timing loop, the carrier loop, the acquisition/tracking
 * handover, and the slicer/demapper state. It owns no filter, no NCO and no
 * cascade — it consumes matched-filtered outputs and produces the two control
 * values that steer whatever produced them.
 *
 * ```
 *   front end (Ddc / Ddcr)                    mpsk_rx_loops_t
 *   ─────────────────────                     ───────────────
 *   LO mix ─ cascade ─ matched filter ──y──>  carrier disc ──> freq_ctrl ─┐
 *      ^                    ^                 timing TED  ──> rate_ctrl ─┤
 *      └────────────────────┴──────────────────────────────────────────── ┘
 * ```
 *
 * That split is the whole reason two receiver types cost barely more than one.
 * MpskReceiver drives a `Ddc` (complex input) and MpskReceiverR drives a
 * `Ddcr` (real input, halfband R2C front end); both call the same
 * mpsk_rx_take_output() on every output their front end emits, so the loops
 * are one implementation rather than two peers that can drift apart. The
 * timing half is literally RateSync's — @ref ratesync_loop_t, embedded here —
 * so a fix to the TED or its normaliser reaches RateSync and both receivers at
 * once.
 *
 * ## Both discriminators live on the symbol strobe
 *
 * **Acquisition** uses the M-th-power NDA error (@ref carrier_nda_disc) and
 * **tracking** uses a decision-directed one, but both read the same sample:
 * the on-time strobe. Only that sample is a constellation point — the other
 * terminal outputs fall between symbols, where the matched filter is averaging
 * two of them, so their M-th power carries ISI rather than carrier phase.
 *
 * This costs less than it appears to. The strobe fires every `m_out`-th output
 * whatever the timing loop currently believes, so the NDA loop still pulls in
 * before timing lock — it just does so on one consistent phase of the pulse
 * instead of all of them. And because both discriminators then share one
 * update rate, the handover is a pure discriminator swap: no update period
 * changes, so the loop filter's integrator — which holds a phase command per
 * update — carries the frequency estimate across untouched, in both
 * directions.
 *
 * ## Units
 *
 * `bn_carrier` and the timing loop's `bn` are both normalised to the **symbol
 * rate**, so one setting means the same thing at every input rate — the same
 * argument RateSync makes for referencing its control to the terminal stage.
 * The discriminators produce a phase error in radians; `freq_ctrl` must be
 * cycles per sample **at the LO's own rate**, which is the input rate for a
 * complex front end and half it for a real one (the halfband decimates before
 * the LO). Each receiver reports its own `lo_sps` for that reason, and the
 * conversion lives in `freq_scale`.
 */
#ifndef MPSK_RX_LOOPS_H
#define MPSK_RX_LOOPS_H

#include "agc/agc_core.h"
#include "carrier_nda/carrier_nda_core.h" /* carrier_nda_disc            */
#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include "ratesync/ratesync_core.h" /* ratesync_loop_t — the timing half */
#include "dp_tlm/dp_tlm_core.h"
#include <complex.h>
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Matched-filter pulse shape. Aliases of the cascade's own
   *  vocabulary so one set of names covers the whole family. */
  enum
  {
    MPSK_RX_PULSE_IANDD = RC_PULSE_IANDD, /**< rectangular / NRZ.        */
    MPSK_RX_PULSE_RRC   = RC_PULSE_RRC    /**< root-raised cosine.       */
  };

/* Numerical guard on the symbol magnitude in the decision discriminator. */
#define MPSK_RX_EPS 1e-12

/* Arm-AGC bandwidth, per SYMBOL. carrier_nda ties its own AGC to
 * 0.01*bn_carrier, but that ratio was chosen for a loop updating every input
 * SAMPLE; with both loops here referenced to the symbol rate the same
 * expression is a time constant of thousands of symbols, which never
 * converges over a realistic burst. The AGC still has to stay slower than the
 * carrier loop -- it must track the signal level and never the carrier
 * dynamics or the within-symbol pulse envelope -- so it gets its own constant,
 * a few times below the usable bn_carrier range, and the first-strobe seed in
 * mpsk_rx_take_output() carries the cold start it is now too slow to handle.
 */
#define MPSK_RX_AGC_BW 0.002

/* Usable samples averaged to seed the arm AGC's gain (see mpsk_rx_disc).
 * One sample is a draw from a random variable and at MPSK_RX_AGC_BW a wrong
 * seed does not recover inside a short burst; a handful removes that
 * variance. Kept well under the default warmup_syms (30) so seeding always
 * finishes before the handover can fire. */
#define MPSK_RX_AGC_SEED_SAMPS 8

/* Default matched-filter bank arms — the timing resolution is 1/this of an
 * output period. What a composing C caller (the DSSS receivers) passes when it
 * has no reason to want anything else; the Python default matches. */
#define MPSK_RX_NUM_PHASES 1024u

/* Default terminal outputs per symbol — where an I&D matched filter reaches
 * the coherent bound (see mpsk_receiver_create's @p m_out for the
 * measurements). Same role as MPSK_RX_NUM_PHASES above: what a composing C
 * caller passes when it has no reason to want anything else, so the composed
 * receivers do not each carry their own copy of the number.
 * MUST match `default` on m_out in objects/mpsk_receiver{,_r}.toml, which is
 * what the Python binding uses. */
#define MPSK_RX_M_OUT_DEFAULT 8

/* Two-way handover rule (see mpsk_rx_loops_init's lock_thresh doc).
 * Declare fast / drop reluctantly: 8 straight above-threshold symbols hand
 * the carrier to the decision-directed discriminator; 32 straight below the
 * 0.8x drop threshold fall back to the NDA acquisition steer. The asymmetry
 * reflects the cost asymmetry — a premature declare on a bad lock derails the
 * NCO (decision errors feed back), while a late drop merely keeps a noisier
 * discriminator a few symbols longer. */
#define MPSK_RX_HANDOVER_DOWN 0.8
#define MPSK_RX_HANDOVER_N_UP 8u
#define MPSK_RX_HANDOVER_N_DOWN 32u

  /**
   * @brief Where the NDA carrier discriminator reads from.
   *
   * An M-th-power discriminator updating at rate `F` can only observe a
   * frequency error of `|df| < F/(2M)` — above that its M-th-power phase
   * advances more than pi per update and the error folds. So the tap point IS
   * the pull-in range, and it trades directly against signal quality:
   *
   * | tap        | update rate | unambiguous \|df\|  | cost                  |
   * | ---------- | ----------- | ------------------- | --------------------- |
   * | `STROBE`   | `Rs`        | `Rs/(2M)`           | needs symbol timing   |
   * | `MF_ALL`   | `m_out*Rs`  | `m_out*Rs/(2M)`     | inter-symbol ISI bias |
   * | `LO_ARM`   | LO rate     | `f_lo/(2M)`         | no matched filtering  |
   *
   * There is a second axis, and it is the one the cascade rebuild lost.
   * `STROBE` is the only tap that depends on **symbol timing**: it reads the
   * one output the timing loop nominates, so before timing lock it is reading
   * an arbitrary phase of the pulse. `MF_ALL` consumes every terminal output
   * and so does not care which one is on-time; `LO_ARM` runs a free-running
   * arm filter ahead of the cascade entirely. Both therefore restore the
   * property the NDA path exists for — acquiring with no data *and no symbol
   * timing* — which is why they are not merely "wider".
   *
   * No tap waits. `STROBE` steers from its first strobe whether or not the
   * timing loop has declared, so its dependency is a reason to CHOOSE another
   * tap when the carrier must acquire before timing does — not something the
   * receiver resolves behind the caller's back. Gating it was tried and
   * measured: across a 24-cell sweep it moved one cell, because what it
   * really bought was a carrier transient that started at a known instant and
   * was therefore easier to measure.
   *
   * Fixed at construction: the caller picks the trade once, and nothing
   * switches underneath it.
   */
  enum
  {
    /** On-time strobe only. Cleanest input, narrowest pull-in, and the only
     *  tap whose input quality depends on symbol timing. */
    MPSK_RX_NDA_TAP_STROBE = 0,
    /** Every terminal output. `m_out`x the range, timing-independent, but the
     *  outputs between symbols are averaging two of them, so their M-th power
     *  carries an ISI bias — worst where the decision margin is smallest. */
    MPSK_RX_NDA_TAP_MF_ALL = 1,
    /** Post-LO, pre-cascade, through a free-running boxcar arm. Widest range
     *  and fully timing-independent, but unmatched: the arm is a short
     *  lowpass, not the pulse filter, so it pays squaring loss. This is the
     *  classic Costas arm-filter placement. */
    MPSK_RX_NDA_TAP_LO_ARM = 2
  };

/* Free-running arm window for MPSK_RX_NDA_TAP_LO_ARM, as a fraction of a
 * symbol at the LO's rate. A half-symbol boxcar is the classic passive arm
 * filter (Yuen Eq. 8-19 is derived for exactly this window) — long enough to
 * buy back some SNR, short enough that it still sees mostly ONE symbol, which
 * is what keeps the tap timing-independent. Clamped into [1, BOXCAR_MAX_LEN].
 */
#define MPSK_RX_ARM_DIV 2u

  /**
   * @brief Telemetry attachment for the receiver's own two probes; the timing
   *        and carrier probes ride their own sub-attachments.
   */
  typedef struct
  {
    dp_tlm_t *ctx;         /**< NULL = detached                        */
    int32_t   id_lock;     /**< "<prefix>.lock"     — carrier lock EMA */
    int32_t   id_tracking; /**< "<prefix>.tracking" — handover 0/1     */
    int32_t   id_e;        /**< "<prefix>.car.e"    — carrier disc     */
    int32_t   id_freq;     /**< "<prefix>.car.freq" — tracked carrier  */
    int32_t   id_locked;   /**< "<prefix>.car.locked" — lockdet flag   */
  } mpsk_rx_tlm_t;

  /**
   * @brief The receiver's loops: timing, carrier, handover, demapper.
   */
  typedef struct
  {
    ratesync_loop_t timing; /**< the shared timing loop -> rate_ctrl.    */

    /* ── carrier loop ────────────────────────────────────────────────── */
    loop_filter_state_t car_lf;  /**< 2nd-order carrier PI loop.          */
    agc_state_t         car_agc; /**< unit-power normaliser feeding the
                                      M-th-power discriminator.           */
    double freq_ctrl;   /**< carrier command now applied, cycles/sample at
                             the LO's own rate.                           */
    double freq_scale;  /**< loop-filter output -> freq_ctrl; rad/symbol
                             to cycles per LO sample, set once at init.   */
    double car_error;   /**< last carrier phase discriminator (stress).   */
    double lock;        /**< EMA of the carrier lock signal.              */
    lockdet_state_t car_lock;   /**< de-chattered binary carrier lock.    */
    int             agc_seeded; /**< arm AGC has taken its first level.   */
    boxcar_state_t  arm;        /**< free-running arm filter; LO_ARM only. */

    /* ── config (restored by the owner's create(), never packed) ─────── */
    int    m;          /**< constellation order M (2, 4, 8).             */
    double sps;        /**< samples per symbol at the receiver's input.  */
    double lo_sps;     /**< samples per symbol at the LO's own rate.     */
    size_t m_out;      /**< terminal outputs per symbol.                 */
    double bn_carrier; /**< carrier loop noise bandwidth (per symbol).   */
    double zeta;       /**< damping factor for both loops.               */
    int    nda_tap;    /**< MPSK_RX_NDA_TAP_* — where the NDA disc reads. */
    int    tap_timed;  /**< tap depends on symbol timing (STROBE only).  */

    /* ── acquisition <-> tracking handover ───────────────────────────── */
    int    acq_to_track; /**< opt-in two-way handover.                   */
    size_t warmup_syms;  /**< symbols before the switch is allowed.      */
    size_t sym_count;    /**< symbols emitted (warmup counter).          */
    lockdet_state_t handover; /**< verify-counted declare/drop rule.     */
    int             tracking; /**< 0 = NDA acquire, 1 = decision.        */

    /* ── demapper ────────────────────────────────────────────────────── */
    int           differential;  /**< bits(): differential demap.        */
    int           have_prev_idx; /**< differential: prev_idx valid.      */
    unsigned      prev_idx;      /**< differential: prev sliced index.   */
    float complex sym_rot;       /**< exp(j*phi0): NDA grid -> slicer.   */

    mpsk_rx_tlm_t tlm; /**< live attachment; zeroed in state blobs.      */
  } mpsk_rx_loops_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Initialise the loops in place (no allocation).
   *
   * @param l             Loops to initialise. Must be non-NULL.
   * @param m             Constellation order M (2, 4, 8).
   * @param sps           Samples per symbol at the receiver's input.
   * @param lo_sps        Samples per symbol at the LO's own rate: `sps` for a
   *                      complex front end, `sps/2` for a real one, whose
   *                      halfband decimates before the LO.
   * @param m_out         Terminal outputs per symbol (even, >= 2).
   * @param bn_carrier    Carrier loop noise bandwidth, per symbol.
   * @param zeta          Damping factor for both loops.
   * @param bn_timing     Timing loop noise bandwidth, per symbol.
   * @param ted           RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL.
   * @param acq_to_track  Enable the two-way NDA<->decision handover.
   * @param lock_thresh   Handover declare threshold on the carrier lock EMA;
   *                      the drop threshold sits at MPSK_RX_HANDOVER_DOWN x
   *                      it, and both directions are verify-counted. The EMA's
   *                      H0 sd is CARRIER_NDA_LOCK_NORM_SD (0.1132) for every
   *                      M, so this divided by that is the threshold in noise
   *                      sigmas and its per-look Pfa is Q(that) — 0.5 is
   *                      4.42 sigma, Pfa 5e-6. See carrier_nda_core.h.
   * @param warmup_syms   Symbols before the handover is allowed.
   * @param differential  bits(): differential (rotation-invariant) demap.
   * @param nda_tap       MPSK_RX_NDA_TAP_* — where the NDA discriminator
   *                      reads, which sets its pull-in range and whether it
   *                      depends on symbol timing at all.
   */
  void mpsk_rx_loops_init (mpsk_rx_loops_t *l, int m, double sps,
                           double lo_sps, size_t m_out, double bn_carrier,
                           double zeta, double bn_timing, int ted,
                           int acq_to_track, double lock_thresh,
                           size_t warmup_syms, int differential, int nda_tap);

  /**
   * @brief How many times per symbol the chosen tap updates the carrier loop.
   *
   * The tap's whole point, as one number: it is both the factor by which the
   * discriminator's unambiguous frequency range grows over the symbol-rate
   * case, and the loop filter's update rate (see config_carrier).
   */
  JM_FORCEINLINE double
  mpsk_rx_updates_per_symbol (const mpsk_rx_loops_t *l)
  {
    if (l->nda_tap == MPSK_RX_NDA_TAP_MF_ALL)
      return (double)l->m_out;
    if (l->nda_tap == MPSK_RX_NDA_TAP_LO_ARM)
      return l->lo_sps;
    return 1.0;
  }

  /** @brief Re-seed both loops to their post-init state; keep configuration.
   *  @param l  Must be non-NULL. */
  void mpsk_rx_loops_reset (mpsk_rx_loops_t *l);

  /** @brief Re-tune the handover detector; see mpsk_receiver_configure_lock(),
   *  which forwards here. A live handover survives; the verify run restarts. */
  void mpsk_rx_configure_lock (mpsk_rx_loops_t *l, double up_thresh,
                               double down_thresh, uint32_t n_up,
                               uint32_t n_down);

  /** @brief Tracked carrier offset in cycles/sample at the LO's rate — the
   *  loop's own estimate, excluding the front end's configured centre. */
  double mpsk_rx_freq_est (const mpsk_rx_loops_t *l);

  /** @brief Overwrite the tracked carrier offset (cycles/sample at the LO's
   *  rate) so the next output de-rotates by exactly @p val. */
  void mpsk_rx_set_freq_est (mpsk_rx_loops_t *l, double val);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  /**
   * @brief Filter a carrier phase error and update `freq_ctrl`.
   *
   * **The negation is load-bearing.** A DDC mixes with its LO directly
   * (`x * lo_step_ctrl(...)`), where carrier_nda's older loop mixed with the
   * conjugate (`x * conjf(lo_step_ctrl(...))`), so the same physical
   * de-rotation is the opposite sign on this port. Without it the loop is
   * positive feedback and the M-th-power S-curve's stable and unstable
   * equilibria swap: the receiver locks *hard* onto the half-way grid, timing
   * and symbol count look perfect, and the only tell is the carrier lock
   * metric sitting at a steady **negative** value (-0.48 for QPSK, where +0.62
   * is a real lock) while every symbol lands on a decision boundary.
   */
  JM_FORCEINLINE JM_HOT void
  mpsk_rx_steer (mpsk_rx_loops_t *l, double pe)
  {
    l->car_error = pe;
    l->freq_ctrl = -loop_filter_step (&l->car_lf, pe) * l->freq_scale;
  }

  /**
   * @brief Run the NDA discriminator on one tapped sample.
   *
   * Shared by all three tap points so the discriminator, its AGC and its lock
   * statistic exist exactly once however the caller chose to feed them.
   *
   * The discriminator and its lock EMA run on every sample it is handed — the
   * drop-back rule needs them, and `lock` must stay observable throughout
   * acquisition. The steer runs whenever the loop is not already tracking.
   *
   * @param l        Loops.
   * @param z        The tapped sample.
   */
  JM_FORCEINLINE JM_HOT void
  mpsk_rx_disc (mpsk_rx_loops_t *l, float complex z)
  {
    /* Seed the AGC from the first usable sample rather than letting it walk
       there. Its bandwidth is deliberately ~100x below the carrier loop's so
       it tracks the signal level and never the carrier dynamics — a time
       constant of thousands of symbols, which is right for tracking drift and
       useless for a cold start. The cold error is not small: a matched-filter
       bank is unit-ENERGY, so its output sits ~sqrt(pulse_sps) above the
       constellation it came from, and since the discriminator's gain goes as
       |z|^m the loop would run 16x hot at QPSK and 256x at 8PSK (measured lock
       statistics of 5.2 and 26.4 against ceilings of 0.62 and 0.41).

       Seeding off a sample that is not yet a symbol is the same bug from the
       other end: the gain can latch far too LOW just as easily. Measured on a
       single-sample seed taken during acquisition: `lock` = 4.9e-19, a
       denormal, on a receiver decoding every bit correctly, so the handover
       never fired. The averaging below is what defends against this — nothing
       waits for symbol timing here (see the strobe tap in
       mpsk_rx_take_output), so the seed samples ARE acquisition samples.

       Average the first MPSK_RX_AGC_SEED_SAMPS usable samples instead of
       taking ONE. A single sample is a sample of a random variable: a strobe
       that straddles a transition, or an early one taken while a composing
       front end is still settling, latches a gain that is simply wrong, and
       at this bandwidth "wrong" is not self-correcting — the time constant is
       ~1/MPSK_RX_AGC_BW = 500 symbols, so a burst shorter than that never
       recovers. Measured in the DSSS stress sweep, which runs 700-symbol
       bursts: two trials in twelve latched off a bad first sample and stayed
       there, reporting `lock` = 5.59 and 0.18 against BPSK's ceiling of ~1.0
       (i.e. ~5.6x hot and ~5x cold) with BER 0.235 and 0.043. Averaging a
       handful of samples costs nothing and removes the variance that made it
       a coin flip.

       `agc_seeded` doubles as the counter (it is the only state this needs,
       so the serialized layout does not change: same field, same type, and a
       value below the target simply means "still averaging"). */
    if (l->agc_seeded < MPSK_RX_AGC_SEED_SAMPS)
      {
        double p0 = (double)(crealf (z) * crealf (z) + cimagf (z) * cimagf (z));
        if (p0 > 0.0)
          {
            /* Exact arithmetic mean of the usable samples seen so far. */
            int k = l->agc_seeded;
            l->car_agc.p_avg
                = (k == 0) ? p0
                           : l->car_agc.p_avg + (p0 - l->car_agc.p_avg) / (k + 1);
            l->car_agc.gain_db
                = l->car_agc.ref_db - 10.0 * log10 (l->car_agc.p_avg);
            l->car_agc.g_last
                = (float)agc_exp10_ (l->car_agc.gain_db * 0.05);
          }
        /* Advance even on a zero sample, so a pathological all-zero prefix
           cannot hold the loop in "seeding" forever. */
        l->agc_seeded++;
      }

    double pe, lk;
    carrier_nda_disc (agc_step (&l->car_agc, z), l->m, &pe, &lk);
    l->lock += CARRIER_NDA_LOCK_ALPHA * (lk - l->lock);
    (void)lockdet_step (&l->car_lock, l->lock);
    if (!l->tracking)
      mpsk_rx_steer (l, pe);
  }

  /**
   * @brief Feed one post-LO, pre-cascade sample to the free-running arm.
   *
   * The MPSK_RX_NDA_TAP_LO_ARM path, and a no-op for every other tap. Called
   * once per LO step by the owning receiver, ahead of the cascade — which is
   * exactly why this tap needs no symbol timing and reaches the widest
   * frequency range the front end can offer.
   */
  JM_FORCEINLINE JM_HOT void
  mpsk_rx_push_lo (mpsk_rx_loops_t *l, float complex z)
  {
    if (l->nda_tap != MPSK_RX_NDA_TAP_LO_ARM)
      return;
    mpsk_rx_disc (l, boxcar_step (&l->arm, z));
  }

  /**
   * @brief Fold one terminal-stage output into both loops.
   *
   * The receiver's whole per-output body, shared verbatim by the complex- and
   * real-input types. On an on-time strobe it writes the recovered symbol and
   * returns 1.
   *
   * @param l    Loops. Must be non-NULL.
   * @param y    One matched-filtered output from the front end.
   * @param sym  Receives the recovered symbol when the return is 1.
   * @param ted  RATESYNC_TED_GARDNER or RATESYNC_TED_DTTL — pass a literal for
   *             a specialised (branch-free) instantiation.
   * @return 1 if this output was an on-time strobe, 0 otherwise.
   */
  JM_FORCEINLINE JM_HOT int
  mpsk_rx_take_output (mpsk_rx_loops_t *l, float complex y, float complex *sym,
                       int ted)
  {
    /* MPSK_RX_NDA_TAP_MF_ALL discriminates on EVERY terminal output, so it
       runs before the strobe test and needs no timing: it does not care which
       output the timing loop would have nominated. That is the trade — m_out
       times the frequency range and no timing dependence, paid for with the
       ISI those between-symbol outputs carry. */
    if (l->nda_tap == MPSK_RX_NDA_TAP_MF_ALL)
      mpsk_rx_disc (l, y);

    float complex on;
    if (!ratesync_loop_take_output (&l->timing, y, &on, ted))
      return 0;

    /* The carrier discriminator sees the ON-TIME STROBE and nothing else.
       The OTHER terminal outputs sit between symbols, where the matched
       filter is averaging two different ones, so they are not constellation
       points at all; their M-th power is ISI, not carrier phase. Feeding them
       in costs nothing visible at QPSK (the loop averages through it) and is
       fatal at 8PSK, whose decision margin is +-pi/8: measured 0.85 symbol
       error rate, i.e. chance, against 0 on the strobe alone.

       Note also that the discriminator runs in BOTH modes and only the STEER
       is gated. Its lock signal is the handover's input in both directions,
       so freezing it while tracking would make the drop-back unreachable —
       the metric could never fall back through the threshold it rose above.
       (Measured exactly that way: a receiver fed pure noise stayed in
       `tracking` forever.) */

    /* MPSK_RX_NDA_TAP_STROBE: the cleanest input there is, and the one tap
       whose quality depends on symbol timing — see the tap enum. It acts on
       every strobe from the first, locked or not.

       An earlier revision gated the steer, the AGC seed and the handover on
       the timing loop's lock detector, on the grounds that a pre-lock strobe
       is an arbitrary phase of the pulse and its M-th power is nothing in
       particular. The reasoning is sound and the transient is real (measured
       QPSK, sps = 8, Es/N0 20 dB, 5 seeds: the timing loop declares at symbol
       185 on average, range 132-265, and across that window the carrier lock
       statistic swings between -0.947 and +0.888 where a settled lock reads
       +0.906 — a statistic ranging over nearly its whole span, sign included,
       is proof the input is not a constellation). But the gate was
       measured and it does not buy what it was supposed to: across a 24-cell
       sweep it changed exactly one cell, and the reason it looked helpful was
       that it made carrier acquisition easier to MEASURE, not easier to
       achieve — with the steer frozen until timing declares, the carrier's
       transient starts from a known instant.

       A tap that needs timing it cannot wait for is a reason to pick a
       different tap, which is what nda_tap is for: MF_ALL and LO_ARM are
       timing-independent by construction. Gating the default hid that choice
       behind a coupling the caller could not see or override. */
    if (l->nda_tap == MPSK_RX_NDA_TAP_STROBE)
      mpsk_rx_disc (l, on);

    /* The NDA loop's stable points are the 0-grid (z^m = +1), but the QPSK
       constellation sits on the pi/4-offset grid, so a raw strobe would land
       every symbol on a decision boundary. Rotating the SYMBOL rather than the
       matched filter's input is both correct and cheaper: the matched filter's
       taps are real, so a rotation commutes through it, and this way it costs
       one multiply per symbol instead of one per input sample — while the
       discriminator above still sees the unrotated stream it locks. */
    float complex y_rot = on * l->sym_rot;

    if (l->tracking)
      {
        /* Postdetection decision-directed error on the full-SNR symbol; m == 2
           reduces to the BPSK Costas discriminator. */
        float complex ahat;
        mpsk_slice (y_rot, l->m, &ahat);
        double ay = (double)cabsf (y_rot) + MPSK_RX_EPS;
        mpsk_rx_steer (l, (double)cimagf (y_rot * conjf (ahat)) / ay);
      }

    l->sym_count++;
    /* Opt-in two-way handover: after warmup, step the verify-counted detector
       on the carrier lock EMA once per symbol. Its flag IS the discriminator
       choice — and nothing else, now that both run at the symbol rate.
       `warmup_syms` is the whole guard: the pre-lock lock EMA overshoots its
       own ceiling (0.9-1.7 against 0.62), so a warmup shorter than the timing
       loop's own settling can declare on garbage and hand the carrier to a
       decision-directed loop with no valid decisions to make. Size it from the
       timing loop's bandwidth (~5/bn_timing symbols), not from the carrier's. */
    if (l->acq_to_track && l->sym_count >= l->warmup_syms)
      {
        l->tracking = lockdet_step (&l->handover, l->lock);
      }
    *sym = y_rot;
    return 1;
  }

  /** @brief Slice one recovered symbol to its log2(M) hard bits (LSB-first).
   *  @return The bit count written to @p bits. */
  int mpsk_rx_symbol_to_bits (mpsk_rx_loops_t *l, float complex y,
                              uint8_t *bits);

  /* ------------------------------------------------------------------
   * Telemetry
   * ------------------------------------------------------------------ */

  /** @brief Emit the receiver's own probes plus the timing loop's.
   *  Out-of-line on purpose; callers gate on `l->tlm.ctx`. */
  void mpsk_rx_tlm_flush (const mpsk_rx_loops_t *l);

  /** @brief Attach (or detach) telemetry across both loops; see
   *  mpsk_receiver_set_telemetry(), which forwards here. */
  int mpsk_rx_set_telemetry (mpsk_rx_loops_t *l, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim);

/* ── Serializable state — the loops alone (nested by every owner) ──────────
 * Envelope, the carrier/handover/demapper running scalars, then the timing
 * loop's and the carrier loop filter's self-validating sub-blobs. The arm AGC
 * is running state too. Config is restored by the owner's create(). */
#define MPSK_RX_LOOPS_STATE_MAGIC DP_FOURCC ('M', 'R', 'X', 'L')
/* v3: `agc_seeded` became a COUNTER (the AGC seed averages
 * MPSK_RX_AGC_SEED_SAMPS samples instead of taking one), so the flags word
 * carries it in bits 8..15 where v2 held a single "seeded" bit. The blob's
 * SIZE is unchanged — the counter reuses spare bits of an existing u64 — so
 * only the version distinguishes them, and it must, since a v2 blob's bit 2
 * read as a count would resume a seeded receiver mid-seeding and corrupt its
 * gain. Rejected rather than reinterpreted, per the envelope rule. */
#define MPSK_RX_LOOPS_STATE_VERSION 3u

  /** @brief Bytes mpsk_rx_loops_get_state() writes. */
  size_t mpsk_rx_loops_state_bytes (const mpsk_rx_loops_t *l);
  /** @brief Serialize the loops' mutable state into @p blob. */
  void mpsk_rx_loops_get_state (const mpsk_rx_loops_t *l, void *blob);
  /** @brief Restore the loops' mutable state from @p blob.
   *  @return DP_OK, or DP_ERR_INVALID if any envelope rejects. */
  int mpsk_rx_loops_set_state (mpsk_rx_loops_t *l, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_RX_LOOPS_H */
