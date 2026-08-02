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
 * Raising the (unit-normalized) arm sample `z` to the Mth power strips the
 * M-PSK data modulation, leaving M times the carrier phase — so the
 * discriminator is
 * **independent of the data symbols and of symbol timing**. That is what lets
 * it acquire a bare/unmodulated carrier, or a modulated carrier before timing
 * lock. It is the M-fold-ambiguous acquisition aid; a decision-directed loop
 * gives the low-jitter steady state (resolve the M-fold ambiguity downstream).
 *
 * The M-th power is computed by **repeated complex squaring**
 * (`z²`→`z⁴`→`z⁸`). Each level yields a phase error and a lock signal:
 *   - `phase_error` = `Im(z^M)` scaled by `1, ½, ¼` for M = 2, 4, 8 — the
 * scale normalizes the phase-detector gain so the S-curve slope at lock is 2
 * for every M (one `bn` behaves identically across M).
 *   - `lock_signal`  = `Re((z/|z|)^M)` — the M-th power of a **limited**
 * sample, so it is bounded in ±1 and its H0 variance is 1/2 for **every** M.
 * ~1 when phase-locked, zero-mean with no carrier. That M-independence is what
 * makes one `lock_thresh` mean one Pfa at every order; the threshold chain is
 * derived above `CARRIER_NDA_LOCK_ALPHA`. Its EMA (`lock`) is the carrier lock
 * metric. See `docs/design/mpsk.md` §2.3 for the derivation.
 *
 * The block API (carrier_nda_steps) is the Python face and emits the
 * de-rotated sample stream; the JM_FORCEINLINE
 * carrier_nda_wipeoff()/_arm_step()/_steer() are the C composition API a
 * receiver inlines into its own sample loop (it can also steer the shared NCO
 * with its own decision-directed error on handover).
 *
 * @note **Input average power must be at or below unity.** The arm sample
 * feeding the M-th-power detector is normalized to unit average power by an
 * internal AGC (bandwidth = 0.01*bn) with a 10 dB square clip, so the loop
 * gain is amplitude-invariant. This is the normal convention for
 * captured/scaled baseband (and holds for the DSSS despreader's correlation
 * gain). A cold input more than ~10 dB above unity is out of spec: the
 * deliberately slow AGC cannot normalize it before the discriminator reacts,
 * and the loop can false-lock. The AGC absorbs residual/slow level variation,
 * not a large cold offset.
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

#include "agc/agc_core.h"
#include "boxcar/boxcar_core.h"
#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <math.h>
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
 * `lock_signal = Re((z/|z|)^M)` -- the M-th power of a LIMITED sample. The
 * limiter is on this path only; the phase error keeps the raw |z|^M weighting,
 * which is deliberate (it is the natural matched weighting on a pulse-shaped
 * signal, and flattening it corrupts the phase estimate). Limiting the lock
 * signal is what makes it a detector you can put a number on:
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
 * `CARRIER_NDA_LOCK_DEFAULT_UP` = 0.5 is eta = 4.416, i.e. a per-look
 * Pfa of 5e-6 -- so the long-standing default turns out to BE the Pfa-derived
 * value once the statistic is M-independent. It was only ever meaningful at
 * BPSK before: the same 0.5 was eta = 0.9 at QPSK and eta = 0.02 at 8PSK.
 */
/* EMA smoothing for the lock metric: alpha = det_ema_alpha(0.0, 15.9), giving
 * N_eff = (2-alpha)/alpha = 39 effective looks (>= the 30-look floor). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/* Analytic H0 sd of the limited lock statistic AFTER the EMA above:
 * sqrt(Var_look * alpha/(2-alpha)) with Var_look = 1/2 exactly, for every M.
 * A threshold of `eta * this` has per-look Pfa = Q(eta). */
#define CARRIER_NDA_LOCK_NORM_SD 0.11322770341445956
/* Arm AGC (the embedded log-domain agc_core primitive) — drives the
 * phase-detector input to unit average power so the loop gain is amplitude-
 * invariant. The AGC runs once per moving-average output and MUST stay slow
 * relative to the carrier loop: its bandwidth is locked to a fixed fraction of
 * the carrier loop bandwidth (agc.loop_bw = CARRIER_NDA_AGC_BW_RATIO * bn), so
 * it is always 100× slower and tracks only the overall signal level — never
 * the carrier dynamics or the within-symbol pulse (RRC) envelope. Flattening
 * the envelope would destroy the raw M-th-power discriminator's natural |z|^M
 * weighting and corrupt the phase estimate on pulse-shaped signals. */
