/**
 * @file carrier_nda_core.h
 * @brief Non-data-aided (NDA) M-th-power carrier-tracking loop.
 *
 * A carrier-recovery loop that locks **without data and without symbol
 * timing** — the cold-start / acquisition counterpart to the decision-directed
 * @ref dp_carrier_mpsk_state_t loop. Per sample it de-rotates the input with the
 * integer-phase @ref dp_lo_state_t NCO (carrier wipe-off); it filters the
 * de-rotated samples through a free-running I/Q **boxcar moving average** of
 * `sps/n` samples (one output per input sample — no rate change), and on every
 * sample runs the
 * **M-th-power** phase discriminator, filters the error through an embedded
 * @ref dp_loop_filter_state_t, and steers the NCO frequency + phase.
 *
 * Raising the arm sample `z` to the Mth power strips the
 * M-PSK data modulation, leaving M times the carrier phase — so the
 * discriminator is
 * **independent of the data symbols and of symbol timing**. That is what lets
 * it acquire a bare/unmodulated carrier, or a modulated carrier before timing
 * lock. It is the M-fold-ambiguous acquisition aid; a decision-directed loop
 * gives the low-jitter steady state (resolve the M-fold ambiguity downstream).
 *
 * The M-th power is computed by **repeated complex squaring**
 * (`z²`→`z⁴`→`z⁸`) of the **unit-magnitude** sample `z/|z|`. Each level yields
 * a phase error and a lock signal:
 *   - `phase_error` = `Im((z/|z|)^M)` scaled by `1, ½, ¼` for M = 2, 4, 8 —
 * the scale normalizes the phase-detector gain so the S-curve slope at lock is
 * 2 for every M (one `bn` behaves identically across M).
 *   - `lock_signal`  = `Re((z/|z|)^M)` — the M-th power of a **limited**
 * sample, so it is bounded in ±1 and its H0 variance is 1/2 for **every** M.
 * ~1 when phase-locked, zero-mean with no carrier. That M-independence is what
 * makes one `lock_thresh` mean one Pfa at every order; the threshold chain is
 * derived above `CARRIER_NDA_LOCK_ALPHA`. Its EMA (`lock`) is the carrier lock
 * metric. See `docs/design/mpsk.md` §4.2, "Limiting — what makes the
 * threshold a Pfa", for the derivation.
 *
 * The block API (dp_carrier_nda_steps) is the Python face and emits the
 * de-rotated sample stream; the JM_FORCEINLINE
 * carrier_nda_wipeoff()/_arm_step()/_steer() are the same loop's three steps
 * as a C composition API, for a caller that runs the loop inside its own
 * sample loop.
 *
 * @note **The input SCALE does not matter, and there is no AGC here.** The
 * discriminator divides out its own amplitude law (`|z|^M`) exactly, so both
 * outputs — and with them the loop gain — are invariant to input scale over
 * the whole float range: measured identical to 6.5e-7 relative from an
 * amplitude of 1e-5 to 1e15, at every M -- a few float eps, which is the
 * rounding floor a float detector has and not a level dependence. The
 * measurement is test_carrier_nda_core.c section 9; this file carried a
 * tighter 5e-7 that the test itself had already corrected, so the number
 * here now comes from the thing that runs it. This loop used to embed a slow arm
 * AGC whose only job was to manufacture `|z| = 1` so a raw `Im(z^M)` would
 * behave; that condition no longer has to be manufactured, and the AGC is
 * gone. A receiver needs exactly one AGC, for its own signal path, and not
 * one per detector (`docs/design/mpsk.md` §3.2, "The NDA discriminator +
 * lock signal").
 *
 * **SCALE is not Es/N0, and only the first of them is invariant here.**
 * Section 9 scales a clean phasor, so it holds signal and noise in the same
 * ratio; that is homogeneity of degree zero, plus — the reason the test
 * earns its place — a float-RANGE gate, since forming `|z|^M` at the end
 * instead returns 0 below `|z| = 0.032` and NaN above 1e4 at M = 8. It says
 * nothing about level relative to NOISE, and it should not be read as
 * though it did: per-sample division by the instantaneous `|s+n|` is a hard
 * limiter, so the S-curve slope genuinely does depend on Es/N0. That
 * dependence is measured, as loop SNR against the un-normalised form across
 * 0–20 dB and every M, in `docs/design/mpsk.md` §3.2 — the penalty is real
 * at 0–3 dB, where the link cannot be closed anyway, and from ~6 dB up
 * normalising is equal or better. This distinction was read the other way
 * once (gh-795), which is why it is spelled out rather than implied.
 *
 * @par The acquisition contract — two bounds, and only one of them is loud
 * A caller must start the residual carrier inside a window, and there are two
 * separate limits on that window. Which one binds decides whether a violation
 * is obvious or silent, so both belong here rather than only in the design of
 * whatever composes this.
 *
 * **Loop capture, roughly `k * bn / M`.** Where pull-in is prompt and
 * predictable: `k` is 1/4 for settling within `2/bn` samples (the validation
 * report's §2.5 seeding rule), and a residual inside `bn/M` settles within
 * `5/bn`. This is the tighter bound at every shipped setting and it is
 * the one a caller normally meets. Violating it is LOUD: the tracked
 * frequency walks and `locked` never asserts, which is unmistakable in
 * telemetry.
 *
 * **The aliasing ceiling, `1/(2M)` cycles per sample.** The discriminator
 * runs once per input sample, so an M-th power folds when the residual
 * advances more than half a cycle per update -- i.e. at a residual of
 * `1/(2M)` cyc/sample, with stable false locks spaced `1/M` apart. The
 * M-fold ambiguity is therefore a FREQUENCY ambiguity as well as a phase
 * one. Violating this is SILENT and it is this object's worst failure mode:
 * the loop sits still at the wrong frequency and reports a healthy lock
 * statistic -- measured on QPSK at a residual of one quarter the update
 * rate, tracked frequency 2e-6 against a true 0.03125 and a lock statistic
 * of +0.83 against the ~1.0 a real lock reads. Nothing self-referenced
 * detects it: the constellation is stationary, so EVM and blind M2M4 both
 * look clean. It takes an external frequency reference, a sync word, or a
 * coarse estimate seeded through `init_norm_freq`.
 *
 * **Both scale as `1/M`,** so an 8PSK caller needs four times tighter tuning
 * than BPSK at the same bandwidth. See `docs/design/mpsk.md` §3.4 and §3.5.
 *
 * @par Update rate is the lever, and a composer owns it
 * The ceiling is linear in the rate the discriminator updates at, and nothing
 * else in the loop depends on that rate -- so it is free margin for whoever
 * picks it. Stepping this object per input sample, which is what
 * dp_carrier_nda_steps does, is the widest setting available and puts the
 * ceiling tens of times beyond loop capture. A composer that instead taps a
 * DECIMATED stream inherits a proportionally tighter ceiling: at two samples
 * per symbol it falls to one symbol rate over M, which is the regime the QPSK
 * false lock above was measured in. The rule for a composition is to choose
 * the tap so the ceiling stays clear of loop capture at every M it supports,
 * which makes the silent failure structurally unreachable and leaves only the
 * loud one.
 *
 * @par What `bn` means
 * `bn` (with `zeta`) is the closed loop's: the discriminator's slope at lock,
 * 2 at every M, is divided out before the loop filter
 * (carrier_nda_loop_error(), #3224), so `bn` is the loop's noise bandwidth
 * and `zeta` its damping whenever the discriminator's gain is its high-SNR
 * value. Measured, test_carrier_nda_core.c section 18: 1.00 to 1.04 of `bn`
 * at every M and denominator. At finite SNR the gain falls by what each
 * denominator gives, and the loop's bandwidth and damping with it:
 *   - the limiter (the default): its gain falls with the SNR at the
 *     discriminator (0.62 at 3.8 dB a look); there `bn` is the design value
 *     of a loop that runs narrower;
 *   - the averaged form: `gamma/(gamma + 1)` at update SNR gamma;
 *   - the unit form: 1, while the caller's noise power holds.
 * Until #3224 the slope went undivided and the loop ran about 1.67 times
 * wider than `bn` said, at a damping near 1.0.
 *
 * @par Options for a composer that updates once a block
 * A composer that runs the loop on its own integrate-and-dump (a despreader's
 * window sum once a code period, say) drives it through the composition API,
 * carrier_nda_wipeoff() once a sample and carrier_nda_update() once a sum,
 * and sets up to three options, each off by default, so dp_carrier_nda_steps()
 * and every existing caller are unchanged:
 *   - **the update period**, dp_carrier_nda_set_update_period(): `bn` becomes
 *     per update and the loop's command is spread over the samples of an
 *     update, as `Costas` does;
 *   - **the denominator**, dp_carrier_nda_set_denominator(): the phase error
 *     over an average of `|z|^2` (the averaged form, gain `gamma/(gamma+1)`),
 *     or over that average less the caller's noise power, clamped (the unit
 *     form, gain 1), in place of the per-update limiter;
 *   - **a block lock**, dp_carrier_nda_set_block_lock(): the ratio of sums
 *     `R = sum(I^2 - Q^2) / sum(I^2 + Q^2)` over a block, against the exact
 *     Beta threshold at a Pfa, in place of the per-sample EMA.
 * The two M-th-power options are derived at M = 2 only and refused at 4 and
 * 8, whose laws are not derived. The derivation and the numbers at the
 * 415.1-B-1 point are docs/design/async-dsss-receiver.md, "The pre-despread
 * carrier loop becomes carrier_nda".
 *
 * @par What `n` costs
 * `n` sets the arm window to `sps / n` samples, and the window is not free:
 * the arm averages across data transitions, so its coherent gain after the
 * M-th power falls as the window widens. For the half-symbol arm (`n = 2`)
 * the gain is `1/2 + 1/(M+1)` -- 5/6 at BPSK, 7/10 at QPSK, 11/18 at 8PSK --
 * against unity for a constant-modulus input. A caller choosing `n` is
 * choosing a sensitivity loss, and the validation report is the evidence for
 * what each choice costs.
 *
 * @code
 * // QPSK NDA carrier loop, 8 samples/symbol, 2-sample moving-average arm
 * dp_carrier_nda_state_t *c = dp_carrier_nda_create(0.01, 0.707, 0.0, 8, 4, 4);
 * float _Complex derot[1024];
 * size_t k = dp_carrier_nda_steps(c, rx, rx_len, derot, 1024);
 * double f = dp_carrier_nda_get_norm_freq(c); // tracked carrier (cyc/sample)
 * dp_carrier_nda_destroy(c);
 * @endcode
 */
