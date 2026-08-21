/**
 * @file carrier_nda_core.h
 * @brief Non-data-aided (NDA) M-th-power carrier-tracking loop.
 *
 * A carrier-recovery loop that locks **without data and without symbol
 * timing** — the cold-start / acquisition counterpart to the decision-directed
 * @ref carrier_mpsk_state_t loop. Per sample it de-rotates the input with the
 * integer-phase @ref lo_state_t NCO (carrier wipe-off); it filters the
 * de-rotated samples through a free-running I/Q **boxcar moving average** of
 * `sps/n` samples (one output per input sample — no rate change), and on every
 * sample runs the
 * **M-th-power** phase discriminator, filters the error through an embedded
 * @ref loop_filter_state_t, and steers the NCO frequency + phase.
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
 * The block API (carrier_nda_steps) is the Python face and emits the
 * de-rotated sample stream; the JM_FORCEINLINE
 * carrier_nda_wipeoff()/_arm_step()/_steer() are the C composition API a
 * receiver inlines into its own sample loop (it can also steer the shared NCO
 * with its own decision-directed error on handover).
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
 * predictable. This is the tighter bound at every shipped setting and it is
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
 * carrier_nda_steps does, is the widest setting available and puts the
 * ceiling tens of times beyond loop capture. A composer that instead taps a
 * DECIMATED stream inherits a proportionally tighter ceiling: at two samples
 * per symbol it falls to one symbol rate over M, which is the regime the QPSK
 * false lock above was measured in. The rule for a composition is to choose
 * the tap so the ceiling stays clear of loop capture at every M it supports,
 * which makes the silent failure structurally unreachable and leaves only the
 * loud one.
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
 * carrier_nda_state_t *c = carrier_nda_create(0.01, 0.707, 0.0, 8, 4, 4);
 * float complex derot[1024];
 * size_t k = carrier_nda_steps(c, rx, rx_len, derot, 1024);
 * double f = carrier_nda_get_norm_freq(c); // tracked carrier (cyc/sample)
 * carrier_nda_destroy(c);
 * @endcode
 */
#ifndef CARRIER_NDA_CORE_H
#define CARRIER_NDA_CORE_H