#define CARRIER_NDA_AGC_REF_DB 0.0
#define CARRIER_NDA_AGC_BW_RATIO 0.01
#define CARRIER_NDA_AGC_ALPHA 0.01
/* Saturated-amplifier soft clip: the AGC's square clip set 10 dB above the
 * unit level. Bounds the peak (constructive-ISI) arm samples that would
 * otherwise dominate the |z|^M weighting, while constant-modulus samples sit
 * below it and pass through unclipped (keeping the raw-arm squaring-loss
 * advantage). */
#define CARRIER_NDA_AGC_CLIP_DB 10.0

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
    agc_state_t    agc;        /**< per-sample log-domain AGC on the arm sample
                                    (normalizes to unit average power).        */
    double          ctl_cyc; /**< NCO control (cyc/sample) for next wipeoff.*/
    lockdet_state_t lockdet; /**< decision rule: thresholds + verify
                                  counters stepped on `lock` each sample
                                  (mirrors MpskReceiver's own pre-existing
                                  handover step on this same statistic).   */
    carrier_nda_tlm_t tlm;   /**< live telemetry attachment; zeroed in blobs */
  } carrier_nda_state_t;


  /**
   * @brief The M-th-power discriminator on an arm sample (raw, no per-dump
   * limit).
   *
   * Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` directly on the arm
   * sample @p z (the "conventional Costas" / linear-arm form) and writes the
   * phase error (= scaled `Im(z^M)`) and lock signal. The arm sample is
   * expected to be AGC-normalized to unit average power upstream
   * (carrier_nda_arm_step runs the window average through an embedded agc_core
   * AGC) — so a clean window is `|z|≈1` and a transition-straddling window is
   * `|z|<1` and is *down-weighted naturally*. This is deliberate: a per-sample
   * unit-magnitude normalization is Yuen's "polarity-type" hard limiter, the
   * worst nonlinearity (≈2.5–4 dB extra squaring loss, and non-monotonic in
   * SNR — see docs/design/mpsk.md §2.3). On a unit-magnitude `z` the raw and
   * normalized forms coincide, so the S-curve and lock-scale calibration are
   * unchanged.
   *
   * @param z      Arm moving-average sample (AGC-normalized, ~unit at lock).
   * @param m      Constellation order (2, 4, 8).
   * @param pe     Receives the phase error.
   * @param lock   Receives the lock signal.
   */
  JM_FORCEINLINE void
  carrier_nda_disc (float complex z, int m, double *pe, double *lock)
  {
    /* The cascade runs in float: the input is a float complex AGC-normalized
     * to |z|~1 (clip caps it at ~3.16), so even z^8 is O(1)-O(1e4) and float's
     * ~1e-7 relative error is far below what the loop tolerates. Keeping it in
     * float avoids the float->double conversions on this loop-carried critical
     * path; only the two outputs (which feed the double loop filter) promote.
     */
    float i  = crealf (z);    /* raw I (AGC-normalized upstream) */
    float q  = cimagf (z);    /* raw Q                          */
    float p  = i * i + q * q; /* |z|^2                          */
    float bl = i * i - q * q; /* Re(z^2) */
    float be = 2.0f * i * q;  /* Im(z^2) */
    /* The LOCK signal is limited, the PHASE ERROR is not -- see the two
     * paragraphs below carrier_nda_lock_norm_sd. |z|^M is a power of p for
     * every M we support, so limiting costs one divide and no sqrt. */
    if (m == 2)
      {
        *pe   = be;
        *lock = (p > CARRIER_NDA_EPS) ? bl / p : 0.0; /* Re((z/|z|)^2) */
        return;
      }
    float ql = bl * bl - be * be; /* Re(z^4)        */
    float qe = be * bl;           /* Im(z^4) / 2    */
    if (m == 4)
      {
        *pe   = qe;
        *lock = (p > CARRIER_NDA_EPS) ? ql / (p * p) : 0.0; /* Re((z/|z|)^4) */
        return;
      }
    *pe = qe * ql; /* Im(z^8) / 4                             */
    /* Re(z^8) = Re(z^4)^2 - Im(z^4)^2, and `qe` is HALF of Im(z^4) -- that
     * half being the deliberate {1, 1/2, 1/4} phase-error scaling which
     * equalises the S-curve slope across M. So reconstructing Re(z^8) from it
     * needs the 2 squared back: ql*ql - (2*qe)^2. Without the 4 the statistic is
     * Re(z^4)^2 - Im(z^4)^2/4, which is NOT Re(z^8) and, unlike it, is not
     * zero-mean on noise -- E[Re(z^4)^2] = E[Im(z^4)^2] for circular noise, so
     * the shortfall leaves a positive residual of (3/4)E[Im(z^4)^2]. Measured
     * on unit-power complex Gaussian noise, 4e5 samples: mean +8.94 without
     * the 4, -0.11 with it (and bit-identical to Re(z^8) computed directly).
     * The value AT LOCK is +1.0000 either way, which is why this hid: it
     * corrupted only the noise-only tail, i.e. exactly the false-alarm
     * behaviour a lock detector is thresholded on. */
    float p4 = p * p * p * p; /* |z|^8 */
    *lock    = (p4 > CARRIER_NDA_EPS) ? (ql * ql - 4.0f * qe * qe) / p4
                                      : 0.0; /* Re((z/|z|)^8) */
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
   * M-th-power discriminator on the AGC-normalized window average, writes @p
   * pe and @p lock, and returns 1 every call.
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
     * average), then normalize that window sample to unit average power with
     * the embedded AGC so the loop gain is amplitude-invariant (the role the
     * old per-sample |z| divide served, now as a slow feedback loop). agc_step
     * is the exact per-sample AGC — gain-apply, power detector, dB loop filter
     * and square clip in one call. The arm is in the *fast* carrier loop, so
     * the AGC runs per sample (no decimation, no block latency in the feedback
     * path); its own slowness (loop_bw = 0.01*bn, ~100x below the carrier
     * loop) is what keeps it tracking the overall level only — never the
     * carrier dynamics or the within-symbol pulse envelope. The square clip
     * (clip_db) saturates the peak (constructive-ISI) samples while
     * constant-modulus samples pass through, so the raw M-th-power
     * discriminator keeps its squaring-loss advantage. */
    float complex y  = boxcar_step (&s->arm, d);
    float complex zn = agc_step (&s->agc, y);
    carrier_nda_disc (zn, s->m, pe, lock);
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
   * phase, the moving-average arm, AGC, loop-filter integrator and lock EMA are
   * cleared, and the lock detector is dropped. The configured (bn, zeta), the
   * arm geometry (sps, n) and the constellation order m are preserved, so the
   * same object can re-acquire a fresh capture.
   *
   * @param state  Must be non-NULL.
   * @code
   * >>> import numpy as np
   * >>> from doppler.track import CarrierNda
   * >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, m=4)
   * >>> rng = np.random.default_rng(0)
   * >>> k = np.arange(40000)
   * >>> x = (np.exp(2j * np.pi * 0.001 * k) + 0.05 * (
   * ...      rng.standard_normal(k.size)
   * ...      + 1j * rng.standard_normal(k.size))).astype(np.complex64)
   * >>> _ = c.steps(x)
   * >>> round(c.norm_freq, 4), round(c.lock, 2)   # acquired the carrier
   * (0.001, 0.99)
   * >>> c.reset()
   * >>> round(c.norm_freq, 4), round(c.lock, 2)   # back to the seed, unlocked
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
   * loop's probes on it — including the embedded arm AGC's.
   * Registers four probes of its own, emitted once per input sample (this
   * is a sample-rate loop — use @p decim to thin the stream) plus the
   * embedded AGC's "<prefix>.agc.gain_db" (emitted at the AGC's own
   * amortized gain-update rate): "<prefix>.lock" (the lock-signal EMA, ~1
   * when phase-locked), "<prefix>.e" (the M-th-power phase discriminator —
   * the loop stress), "<prefix>.freq" (the tracked carrier frequency,
   * cycles/sample) and "<prefix>.locked" (the verify-counted lockdet
   * decision, 0/1).  Passing NULL detaches the loop and the embedded AGC.
   * Setup path, never hot: call before the producer thread starts
   * stepping; the context is borrowed and must outlive the attachment
   * (SPSC rules in telemetry/telemetry.h).
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
   * >>> sorted(tlm.probe_names())
   * ['car.agc.gain_db', 'car.e', 'car.freq', 'car.lock', 'car.locked']
   * >>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(np.complex64)
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
  4u /* v4: lockdet decision rule (verify counters) */

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
   * >>> c = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, m=4)
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