#ifndef DP_CARRIER_NDA_CORE_H
#define DP_CARRIER_NDA_CORE_H

#include "doppler/boxcar/boxcar_core.h"
#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/lo/lo_core.h"
#include "doppler/lockdet/lockdet_core.h"
#include "doppler/loop_filter/loop_filter_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include <math.h>
#include "doppler/telemetry/telemetry_core.h"
#include "doppler/detection/detection_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Numerical guard on the arm sample's power |z|^2: at or below it
 *  (or NaN) the discriminator outputs 0 for both phase error and lock
 *  signal (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/** @brief 1/(2*pi): rad -> cycles scale folded into the loop gains so the
 *  loop filter outputs the NCO control in cycles/sample. */
#define CARRIER_NDA_INV_2PI 0.15915494309189535 /* 1 / (2*pi) */
/* ── The lock statistic, and where its threshold comes from ──────────────
 *
 * `lock_signal = Re((z/|z|)^M)` -- the M-th power of a LIMITED sample. Both
 * outputs are limited now (the phase error kept the raw |z|^M weighting until
 * the detector was made to normalise by its own amplitude law), but the two
 * paths wanted it for different reasons: the phase error to keep the loop
 * gain out of the input's hands, the lock signal to be a detector you can put
 * a number on:
 *
 *   - **Bounded.** Each look is Re(e^{j M theta}) in [-1, 1], so the EMA is too.
 *     The raw form is unbounded and, at M = 8, |z|^8 on Gaussian noise gives it
 *     an sd of 137 per look against a value of 1.0 at lock.
 *   - **M-independent.** Under H0 (no carrier) theta is uniform, so
 *     Var[Re(e^{j M theta})] = 1/2 for EVERY M. One threshold is one Pfa at
 *     every constellation order -- which is the property that makes a single
 *     `lock_thresh` mean one thing, and which the unlimited statistic does not
 *     have at any scaling.
 *   - **Detectable.** Measured d' = (mu_H1 - mu_H0)/sd_H0 post-EMA, raw vs
 *     limited, at Es/N0 = 10 / 20 dB: BPSK 5.70/6.21 -> 7.95/8.75,
 *     QPSK 1.50/1.78 -> 5.81/8.47, 8PSK 0.02/0.04 -> 1.76/7.52. The limiter
 *     costs H1 (it discards the |z|^M boost at low SNR) and wins anyway,
 *     everywhere, because it cuts H0's variance by far more than it cuts H1.
 *     With the raw form only BPSK ever clears a 1e-3 Pfa, so no Pfa-derived
 *     threshold existed for M >= 4 at all.
 *
 * The chain, all three numbers derived rather than picked:
 *
 *   alpha  = det_ema_alpha(0.0, 15.9 dB) = 0.05  -> N_eff = (2-a)/a = 39 looks
 *   sd_H0  = sqrt(1/2 * alpha/(2-alpha))  = 0.1132   (analytic; measured 0.1132)
 *   thresh = eta * sd_H0, eta from the Pfa budget
 *
 * The shipped up-threshold of 0.5 is eta = 4.416, i.e. a per-look Pfa of
 * 5e-6 -- so the long-standing default turns out to BE the Pfa-derived value
 * once the statistic is M-independent. It was only ever meaningful at BPSK
 * before: the same 0.5 was eta = 0.9 at QPSK and eta = 0.02 at 8PSK. That
 * threshold lives in carrier_nda_core.c as CARRIER_NDA_LOCK_DEFAULT_UP and is
 * not visible from here, so read it back off a constructed instance rather
 * than assuming this paragraph -- which is what test_carrier_nda_core.c
 * section 15 does when it pins the chain.
 *
 * ── The verify count is NOT sized by that Pfa, and must not be ───────────
 *
 * Everything above sizes a PER-LOOK threshold, and compounding it over n_up
 * consecutive looks assumes those looks are independent. They are not. The
 * EMA above has N_eff = 39, so its output stays correlated for roughly that
 * many samples, and this detector steps once per sample -- a verify count
 * shorter than N_eff is counting one look several times. Measured directly
 * against a noise-only input: n_up = 8, the value the composing receiver uses
 * on this same statistic, false-locked 4 trials in 30; n_up = 64 was the
 * smallest count clean over 300. The shipped default is 64, and 64 > N_eff is
 * the reason it holds rather than a coincidence -- carrier_nda_core.c carries
 * the full trial table. Anyone retuning CARRIER_NDA_LOCK_ALPHA moves N_eff and
 * therefore the floor under n_up. See `docs/design/lock-detect.md` section 3,
 * which owns this failure mode across every detector in the tree.
 */
/** @brief EMA smoothing (per sample) for the lock metric: alpha =
 * det_ema_alpha(0.0, 15.9), giving
 * N_eff = (2-alpha)/alpha = 39 effective looks (>= the 30-look floor). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/** @brief Analytic H0 sd of the limited lock statistic AFTER the EMA above:
 * sqrt(Var_look * alpha/(2-alpha)) with Var_look = 1/2 exactly, for every M.
 * A threshold of `eta * this` has per-look Pfa = Q(eta). */
#define CARRIER_NDA_LOCK_NORM_SD 0.11322770341445956

/** @brief The discriminator's slope at lock, error per radian of carrier
 *  phase, at every M: `sin 2 theta` at M = 2 and the {1, 1/2, 1/4} scale
 *  that makes it 2 at 4 and 8 too. **What has slope 2** is the raw
 *  discriminator output: carrier_nda_disc(), `last_error`, the `.e` probe
 *  and dp_carrier_nda_get_last_error(). **What the filter is handed** has
 *  slope 1: a loop filter's gains assume the rest of its loop has unit
 *  gain (dp_loop_filter_init()), so the error reaches the filter divided by
 *  this, through carrier_nda_loop_error(), and `bn` and `zeta` are then the
 *  closed loop's (#3224). */
#define CARRIER_NDA_DISC_SLOPE 2.0

/** @brief The discriminator's error as a unit-slope loop takes it: divided
 *  by CARRIER_NDA_DISC_SLOPE. The one place every loop on this
 *  discriminator scales it -- carrier_nda_steer() and MpskReceiver's
 *  mpsk_rx_steer() -- so `bn` means the closed loop's noise bandwidth at
 *  high SNR for every M (#3224). Dividing by 2 is exact.
 *  @param pe  The discriminator's output.
 *  @return The error in radians of carrier phase, slope 1 at lock. */
JM_FORCEINLINE double
carrier_nda_loop_error (double pe)
{
  return pe / CARRIER_NDA_DISC_SLOPE;
}

/** @brief The denominator in force (the state's `den`, which
 *  dp_carrier_nda_set_denominator() derives from its arguments): the
 *  update's own `|z|^2`, a per-update limiter (the default, and the one
 *  carrier_nda_disc() computes). Gain `E[1/(1 + n/A)]`, falling with SNR. */
#define CARRIER_NDA_DEN_LIMITER 0
/** @brief An average of `|z|^2` (an EMA, booted as a cumulative mean). Its
 *  mean is signal plus noise, so the gain is `gamma/(gamma + 1)` at update
 *  SNR gamma. M = 2 only. */
#define CARRIER_NDA_DEN_AVERAGED 1
/** @brief That average less the caller's noise power, clamped between
 *  avg * gamma_d/(gamma_d + 1) and avg, with gamma_d the design update SNR:
 *  gain 1 where the estimate holds, never above the design point's, never
 *  the averaged form's sign. M = 2 only. */
#define CARRIER_NDA_DEN_UNIT 2
/** @brief The largest block a block lock takes, updates. Its threshold is
 *  dp_det_threshold_f()'s F(len, len) quantile, a bisection on a continued
 *  fraction capped at 300 terms. Against scipy's `f.isf`, the threshold r it
 *  gives holds to 1e-9 relative at 1e7 and fails outright between 2e8 and
 *  5e8 (r even turns negative at 2^31). A block of 1e7 code periods is
 *  about 50 minutes at the 415.1-B-1 point. */
#define CARRIER_NDA_BLOCK_MAX_LEN 10000000u

  /**
   * @brief Telemetry attachment: a borrowed context + this object's probe
   *        ids.  NULL ctx (the default) means detached — the probe site is
   *        then a single predicted-not-taken branch per block loop.  Zeroed
   *        in state blobs and preserved across set_state
   *        (DP_DEFINE_POD_STATE_TLM).
   */
  typedef struct
  {
    dp_tlm_t *ctx;       /**< NULL = detached                          */
    int32_t   id_lock;   /**< "<prefix>.lock" — lock-signal EMA        */
    int32_t   id_e;      /**< "<prefix>.e"    — M-th-power phase error */
    int32_t   id_freq;   /**< "<prefix>.freq" — tracked carrier freq   */
    int32_t   id_locked; /**< "<prefix>.locked" — lockdet flag 0/1     */
    int32_t   id_nco;    /**< "<prefix>.nco" — the full NCO command    */
  } carrier_nda_tlm_t;

  /**
   * @brief NDA M-th-power carrier loop state.
   *
   * Allocate with dp_carrier_nda_create(), or embed by value and
   * dp_carrier_nda_init(). The carrier NCO (`nco`) and PI loop (`lf`) are public
   * sub-components so a composing receiver can drive the same NCO; treat the
   * arm accumulator and the diagnostics as internal.
   */
  typedef struct
  {
    dp_lo_state_t          nco; /**< integer carrier NCO (uint32 phase).      */
    dp_loop_filter_state_t lf;  /**< 2nd-order carrier PI loop.               */
    size_t              sps; /**< samples per symbol.                      */
    int                 m;   /**< constellation order M (2, 4, 8).         */
    int                 n;   /**< sets the MA window (= a 1/n-symbol box).  */
    size_t arm_len;          /**< moving-average window length (= sps / n). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    dp_boxcar_state_t arm;      /**< I/Q boxcar moving-average arm (sps/n).    */
    double         lock;     /**< EMA of the lock signal (1 = locked).     */
    double         last_error; /**< last phase discriminator (loop stress).  */
    double          ctl_cyc; /**< NCO control (cyc/sample) for next wipeoff.*/
    dp_lockdet_state_t lockdet; /**< decision rule: thresholds + verify
                                  counters stepped on `lock` each sample
                                  (mirrors MpskReceiver's own pre-existing
                                  handover step on this same statistic).   */
    /* The composer's options (all off by default; see "Options for a
       composer" at the top). */
    double t_upd;     /**< samples a loop update (1 = every sample).      */
    double den_alpha; /**< the averaged power's EMA weight a update.      */
    double den_gamma; /**< the unit form's design update SNR, linear.     */
    double pow_avg;   /**< running average of |z|^2 (averaged, unit).     */
    double pow_n;     /**< updates averaged so far: the boot's mean.      */
    int32_t  den;     /**< CARRIER_NDA_DEN_*: what divides the error.     */
    uint32_t blk_len; /**< block lock: updates a block (0 = the EMA).     */
    uint32_t blk_n;   /**< updates in the current block.                  */
    uint32_t opt_zero; /**< always 0: the four 32-bit fields fill two
                            words, so a whole-struct blob has no padding
                            between them; the state check holds it 0.   */
    double   blk_thr; /**< its threshold on R (exact Beta, at blk_pfa).   */
    double   blk_pfa; /**< the false-alarm rate the threshold is for.    */
    double   blk_num; /**< sum of I^2 - Q^2 in the current block.        */
    double   blk_den; /**< sum of I^2 + Q^2 in the current block.         */
    carrier_nda_tlm_t tlm;   /**< live telemetry attachment; zeroed in blobs */
  } dp_carrier_nda_state_t;


  /**
   * @brief The M-th-power discriminator on an arm sample, normalized by its
   * own amplitude law.
   *
   * Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` on the **unit-
   * magnitude** sample `z/|z|` and writes the phase error (= scaled
   * `Im((z/|z|)^M)`) and the lock signal (`Re((z/|z|)^M)`). Both outputs are
   * therefore invariant to the input's scale, which is what lets this loop
   * run with no AGC in front of it — see the amplitude note at the top of
   * this file, and docs/design/mpsk.md §3.2, "The NDA discriminator + lock
   * signal", for the squaring-loss measurement that says normalizing is
   * equal or better from ~6 dB Es/N0 up: loop SNR against the raw form,
   * 4e5 samples per point, tabulated over 0–20 dB and M = 2/4/8.
   *
   * @param z      Arm moving-average sample (any scale; only its phase is
   *               used).
   * @param m      Constellation order (2, 4, 8).
   * @param pe     Receives the phase error.
   * @param lock   Receives the lock signal.
   */
#ifndef __cplusplus
  /* C only: the body uses the C99 complex library, which C++ does not have
     (dp_complex.h, "From C++"). A C++ caller uses dp_carrier_nda_steps(). */
  JM_FORCEINLINE void
  carrier_nda_disc (float _Complex z, int m, double *pe, double *lock)
  {
    /* BOTH outputs normalise by the detector's OWN amplitude law, |z|^M.
     *
     * A discriminator's raw output is the phase error multiplied by things it
     * did not choose, and amplitude is the largest of them: Im(z^M) scales as
     * A^M, so a 2x level error is 4x loop gain at BPSK and 256x at 8PSK. Only
     * the detector can divide that out, and it can do it exactly -- |z|^M is a
     * power of p for every M supported here, so it costs one divide and no
     * sqrt. This is the same rule the timing detector follows (a TED
     * normalises by its own slope, dp_symsync_ted_slope()), applied to its
     * sibling.
     *
     * At |z| = 1 this is identical to the un-normalised form, so the S-curve
     * slope -- and with it the meaning of bn -- is unchanged from when an
     * upstream AGC was manufacturing that condition. What changes is that it
     * no longer HAS to be manufactured: an AGC ahead of this detector existed
     * only to make |z| = 1 true, and a receiver now needs exactly one AGC,
     * for its own signal path, not one per detector.
     *
     * DIVIDE ONCE, AT THE FIRST SQUARING -- not once per M at the end. The
     * pair (Re(z^2), Im(z^2))/p IS the unit vector (z/|z|)^2, and every later
     * squaring of a unit vector is a unit vector, so after that one divide
     * nothing in this function ever exceeds 1 in magnitude. Dividing at the
     * end instead means forming |z|^M explicitly, and BOTH ends of that
     * overflow the float range the AGC used to keep us away from: measured on
     * the |z|^8 form, M = 8 returned exactly 0 below |z| = 0.032 (the eps
     * guard, applied to |z|^8, trips at 1e-12^(1/8)) and NaN at |z| = 1e5
     * (|z|^8 = 1e40 > FLT_MAX, then inf/inf) -- and a NaN here poisons the
     * loop filter and the NCO permanently. With the divide hoisted, the guard
     * is on p alone and means the same thing at every M, and the outputs are
     * scale-invariant from 1e-5 to 1e15 (max 6.5e-7 relative, at M = 8 and
     * the top of that range). At |z| = 1 the
     * two forms agree to 1.8e-7 over a full phase sweep, so the S-curve slope
     * -- and with it the meaning of bn -- is unchanged either way.
     *
     * The cascade runs in float: the unit-magnitude intermediates are all
     * O(1) and float's ~1e-7 relative error is far below what the loop
     * tolerates. Keeping it in float avoids the float->double conversions on
     * this loop-carried critical path; only the two outputs (which feed the
     * double loop filter) promote. */
    float i = crealf (z);    /* raw I, any scale */
    float q = cimagf (z);    /* raw Q            */
    float p = i * i + q * q; /* |z|^2            */
    /* Written !(p > eps) so a NaN input yields zero rather than a NaN error
       fed to the loop filter. */
    if (!(p > CARRIER_NDA_EPS))
      {
        *pe = *lock = 0.0;
        return;
      }
    float rp = 1.0f / p;
    float bl = (i * i - q * q) * rp; /* Re((z/|z|)^2) */
    float be = (2.0f * i * q) * rp;  /* Im((z/|z|)^2) */
    if (m == 2)
      {
        *pe   = be; /* Im((z/|z|)^2) */
        *lock = bl; /* Re((z/|z|)^2) */
        return;
      }
    float ql = bl * bl - be * be; /* Re((z/|z|)^4)     */
    float qe = be * bl;           /* Im((z/|z|)^4) / 2 */
    if (m == 4)
      {
        *pe   = qe;
        *lock = ql;
        return;
      }
    /* Im((z/|z|)^8) / 4. */
    float pe8 = qe * ql;
    /* Re(u^8) = Re(u^4)^2 - Im(u^4)^2, and `qe` is HALF of Im(u^4) -- that
     * half being the deliberate {1, 1/2, 1/4} phase-error scaling which
     * equalises the S-curve slope across M. So reconstructing Re(u^8) from it
     * needs the 2 squared back: ql*ql - (2*qe)^2. Without the 4 the statistic
     * is Re(u^4)^2 - Im(u^4)^2/4, which is NOT Re(u^8) and, unlike it, is not
     * zero-mean on noise -- E[Re(u^4)^2] = E[Im(u^4)^2] for circular noise, so
     * the shortfall leaves a positive residual of (3/4)E[Im(u^4)^2]. Measured
     * on unit-power complex Gaussian noise, 4e5 samples: mean +8.94 without
     * the 4, -0.11 with it (and bit-identical to Re(z^8) computed directly).
     * The value AT LOCK is +1.0000 either way, which is why this hid: it
     * corrupted only the noise-only tail, i.e. exactly the false-alarm
     * behaviour a lock detector is thresholded on. */
    *pe   = pe8;                      /* Im((z/|z|)^8) / 4 */
    *lock = ql * ql - 4.0f * qe * qe; /* Re((z/|z|)^8)     */
  }
#endif /* !__cplusplus */

  /**
   * @brief Initialise an NDA carrier loop in place (no allocation).
   *
   * @param s               State to initialise.  Must be non-NULL.
   * @param bn              Loop noise bandwidth, cycles/sample (per-sample
   * loop).
   * @param zeta            Damping factor (1.0 = critical; 0.707 usual).
   * @param init_norm_freq  Seed carrier frequency, cycles/sample.
   * @param sps             Samples per symbol.
   * @param n               MA window divisor: window = sps/n samples (sps % n
   * == 0, sps/n <= BOXCAR_MAX_LEN).
   * @param m               Constellation order M (2, 4, 8).
   *
   * Unguarded, as the by-value path: the caller checks (bn, zeta) with
   * dp_loop_filter_params_ok() first. Outside it the loop filter is not
   * written at all -- config_loop refuses and changes nothing -- so an
   * in-place init leaves the gains and bn/zeta unset, since seed() zeroes
   * integ (doppler#2103).
   */
  void dp_carrier_nda_init (dp_carrier_nda_state_t *s, double bn, double zeta,
                         double init_norm_freq, size_t sps, int n, int m);

  /**
   * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
   * @param s  Carrier loop state.  Must be non-NULL.
   * @param x  One input sample.
   * @return The de-rotated sample to feed the moving-average arm.
   */
#ifndef __cplusplus
  /* C only: the body uses the C99 complex library, which C++ does not have
     (dp_complex.h, "From C++"). A C++ caller uses dp_carrier_nda_steps(). */
  JM_FORCEINLINE JM_HOT float _Complex
  carrier_nda_wipeoff (dp_carrier_nda_state_t *s, float _Complex x)
  {
    /* De-rotate through the NCO's control port: the LO advances by its centre
     * frequency (phase_inc) plus the loop's last control (ctl_cyc, set by
     * carrier_nda_steer). The LO owns the phase accumulation and scaling. */
    return x * conjf (lo_step_ctrl (&s->nco, s->ctl_cyc));
  }
#endif /* !__cplusplus */

  /**
   * @brief Slide the moving-average arm by one sample; discriminate the
   * output.
   *
   * The arm is a free-running boxcar **moving average** of the last `arm_len`
   * de-rotated samples — one output per input sample, **no rate change** (not
   * a decimating integrate-and-dump). It updates the running window sum in
   * O(1) (add @p d, subtract the sample leaving the window), runs the
   * M-th-power discriminator on the window average, writes @p pe and @p lock,
   * and returns 1 every call.
   *
   * @param s     Carrier loop state.  Must be non-NULL.
   * @param d     One de-rotated sample (from carrier_nda_wipeoff).
   * @param pe    Receives the phase error.
   * @param lock  Receives the lock signal.
   * @return Always 1 (one discriminator output per input sample).
   */
#ifndef __cplusplus
  /* C only: the body uses the C99 complex library, which C++ does not have
     (dp_complex.h, "From C++"). A C++ caller uses dp_carrier_nda_steps(). */
  JM_FORCEINLINE JM_HOT int
  carrier_nda_arm_step (dp_carrier_nda_state_t *s, float _Complex d, double *pe,
                        double *lock)
  {
    /* Slide the boxcar moving average by one sample (unit gain — pure I/Q
     * average) and discriminate it directly. There is no AGC on this path and
     * none is wanted: carrier_nda_disc normalises by its own amplitude law,
     * so the loop gain is already amplitude-invariant and a second level loop
     * in series would only add its own transient to correct. */
    carrier_nda_disc (dp_boxcar_step (&s->arm, d), s->m, pe, lock);
    return 1;
  }
#endif /* !__cplusplus */

  /**
   * @brief Steer the NCO with a phase error through the loop filter.
   *
   * Filters @p pe and stores the PI filter's output (`integ + kp*pe`, in
   * cycles an update) spread over the update's `t_upd` samples, cycles a
   * sample, as the frequency control the next carrier_nda_wipeoff() feeds
   * the LO. At the default `t_upd` of 1 the division is exact and the
   * control is the filter's output.
   *
   * @param s   Carrier loop state.  Must be non-NULL.
   * @param pe  Phase error from the NDA discriminator (carrier_nda_arm_step).
   */
  JM_FORCEINLINE JM_HOT void
  carrier_nda_steer (dp_carrier_nda_state_t *s, double pe)
  {
    s->last_error = pe;
    /* The PI loop filter output (integ + kp*pe) is the NCO frequency command.
     * config_loop folds the rad->cycle constant (1/2*pi) into kp/ki, so the
     * output is already in cycles/sample — store it directly as the control
     * the next wipeoff feeds to the LO's control port (no per-sample
     * conversion). The LO does the cycles->phase scaling and phase
     * accumulation, so the loop never touches the integer phase. The loop
     * filter is init'd with t = 1 (the MA arm updates every sample), so bn is
     * cycles/sample and n-invariant — n only sets the window length. lf.integ
     * is thus the carrier frequency correction in cycles/sample (read back by
     * dp_carrier_nda_get_norm_freq). */
    s->ctl_cyc
        = dp_loop_filter_step (&s->lf, carrier_nda_loop_error (pe)) / s->t_upd;
  }

  /**
   * @brief One loop update on a caller's integrate-and-dump @p z: the
   * discriminator with the configured denominator, the configured lock
   * statistic, and the steer.
   *
   * The composition API's update, for a caller that forms its own arm (a
   * despread window sum, once a code period) and calls
   * carrier_nda_wipeoff() once a sample in between. With every option at its
   * default it is exactly dp_carrier_nda_steps()'s per-sample update:
   * carrier_nda_disc() on @p z, the lock EMA and its decision, the steer.
   *
   * - **The error** is `Im(z^M)` over the configured denominator: `|z|^M`
   *   (the limiter, carrier_nda_disc()), the running average of `|z|^2`
   *   (averaged, M = 2), or that average less @p noise_pw, clamped
   *   (unit, M = 2). The running average takes @p z before it divides, and
   *   boots as a cumulative mean: the weight is the larger of 1/n and
   *   `den_alpha`.
   * - **The lock** is the EMA of `Re((z/|z|)^M)` and its decision, or, with
   *   a block lock set, the ratio of sums over the block, decided once a
   *   block against its threshold.
   *
   * @param s         Carrier loop state.  Must be non-NULL.
   * @param z         The update's arm sample (any scale).
   * @param noise_pw  The noise power in @p z's own units (`E|n|^2` of one
   *                  update), read by the unit denominator only; pass 0
   *                  otherwise.
   */
#ifndef __cplusplus
  JM_FORCEINLINE JM_HOT void
  carrier_nda_update (dp_carrier_nda_state_t *s, float _Complex z,
                      double noise_pw)
  {
    double pe, lk;
    carrier_nda_disc (z, s->m, &pe, &lk);
    if (s->den != CARRIER_NDA_DEN_LIMITER)
      {
        /* M = 2 only (set_denominator refuses the rest): Im(z^2) = 2IQ over
           the average power, or over the average less the noise, clamped
           between the design point's signal share and the average itself.
           Formed in double: the products are of the caller's scale. */
        const double i = (double)crealf (z), q = (double)cimagf (z);
        const double p = i * i + q * q;
        s->pow_n += 1.0;
        const double w = 1.0 / s->pow_n > s->den_alpha ? 1.0 / s->pow_n
                                                       : s->den_alpha;
        s->pow_avg += w * (p - s->pow_avg);
        double a2 = s->pow_avg;
        if (s->den == CARRIER_NDA_DEN_UNIT)
          {
            const double lo = s->pow_avg * s->den_gamma / (s->den_gamma + 1.0);
            a2              = s->pow_avg - noise_pw;
            a2              = a2 < lo ? lo : a2 > s->pow_avg ? s->pow_avg : a2;
          }
        pe = a2 > 0.0 ? 2.0 * i * q / a2 : 0.0;
      }
    if (s->blk_len)
      {
        /* The ratio of sums over the block, R = sum(I^2 - Q^2) / sum(I^2 +
           Q^2), M = 2: Beta(B/2, B/2) about 0 on noise, so its threshold is
           exact (set_block_lock). */
        const double i = (double)crealf (z), q = (double)cimagf (z);
        s->blk_num += i * i - q * q;
        s->blk_den += i * i + q * q;
        if (++s->blk_n >= s->blk_len)
          {
            s->lock = s->blk_den > 0.0 ? s->blk_num / s->blk_den : 0.0;
            (void)dp_lockdet_step (&s->lockdet, s->lock);
            s->blk_n   = 0;
            s->blk_num = s->blk_den = 0.0;
          }
      }
    else
      {
        s->lock += CARRIER_NDA_LOCK_ALPHA * (lk - s->lock);
        (void)dp_lockdet_step (&s->lockdet, s->lock);
      }
    carrier_nda_steer (s, pe);
  }
#endif /* !__cplusplus */

  /**
   * @brief Create an NDA carrier loop instance.
   *
   * @param bn              Loop noise bandwidth (default 0.01).
   * @param zeta            Damping factor (default 0.707).
   * @param init_norm_freq  Seed carrier frequency, cycles/sample (default
   * 0.0).
   * @param sps             Samples per symbol (default 8).
   * @param n               MA window divisor: window = sps/n (default 4;
   * sps%n==0).
   * @param m               Constellation order M, 2/4/8 (default 4 = QPSK).
   * @return Heap-allocated state, or NULL on invalid args / allocation
   * failure.
   * @note Caller must call dp_carrier_nda_destroy() when done.
   */
  dp_carrier_nda_state_t *dp_carrier_nda_create (double bn, double zeta,
                                           double init_norm_freq, size_t sps,
                                           int n, int m);

  /**
   * @brief Destroy an NDA carrier loop instance and release all memory.
   * @param state  May be NULL.
   */
  void dp_carrier_nda_destroy (dp_carrier_nda_state_t *state);

  /**
   * @brief Re-seed the loop to its create-time frequency/phase; keep config.
   *
   * Restores the object to its post-create state: the carrier NCO is reset to
   * the seed frequency it was constructed with (init_norm_freq) with zero
   * phase, the moving-average arm, the loop-filter integrator and the lock
   * EMA are cleared, and the lock detector is dropped. The configured (bn, zeta), the
   * arm geometry (sps, n) and the constellation order m are preserved, so the
   * same object can re-acquire a fresh capture.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
   * ...                sps=8, n=4, m=4)
   * >>> rng = np.random.default_rng(0)
   * >>> k = np.arange(40000)
   * >>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
   * ...      rng.standard_normal(k.size)
   * ...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
   * >>> _ = c.steps(x)
   * >>> round(c.norm_freq, 4), round(c.lock, 2)   # acquired the carrier
   * (0.001, 0.99)
   * >>> c.reset()
   * >>> round(c.norm_freq, 4), round(c.lock, 2)   # back to seed, unlocked
   * (0.0, 0.0)
   *
   * @endcode
   */
  void dp_carrier_nda_reset (dp_carrier_nda_state_t *state);

  /**
   * @brief Emit the carrier loop's telemetry records for the current sample.
   *
   * Out-of-line on purpose: the emit machinery must not inline into the
   * per-sample hot loop (inlined ring-write expansions bloat the loop body
   * and an extern call site forces per-iteration state reloads — both
   * measured ~20% slower detached on other loops). Callers gate on
   * `s->tlm.ctx`. This loop updates every sample, so the natural call rate
   * is per sample — decim (set at attach) is the throttle. Records
   * "<prefix>.lock" (the lock-signal EMA), "<prefix>.e" (the M-th-power
   * phase discriminator — the loop stress), "<prefix>.freq" (the tracked
   * carrier, NCO centre + integrated correction, cycles/sample) and
   * "<prefix>.locked" (the verify-counted lockdet decision, 0/1) and
   * "<prefix>.nco" (the full NCO command, centre plus `integ + kp*e` spread
   * over the update, cycles/sample: dp_carrier_nda_get_nco_freq()). A
   * composer that updates once a block calls this once an update; summed over
   * the update's samples, `.nco` is the phase the LO advanced by.
   *
   * @param s  State with a non-NULL tlm.ctx (caller-checked).
   */
  void dp_carrier_nda_tlm_flush (const dp_carrier_nda_state_t *s);

  /**
   * @brief Attach (or detach) a telemetry context and register the carrier
   * loop's probes on it.
   * Registers five probes, emitted once per input sample (this is a
   * sample-rate loop — use @p decim to thin the stream): "<prefix>.lock"
   * (the lock statistic: the EMA, ~1 when phase-locked, or the last block's
   * R), "<prefix>.e" (the M-th-power phase discriminator — the loop
   * stress), "<prefix>.freq" (the tracked carrier frequency,
   * cycles/sample), "<prefix>.locked" (the verify-counted lockdet decision,
   * 0/1) and "<prefix>.nco" (the full NCO command, cycles/sample).  Passing
   * NULL detaches.
   * Setup path, never hot: call before the producer thread starts
   * stepping; the context is borrowed and must outlive the attachment
   * (SPSC rules in dp_tlm/dp_tlm_core.h).
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "car" or "rx.car".
   * @param decim  Emit every decim-th sample; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         five probes (the attach fails whole; everything stays
   *         detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)
   * >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
   * >>> c.set_telemetry(tlm, "car", decim=8)
   * >>> sorted(tlm.probe_names)
   * ['car.e', 'car.freq', 'car.lock', 'car.locked', 'car.nco']
   * >>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(
   * ...     np.complex64)
   * >>> _ = c.steps(x)
   * >>> recs = tlm.read()
   * >>> len(recs[recs["probe"] == tlm.probe_id("car.e")]) == 4096 // 8
   * True
   *
   * @endcode
   */
  int dp_carrier_nda_set_telemetry (dp_carrier_nda_state_t *state, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

  /**
   * @brief Re-tune the carrier lock detector's geometry directly.
   *
   * Full lockdet control, mirroring dp_costas_configure_lock(): a split
   * declare/drop threshold pair on the lock-signal EMA (level hysteresis)
   * and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up /
   * 32 down) start from MpskReceiver's own carrier lock-indicator
   * thresholds, but size n_up independently: `lock` is
   * a fast per-sample EMA, so consecutive looks are highly autocorrelated
   * and MpskReceiver's own n_up=8 does not compound the false-declare
   * rate the way it would for independent looks (direct Monte Carlo
   * against a noise-only, no-carrier input found real false locks at
   * n_up=8; n_up=64 was the smallest verify count that reliably
   * eliminated them -- see carrier_nda_core.c's CARRIER_NDA_LOCK_DEFAULT_*
   * comment for the exact trial data). A live lock survives the re-tune;
   * the in-flight verify run restarts. It re-tunes the EMA's rule: with a
   * block lock set (dp_carrier_nda_set_block_lock()), it returns to the EMA
   * first, so block_thresh and the rule in force never disagree.
   *
   * @param state        Must be non-NULL.
   * @param up_thresh    Declare threshold on the lock-signal EMA.
   * @param down_thresh  Drop threshold; choose <= up_thresh for level
   *                     hysteresis.
   * @param n_up         Consecutive above-threshold samples to declare;
   *                     clamped >= 1.
   * @param n_down       Consecutive below-threshold samples to drop;
   *                     clamped >= 1.
   * @code
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
   * >>> c.locked
   * False
   * >>> c.configure_lock(0.6, 0.5, 16, 64)   # tighter declare, slower drop
   *
   * @endcode
   */
  void dp_carrier_nda_configure_lock (dp_carrier_nda_state_t *state,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  /** @brief Current lock decision (1 = locked, 0 = not), with the
   *         configured verify-count / hysteresis rule applied (see
   *         dp_carrier_nda_configure_lock).
   *  @param state  Must be non-NULL.
   *  @return 1 when locked, 0 otherwise (0 after create/reset). */
  int dp_carrier_nda_get_locked (const dp_carrier_nda_state_t *state);

  /**
   * @brief Set the loop's update period: @p t_upd samples a loop update.
   *
   * For a composer that updates the loop once a block (carrier_nda_update()
   * once a sum, carrier_nda_wipeoff() once a sample). The loop is designed
   * per update: `bn` becomes the noise bandwidth times the update period
   * (`B_L * t_upd / fs`), and the filter's output, a frequency in cycles an
   * update, is spread over the update's samples (carrier_nda_steer()). The
   * frequency readouts follow. The configured `bn` and `zeta` are kept. On a
   * running loop the integrator, which holds cycles an update, is rescaled by
   * the new period over the old, so the tracked frequency (cycles a sample)
   * and the command held for the next sample are unchanged by the call; the
   * NCO and the lock are not touched.
   *
   * dp_carrier_nda_steps() runs the loop once a sample, so it is defined at
   * the default 1 only: at any other period it writes nothing and returns 0.
   *
   * @param state  Must be non-NULL.
   * @param t_upd  Samples an update, finite and >= 1 (it may be fractional:
   *               a code period of 2,750.4 samples).
   * @return DP_OK, or DP_ERR_INVALID (nothing changes) for a period that is
   *         not finite or under 1.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.012, sps=8, n=1, m=2)
   * >>> c.set_update_period(3500.0)   # once a code period of 3,500 samples
   * >>> len(c.steps(np.ones(8, dtype=np.complex64)))   # steps() is per sample
   * 0
   *
   * @endcode
   */
  int dp_carrier_nda_set_update_period (dp_carrier_nda_state_t *state,
                                        double                  t_upd);

  /**
   * @brief Choose what divides the phase error: the update's power averaged
   * with weight @p alpha, less the caller's noise power when @p gamma_d is
   * set.
   *
   * One parametrization covers the three forms:
   *   - `alpha = 1, gamma_d = 0`: one update's own `|z|^2`, the limiter (the
   *     default; carrier_nda_disc()'s arithmetic, exactly);
   *   - `alpha < 1, gamma_d = 0`: the average, the averaged form;
   *   - `gamma_d > 0`: the average less the noise, clamped between the
   *     design point's signal share, `avg * gamma_d/(gamma_d + 1)`, and the
   *     average itself, the unit form.
   * At update SNR `gamma` the limiter's gain falls with the SNR, the averaged
   * form's is `gamma/(gamma + 1)`, and the unit form's is 1 while the
   * caller's noise power holds (carrier_nda_update()). The clamp keeps the
   * unit form's gain at most the design point's 1, and its sign the
   * average's, at boot or when the noise estimate is off.
   *
   * Every form but the limiter is derived at M = 2 only and refused at M = 4
   * and 8. Setting a form clears the running average; the loop's other state
   * is untouched.
   *
   * @param state    Must be non-NULL.
   * @param alpha    The average's EMA weight an update, in (0, 1]: 1/256 at
   *                 the 415.1-B-1 point, 1 for the limiter.
   * @param gamma_d  The design update SNR, linear: > 0 for the unit form,
   *                 0 otherwise.
   * @return DP_OK, or DP_ERR_INVALID (nothing changes) for an @p alpha
   *         outside (0, 1], a @p gamma_d negative or not finite, or a form
   *         other than the limiter at an M other than 2.
   * @code
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.01, sps=8, n=1, m=2)
   * >>> c.set_denominator(1.0 / 256.0, 2.4)   # the unit form, gamma_d 3.8 dB
   * >>> c.set_denominator(1.0, 0.0)           # back to the limiter
   *
   * @endcode
   */
  int dp_carrier_nda_set_denominator (dp_carrier_nda_state_t *state,
                                      double alpha, double gamma_d);

  /**
   * @brief Decide the lock on a block: the ratio of sums over @p len
   * updates, against the exact threshold at @p pfa.
   *
   * `R = sum(I^2 - Q^2) / sum(I^2 + Q^2)` over a block of updates, dumped
   * once a block. On noise alone `(R + 1)/2` is exactly `Beta(len/2,
   * len/2)`: the block's own total power is the noise reference, so the
   * threshold needs no noise estimate. It is that law's upper @p pfa point,
   * through dp_det_threshold_f() (`R > r` is `X/Y > (1 + r)/(1 - r)` for
   * `X, Y` chi-square on @p len degrees). The decision takes @p n_up blocks
   * above it to declare and @p n_down below it to drop (one threshold, time
   * hysteresis only). `lock` then reads the last block's R.
   *
   * @p len 0 returns to the per-sample EMA and its default rule
   * (dp_carrier_nda_configure_lock()'s). M = 2 only. Either way the lock
   * decision restarts unlocked, and the block in progress is cleared.
   *
   * @param state   Must be non-NULL.
   * @param len     Updates a block, 2 to CARRIER_NDA_BLOCK_MAX_LEN; or 0 for
   *                the EMA.
   * @param pfa     The false-alarm rate a block, in (0, 1).
   * @param n_up    Blocks above the threshold to declare; clamped >= 1.
   * @param n_down  Blocks below it to drop; clamped >= 1.
   * @return DP_OK, or DP_ERR_INVALID (nothing changes) for an M other than
   *         2, a @p len of 1 or past CARRIER_NDA_BLOCK_MAX_LEN, or a @p pfa
   *         out of range.
   * @code
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.012, sps=8, n=1, m=2)
   * >>> c.set_block_lock(31, 1e-3, 2, 3)
   * >>> round(c.block_thresh, 3)   # the Beta(15.5, 15.5) upper 1e-3 point
   * 0.518
   *
   * @endcode
   */
  int dp_carrier_nda_set_block_lock (dp_carrier_nda_state_t *state,
                                     uint32_t len, double pfa, uint32_t n_up,
                                     uint32_t n_down);

  /** @brief The block lock's threshold on R (0 when the EMA decides).
   *  @param state  Must be non-NULL.
   *  @return The threshold set_block_lock() derived, or 0. */
  double dp_carrier_nda_get_block_thresh (const dp_carrier_nda_state_t *state);

  /** @brief The lock decision's rule (thresholds and verify counts).
   *  @param state  Must be non-NULL.
   *  @return The rule the lock detector holds. */
  dp_lockdet_rule_t
  dp_carrier_nda_get_lock_rule (const dp_carrier_nda_state_t *state);

  /**
   * @brief Hold the loop: the NCO runs on the integrator alone until the
   *        next update.
   *
   * carrier_nda_steer() commands `(integ + kp*e) / t_upd` cycles a sample,
   * and carrier_nda_wipeoff() applies that command every sample until the
   * next update: the proportional term `kp*e` is spread over the update's
   * `t_upd` samples, a phase step of `kp*e` in all. A composer that stops
   * updating (a coast, or a period whose update it skips) would otherwise go
   * on applying that step every update interval, a frequency bias of `kp*e
   * / t_upd` for as long as it holds. This sets the command to `integ /
   * t_upd`, the integrator's frequency, as a coast should run. The
   * integrator, the lock and the NCO's phase are unchanged; the next update
   * steers as usual.
   *
   * @param state  Must be non-NULL.
   */
  void dp_carrier_nda_hold (dp_carrier_nda_state_t *state);

  /**
   * @brief The carrier phase the NCO will apply @p ahead samples from now,
   *        with no loop update in between.
   *
   * carrier_nda_wipeoff() advances the NCO by its centre increment plus
   * the loop's control, both fixed between updates, so @p ahead samples on
   * the phase word is that sum @p ahead times, in the NCO's own 32-bit
   * arithmetic: exact for every sample before the next update. A
   * composition holding samples not yet wiped asks this for the phase at
   * the newest one.
   *
   * @param state  Must be non-NULL.
   * @param ahead  Samples from the next one to be wiped (0 = that one).
   * @return Phase in cycles, in [0, 1).
   */
  double dp_carrier_nda_get_nco_phase_ahead (const dp_carrier_nda_state_t *state,
                                             size_t ahead);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop
 * exactly.
 */
/** @brief State-blob magic for CarrierNda (FourCC "CNDA"). */
#define CARRIER_NDA_STATE_MAGIC DP_FOURCC ('C', 'N', 'D', 'A')
/** @brief State-blob layout version (v6: the composer's options). */
#define CARRIER_NDA_STATE_VERSION                                             \
  6u /* v6: the update period, the denominator, the block lock and their     \
        running sums; v5: the arm AGC is gone -- carrier_nda_disc            \
        normalises by its own |z|^M (gh-657) */

  /**
   * @brief Serialized-state byte size.
   * @param state  Must be non-NULL (its contents do not affect the size).
   * @return Bytes: the dp_state_hdr_t envelope plus the whole struct.
   */
  size_t dp_carrier_nda_state_bytes (const dp_carrier_nda_state_t *state);
  /**
   * @brief Serialize the full loop state into @p blob.
   *
   * A whole-struct snapshot (config, NCO, loop filter, boxcar arm, lock
   * EMA and detector) with the telemetry attachment zeroed.
   *
   * @param state  Must be non-NULL.
   * @param blob   Caller-owned buffer of at least
   *               dp_carrier_nda_state_bytes() bytes.
   */
  void dp_carrier_nda_get_state (const dp_carrier_nda_state_t *state, void *blob);
  /**
   * @brief Restore a state written by dp_carrier_nda_get_state().
   *
   * Restores every field, config included, but keeps the receiver's own
   * telemetry attachment. The payload is decoded into a temporary and
   * checked first, so on rejection @p state is untouched.
   *
   * @param state  Must be non-NULL.
   * @param blob   Blob from dp_carrier_nda_get_state().
   * @return DP_OK, or DP_ERR_INVALID on a magic, version, endian or size
   *         mismatch or a payload the object's check refuses.
   */
  int dp_carrier_nda_set_state (dp_carrier_nda_state_t *state, const void *blob);

  /**
   * @brief Output capacity hint for dp_carrier_nda_steps(): always 0.
   *
   * The output is one sample per input, so size @p out to the input
   * length.
   *
   * @param state  Unused.
   * @return 0 (no state-derived bound).
   */
  size_t dp_carrier_nda_steps_max_out (dp_carrier_nda_state_t *state);

  /**
   * @brief De-rotate a cf32 block with the recovered carrier and return the
   * de-rotated stream (one output per input sample).
   *
   * Runs the non-data-aided carrier loop over the block: each sample is
   * wiped off by the integer-phase NCO, the de-rotated sample slides the I/Q
   * moving-average arm, and the M-th-power discriminator (which strips the
   * M-PSK data modulation) steers the NCO frequency and phase. Because the
   * discriminator is data- and timing-independent, this acquires the carrier
   * with no symbol timing and no data present — a bare carrier, or a modulated
   * carrier before timing lock. It resolves to one of m carrier phases (M-fold
   * ambiguity, resolved downstream). Read norm_freq for the tracked carrier
   * (cycles/sample) and lock for the carrier lock metric.
   *
   * @param state    Must be non-NULL.
   * @param x        Input samples (average power at or below unity).
   * @param x_len    Number of input samples.
   * @param out      De-rotated samples, one per input.
   * @param max_out  Capacity of @p out.
   * @return Number of de-rotated samples written to @p out (equals @p x_len);
   *         0, with nothing written, when an update period other than 1 is
   *         set (dp_carrier_nda_set_update_period()).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0,
   * ...                sps=8, n=4, m=4)
   * >>> rng = np.random.default_rng(0)
   * >>> k = np.arange(40000)
   * >>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
   * ...      rng.standard_normal(k.size)
   * ...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
   * >>> y = c.steps(x)                 # de-rotated toward DC
   * >>> y.shape[0]
   * 40000
   * >>> round(c.norm_freq, 4)          # tracked carrier, cycles/sample
   * 0.001
   * >>> c.lock > 0.5                    # carrier lock metric, ~1 at lock
   * True
   *
   * @endcode
   */
  size_t dp_carrier_nda_steps (dp_carrier_nda_state_t *state, const float _Complex *x,
                            size_t x_len, float _Complex *out, size_t max_out);
  /**
   * @brief Tracked carrier frequency: the NCO centre plus the loop's
   * integrated correction (integrator only, smoothed), spread over the
   * update period.
   *
   * Under a frequency ramp it lags the true carrier by the Type-II ramp
   * error; see dp_carrier_nda_get_nco_freq() for the full command.
   *
   * @param state  Must be non-NULL.
   * @return Frequency in cycles/sample; the seed frequency until the loop
   *         has run.
   */
  double dp_carrier_nda_get_norm_freq (const dp_carrier_nda_state_t *state);
  /** @brief Instantaneous NCO frequency command = centre + full loop-filter
   * output (integ + kp*e), cycles/sample. Mean rides a ramp with no lag;
   * variance is the loop stress. See the impl for the estimator-vs-command
   * distinction.
   * @param state  Must be non-NULL.
   * @return Frequency in cycles/sample; the seed frequency until the loop
   *         has run. */
  double dp_carrier_nda_get_nco_freq (const dp_carrier_nda_state_t *state);
  /**
   * @brief Re-seed the loop at a new carrier frequency.
   *
   * Not a gentle nudge: it resets the loop filter, re-initializes the NCO
   * (phase included) at @p val, clears the moving-average arm, the lock
   * EMA and the lock decision, and makes @p val the frequency a later
   * dp_carrier_nda_reset() returns to.
   *
   * @param state  Must be non-NULL.
   * @param val    New carrier centre frequency, cycles/sample.
   */
  void   dp_carrier_nda_set_norm_freq (dp_carrier_nda_state_t *state, double val);
  /**
   * @brief Lock-signal EMA: Re((z/|z|)^M) smoothed per sample by
   * CARRIER_NDA_LOCK_ALPHA.
   * @param state  Must be non-NULL.
   * @return Value from -1 to 1; ~1 at lock, zero-mean (sd
   *         CARRIER_NDA_LOCK_NORM_SD) with no carrier; 0 after
   *         create/reset.
   */
  double dp_carrier_nda_get_lock (const dp_carrier_nda_state_t *state);
  /**
   * @brief Last phase error from the M-th-power discriminator, raw.
   *
   * Im((z/|z|)^M) scaled by 1, 1/2, 1/4 for M = 2, 4, 8, which equalizes
   * the S-curve slope across M at CARRIER_NDA_DISC_SLOPE (2); with an
   * averaged or unit denominator, 2IQ over that denominator, slope 2 at
   * unit amplitude. This is the discriminator's output, as the `.e` probe
   * reads it, NOT what the loop filter is handed: the filter takes it
   * divided by the slope, carrier_nda_loop_error(), slope 1.
   *
   * @param state  Must be non-NULL.
   * @return Dimensionless error, bounded in magnitude by 1; 0 before the
   *         first sample.
   */
  double dp_carrier_nda_get_last_error (const dp_carrier_nda_state_t *state);
  /**
   * @brief Loop noise bandwidth currently in use.
   * @param state  Must be non-NULL.
   * @return bn per update, cycles an update: cycles/sample at the default
   *         update period of 1. The create() value or the last one
   *         dp_carrier_nda_set_bn() accepted.
   */
  double dp_carrier_nda_get_bn (const dp_carrier_nda_state_t *state);
  /** @brief Retune bn, keeping zeta. A bn outside dp_loop_filter_params_ok()
   *  is refused (`DP_ERR_INVALID`) and changes nothing: the gains, their
   *  rad->cycle scale and the reported bn all stay (doppler#2112). As a
   *  writable property a refusal raises ValueError (doppler#1987).
   *  @param state  Must be non-NULL.
   *  @param val    New loop noise bandwidth, cycles/sample.
   *  @return DP_OK, or DP_ERR_INVALID if refused (nothing changes). */
  int    dp_carrier_nda_set_bn (dp_carrier_nda_state_t *state, double val);
  /**
   * @brief Constellation order the discriminator strips.
   * @param state  Must be non-NULL.
   * @return M: 2, 4 or 8, fixed at create.
   */
  int    dp_carrier_nda_get_m (const dp_carrier_nda_state_t *state);
  /**
   * @brief Arm-window divisor: the moving average spans sps / n samples.
   * @param state  Must be non-NULL.
   * @return n, fixed at create.
   */
  int    dp_carrier_nda_get_n (const dp_carrier_nda_state_t *state);
  /**
   * @brief Samples per symbol the arm window is sized from.
   * @param state  Must be non-NULL.
   * @return sps, in samples, fixed at create.
   */
  size_t dp_carrier_nda_get_sps (const dp_carrier_nda_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_NDA_CORE_H */