#include "boxcar/boxcar_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "dp_tlm/dp_tlm_core.h"
#include <math.h>
#include "telemetry/telemetry_core.h"
#ifdef __cplusplus
extern "C"
{
#endif

/* Numerical guard on the arm-sample magnitude (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/* rad/sample -> cycles/sample for the NCO control port (replaces /(2*pi)). */
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
/* EMA smoothing for the lock metric: alpha = det_ema_alpha(0.0, 15.9), giving
 * N_eff = (2-alpha)/alpha = 39 effective looks (>= the 30-look floor). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/* Analytic H0 sd of the limited lock statistic AFTER the EMA above:
 * sqrt(Var_look * alpha/(2-alpha)) with Var_look = 1/2 exactly, for every M.
 * A threshold of `eta * this` has per-look Pfa = Q(eta). */
#define CARRIER_NDA_LOCK_NORM_SD 0.11322770341445956

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
  } carrier_nda_tlm_t;

  /**
   * @brief NDA M-th-power carrier loop state.
   *
   * Allocate with carrier_nda_create(), or embed by value and
   * carrier_nda_init(). The carrier NCO (`nco`) and PI loop (`lf`) are public
   * sub-components so a composing receiver can drive the same NCO; treat the
   * arm accumulator and the diagnostics as internal.
   */
  typedef struct
  {
    lo_state_t          nco; /**< integer carrier NCO (uint32 phase).      */
    loop_filter_state_t lf;  /**< 2nd-order carrier PI loop.               */
    size_t              sps; /**< samples per symbol.                      */
    int                 m;   /**< constellation order M (2, 4, 8).         */
    int                 n;   /**< sets the MA window (= a 1/n-symbol box).  */
    size_t arm_len;          /**< moving-average window length (= sps / n). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    boxcar_state_t arm;      /**< I/Q boxcar moving-average arm (sps/n).    */
    double         lock;     /**< EMA of the lock signal (1 = locked).     */
    double         last_error; /**< last phase discriminator (loop stress).  */
    double          ctl_cyc; /**< NCO control (cyc/sample) for next wipeoff.*/
    lockdet_state_t lockdet; /**< decision rule: thresholds + verify
                                  counters stepped on `lock` each sample
                                  (mirrors MpskReceiver's own pre-existing
                                  handover step on this same statistic).   */
    carrier_nda_tlm_t tlm;   /**< live telemetry attachment; zeroed in blobs */
  } carrier_nda_state_t;


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
  JM_FORCEINLINE void
  carrier_nda_disc (float complex z, int m, double *pe, double *lock)
  {
    /* BOTH outputs normalise by the detector's OWN amplitude law, |z|^M.
     *
     * A discriminator's raw output is the phase error multiplied by things it
     * did not choose, and amplitude is the largest of them: Im(z^M) scales as
     * A^M, so a 2x level error is 4x loop gain at BPSK and 256x at 8PSK. Only
     * the detector can divide that out, and it can do it exactly -- |z|^M is a
     * power of p for every M supported here, so it costs one divide and no
     * sqrt. This is the same rule the timing detector follows (a TED
     * normalises by its own slope, symsync_ted_slope()), applied to its
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

  /**
   * @brief Initialise an NDA carrier loop in place (no allocation).
   *
   * @param s               State to initialise.  Must be non-NULL.
   * @param bn              Loop noise bandwidth, cycles/sample (per-sample
   * loop).
   * @param zeta            Damping factor (0.707 = critically damped).
   * @param init_norm_freq  Seed carrier frequency, cycles/sample.
   * @param sps             Samples per symbol.
   * @param n               MA window divisor: window = sps/n samples (sps % n
   * == 0, sps/n <= BOXCAR_MAX_LEN).
   * @param m               Constellation order M (2, 4, 8).
   */
  void carrier_nda_init (carrier_nda_state_t *s, double bn, double zeta,
                         double init_norm_freq, size_t sps, int n, int m);

  /**
   * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
   * @param s  Carrier loop state.  Must be non-NULL.
   * @param x  One input sample.
   * @return The de-rotated sample to feed the moving-average arm.
   */
  JM_FORCEINLINE JM_HOT float complex
  carrier_nda_wipeoff (carrier_nda_state_t *s, float complex x)
  {
    /* De-rotate through the NCO's control port: the LO advances by its centre
     * frequency (phase_inc) plus the loop's last control (ctl_cyc, set by
     * carrier_nda_steer). The LO owns the phase accumulation and scaling. */
    return x * conjf (lo_step_ctrl (&s->nco, s->ctl_cyc));
  }

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
  JM_FORCEINLINE JM_HOT int
  carrier_nda_arm_step (carrier_nda_state_t *s, float complex d, double *pe,
                        double *lock)
  {
    /* Slide the boxcar moving average by one sample (unit gain — pure I/Q
     * average) and discriminate it directly. There is no AGC on this path and
     * none is wanted: carrier_nda_disc normalises by its own amplitude law,
     * so the loop gain is already amplitude-invariant and a second level loop
     * in series would only add its own transient to correct. */
    carrier_nda_disc (boxcar_step (&s->arm, d), s->m, pe, lock);
    return 1;
  }

  /**
   * @brief Steer the shared NCO with a phase error through the loop filter.
   *
   * Filters @p pe and updates the NCO frequency (per sample) + a proportional
   * phase nudge. Shared by the NDA acquisition path and a composing receiver's
   * decision-directed tracking path (handover writes the same NCO).
   *
   * @param s   Carrier loop state.  Must be non-NULL.
   * @param pe  Phase error (NDA discriminator, or a decision-directed error).
   */
  JM_FORCEINLINE JM_HOT void
  carrier_nda_steer (carrier_nda_state_t *s, double pe)
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
     * carrier_nda_get_norm_freq). */
    s->ctl_cyc = loop_filter_step (&s->lf, pe);
  }

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
   * @note Caller must call carrier_nda_destroy() when done.
   */
  carrier_nda_state_t *carrier_nda_create (double bn, double zeta,
                                           double init_norm_freq, size_t sps,
                                           int n, int m);

  /**
   * @brief Destroy an NDA carrier loop instance and release all memory.
   * @param state  May be NULL.
   */
  void carrier_nda_destroy (carrier_nda_state_t *state);

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
  void carrier_nda_reset (carrier_nda_state_t *state);

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
   * "<prefix>.locked" (the verify-counted lockdet decision, 0/1).
   * A composing receiver (the MPSK receiver) calls this once per recovered
   * symbol instead.
   *
   * @param s  State with a non-NULL tlm.ctx (caller-checked).
   */
  void carrier_nda_tlm_flush (const carrier_nda_state_t *s);

  /**
   * @brief Attach (or detach) a telemetry context and register the carrier
   * loop's probes on it.
   * Registers four probes, emitted once per input sample (this is a
   * sample-rate loop — use @p decim to thin the stream): "<prefix>.lock"
   * (the lock-signal EMA, ~1 when phase-locked), "<prefix>.e" (the
   * M-th-power phase discriminator — the loop stress), "<prefix>.freq" (the
   * tracked carrier frequency, cycles/sample) and "<prefix>.locked" (the
   * verify-counted lockdet decision, 0/1).  Passing NULL detaches.
   * Setup path, never hot: call before the producer thread starts
   * stepping; the context is borrowed and must outlive the attachment
   * (SPSC rules in dp_tlm/dp_tlm_core.h).
   * @param state  Must be non-NULL.
   * @param tlm    Telemetry context to attach, or NULL to detach.
   * @param prefix Probe-name prefix, e.g. "car" or "rx.car".
   * @param decim  Emit every decim-th sample; >= 1.
   * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
   *         four probes (the attach fails whole; everything stays
   *         detached).
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> from doppler.telemetry import Telemetry
   * >>> tlm = Telemetry(1 << 14)
   * >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
   * >>> c.set_telemetry(tlm, "car", decim=8)
   * >>> sorted(tlm.probe_names)
   * ['car.e', 'car.freq', 'car.lock', 'car.locked']
   * >>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(
   * ...     np.complex64)
   * >>> _ = c.steps(x)
   * >>> recs = tlm.read()
   * >>> len(recs[recs["probe"] == tlm.probe_id("car.e")]) == 4096 // 8
   * True
   *
   * @endcode
   */
  int carrier_nda_set_telemetry (carrier_nda_state_t *state, dp_tlm_t *tlm,
                                 const char *prefix, uint32_t decim);

  /**
   * @brief Re-tune the carrier lock detector's geometry directly.
   *
   * Full lockdet control, mirroring costas_configure_lock(): a split
   * declare/drop threshold pair on the lock-signal EMA (level hysteresis)
   * and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up /
   * 32 down) start from MpskReceiver's own pre-existing acquisition<->
   * tracking handover thresholds, but size n_up independently: `lock` is
   * a fast per-sample EMA, so consecutive looks are highly autocorrelated
   * and MpskReceiver's own n_up=8 does not compound the false-declare
   * rate the way it would for independent looks (direct Monte Carlo
   * against a noise-only, no-carrier input found real false locks at
   * n_up=8; n_up=64 was the smallest verify count that reliably
   * eliminated them -- see carrier_nda_core.c's CARRIER_NDA_LOCK_DEFAULT_*
   * comment for the exact trial data). A live lock survives the re-tune;
   * the in-flight verify run restarts.
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
  void carrier_nda_configure_lock (carrier_nda_state_t *state,
                                   double up_thresh, double down_thresh,
                                   uint32_t n_up, uint32_t n_down);

  /** @brief Current lock decision (1 = locked, 0 = not), with the
   *         configured verify-count / hysteresis rule applied (see
   *         carrier_nda_configure_lock). */
  int carrier_nda_get_locked (const carrier_nda_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop
 * exactly.
 */
#define CARRIER_NDA_STATE_MAGIC DP_FOURCC ('C', 'N', 'D', 'A')
#define CARRIER_NDA_STATE_VERSION                                             \
  5u /* v5: the arm AGC is gone -- carrier_nda_disc normalises by its own    \
        |z|^M, so nothing upstream has to manufacture |z| = 1 (gh-657) */

  /** @brief Serialized-state byte size. */
  size_t carrier_nda_state_bytes (const carrier_nda_state_t *state);
  /** @brief Serialize the full loop state into @p blob. */
  void carrier_nda_get_state (const carrier_nda_state_t *state, void *blob);
  /** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects.
   */
  int carrier_nda_set_state (carrier_nda_state_t *state, const void *blob);

  size_t carrier_nda_steps_max_out (carrier_nda_state_t *state);

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
   * @return Number of de-rotated samples written to @p out (equals @p x_len).
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
  size_t carrier_nda_steps (carrier_nda_state_t *state, const float complex *x,
                            size_t x_len, float complex *out, size_t max_out);
  double carrier_nda_get_norm_freq (const carrier_nda_state_t *state);
  /** @brief Instantaneous NCO frequency command = centre + full loop-filter
   * output (integ + kp*e), cycles/sample. Mean rides a ramp with no lag;
   * variance is the loop stress. See the impl for the estimator-vs-command
   * distinction. */
  double carrier_nda_get_nco_freq (const carrier_nda_state_t *state);
  void   carrier_nda_set_norm_freq (carrier_nda_state_t *state, double val);
  double carrier_nda_get_lock (const carrier_nda_state_t *state);
  double carrier_nda_get_last_error (const carrier_nda_state_t *state);
  double carrier_nda_get_bn (const carrier_nda_state_t *state);
  void   carrier_nda_set_bn (carrier_nda_state_t *state, double val);
  int    carrier_nda_get_m (const carrier_nda_state_t *state);
  int    carrier_nda_get_n (const carrier_nda_state_t *state);
  size_t carrier_nda_get_sps (const carrier_nda_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_NDA_CORE_H */
