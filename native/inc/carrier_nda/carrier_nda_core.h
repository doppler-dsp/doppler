/**
 * @file carrier_nda_core.h
 * @brief Non-data-aided (NDA) M-th-power carrier-tracking loop.
 *
 * A carrier-recovery loop that locks **without data and without symbol timing**
 * — the cold-start / acquisition counterpart to the decision-directed
 * @ref carrier_mpsk_state_t loop. Per sample it de-rotates the input with the
 * integer-phase @ref lo_state_t NCO (carrier wipe-off); it filters the de-rotated
 * samples through a free-running I/Q **boxcar moving average** of `sps/n` samples
 * (one output per input sample — no rate change), and on every sample runs the
 * **M-th-power** phase discriminator, filters the error through an embedded
 * @ref loop_filter_state_t, and steers the NCO frequency + phase.
 *
 * Raising the (unit-normalized) arm sample `z` to the Mth power strips the M-PSK
 * data modulation, leaving M times the carrier phase — so the discriminator is
 * **independent of the data symbols and of symbol timing**. That is what lets it
 * acquire a bare/unmodulated carrier, or a modulated carrier before timing lock.
 * It is the M-fold-ambiguous acquisition aid; a decision-directed loop gives the
 * low-jitter steady state (resolve the M-fold ambiguity downstream).
 *
 * The M-th power is computed by **repeated complex squaring** (`z²`→`z⁴`→`z⁸`).
 * Each level yields a phase error and a lock signal:
 *   - `phase_error` = `Im(z^M)` scaled by `1, ½, ¼` for M = 2, 4, 8 — the scale
 *     normalizes the phase-detector gain so the S-curve slope at lock is 2 for
 *     every M (one `bn` behaves identically across M).
 *   - `lock_signal`  = `Re(z^M)` (× a per-M `lock_scale`) for M ≤ 4, and a
 *     faithful monotone lock detector for M = 8 — ~1 when phase-locked, ~0 with
 *     no carrier. Its EMA (`lock`) is the carrier lock metric.
 * See `docs/design/mpsk.md` §2.3 for the derivation.
 *
 * The block API (carrier_nda_steps) is the Python face and emits the de-rotated
 * sample stream; the JM_FORCEINLINE carrier_nda_wipeoff()/_arm_step()/_steer()
 * are the C composition API a receiver inlines into its own sample loop (it can
 * also steer the shared NCO with its own decision-directed error on handover).
 *
 * @note **Input average power must be at or below unity.** The arm sample
 * feeding the M-th-power detector is normalized to unit average power by an
 * internal AGC (bandwidth = 0.01*bn) with a 10 dB square clip, so the loop gain
 * is amplitude-invariant. This is the normal convention for captured/scaled
 * baseband (and holds for the DSSS despreader's correlation gain). A cold input
 * more than ~10 dB above unity is out of spec: the deliberately slow AGC cannot
 * normalize it before the discriminator reacts, and the loop can false-lock.
 * The AGC absorbs residual/slow level variation, not a large cold offset.
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
#include "loop_filter/loop_filter_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the arm-sample magnitude (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/* rad/sample -> cycles/sample for the NCO control port (replaces /(2*pi)). */
#define CARRIER_NDA_INV_2PI 0.15915494309189535 /* 1 / (2*pi) */
/* EMA smoothing for the lock metric (status diagnostic / handover input). */
#define CARRIER_NDA_LOCK_ALPHA 0.05
/* Arm AGC (the embedded log-domain agc_core primitive) — drives the
 * phase-detector input to unit average power so the loop gain is amplitude-
 * invariant. The AGC runs once per moving-average output and MUST stay slow
 * relative to the carrier loop: its bandwidth is locked to a fixed fraction of
 * the carrier loop bandwidth (agc.loop_bw = CARRIER_NDA_AGC_BW_RATIO * bn), so
 * it is always 100× slower and tracks only the overall signal level — never the
 * carrier dynamics or the within-symbol pulse (RRC) envelope. Flattening the
 * envelope would destroy the raw M-th-power discriminator's natural |z|^M
 * weighting and corrupt the phase estimate on pulse-shaped signals. */
#define CARRIER_NDA_AGC_REF_DB 0.0
#define CARRIER_NDA_AGC_BW_RATIO 0.01
#define CARRIER_NDA_AGC_ALPHA 0.01
/* Saturated-amplifier soft clip: the AGC's square clip set 10 dB above the unit
 * level. Bounds the peak (constructive-ISI) arm samples that would otherwise
 * dominate the |z|^M weighting, while constant-modulus samples sit below it and
 * pass through unclipped (keeping the raw-arm squaring-loss advantage). */
#define CARRIER_NDA_AGC_CLIP_DB 10.0

/**
 * @brief NDA M-th-power carrier loop state.
 *
 * Allocate with carrier_nda_create(), or embed by value and carrier_nda_init().
 * The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so a
 * composing receiver can drive the same NCO; treat the arm accumulator and the
 * diagnostics as internal.
 */
typedef struct {
    lo_state_t nco;          /**< integer carrier NCO (uint32 phase).      */
    loop_filter_state_t lf;  /**< 2nd-order carrier PI loop.               */
    size_t sps;              /**< samples per symbol.                      */
    int m;                   /**< constellation order M (2, 4, 8).         */
    int n;                   /**< sets the MA window (= a 1/n-symbol box).  */
    size_t arm_len;          /**< moving-average window length (= sps / n). */
    double lock_scale;       /**< per-M lock-signal scale (1/0.619/0.412). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    boxcar_state_t arm;      /**< I/Q boxcar moving-average arm (sps/n).    */
    double lock;             /**< EMA of the lock signal (1 = locked).     */
    double last_error;       /**< last phase discriminator (loop stress).  */
    agc_state_t agc;         /**< per-sample log-domain AGC on the arm sample
                                  (normalizes to unit average power).        */
    double ctl_cyc;          /**< NCO control (cyc/sample) for next wipeoff.*/
} carrier_nda_state_t;

/**
 * @brief The M-th-power discriminator on an arm sample (raw, no per-dump limit).
 *
 * Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` directly on the arm
 * sample @p z (the "conventional Costas" / linear-arm form) and writes the phase
 * error (= scaled `Im(z^M)`) and lock signal. The arm sample is expected to be
 * AGC-normalized to unit average power upstream (carrier_nda_arm_step runs the
 * window average through an embedded agc_core AGC) — so a clean window is `|z|≈1`
 * and a transition-straddling window is `|z|<1` and is *down-weighted naturally*.
 * This is deliberate: a per-sample unit-magnitude normalization is Yuen's
 * "polarity-type" hard limiter, the worst nonlinearity (≈2.5–4 dB extra squaring
 * loss, and non-monotonic in SNR — see docs/design/mpsk.md §2.3). On a
 * unit-magnitude `z` the raw and normalized forms coincide, so the S-curve and
 * lock-scale calibration are unchanged.
 *
 * @param z      Arm moving-average sample (AGC-normalized, ~unit at lock).
 * @param m      Constellation order (2, 4, 8).
 * @param scale  Per-M lock scale (1 / 0.619 / 0.412).
 * @param pe     Receives the phase error.
 * @param lock   Receives the lock signal.
 */
JM_FORCEINLINE void
carrier_nda_disc(float complex z, int m, double scale, double *pe, double *lock)
{
    /* The cascade runs in float: the input is a float complex AGC-normalized to
     * |z|~1 (clip caps it at ~3.16), so even z^8 is O(1)-O(1e4) and float's
     * ~1e-7 relative error is far below what the loop tolerates. Keeping it in
     * float avoids the float->double conversions on this loop-carried critical
     * path; only the two outputs (which feed the double loop filter) promote. */
    float i = crealf(z); /* raw I (AGC-normalized upstream) */
    float q = cimagf(z); /* raw Q                          */
    float bl = i * i - q * q; /* Re(z^2) */
    float be = 2.0f * i * q;  /* Im(z^2) */
    if (m == 2)
    {
        *pe = be;
        *lock = scale * bl;
        return;
    }
    float ql = bl * bl - be * be; /* Re(z^4)        */
    float qe = be * bl;           /* Im(z^4) / 2    */
    if (m == 4)
    {
        *pe = qe;
        *lock = scale * ql;
        return;
    }
    *pe = qe * ql;                       /* Im(z^8) / 4               */
    *lock = scale * (ql * ql - qe * qe); /* faithful 8-PSK lock det.  */
}

/**
 * @brief Initialise an NDA carrier loop in place (no allocation).
 *
 * @param s               State to initialise.  Must be non-NULL.
 * @param bn              Loop noise bandwidth, cycles/sample (per-sample loop).
 * @param zeta            Damping factor (0.707 = critically damped).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param sps             Samples per symbol.
 * @param n               MA window divisor: window = sps/n samples (sps % n == 0,
 *                        sps/n <= BOXCAR_MAX_LEN).
 * @param m               Constellation order M (2, 4, 8).
 */
void carrier_nda_init(carrier_nda_state_t *s, double bn, double zeta,
                      double init_norm_freq, size_t sps, int n, int m);

/**
 * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
 * @param s  Carrier loop state.  Must be non-NULL.
 * @param x  One input sample.
 * @return The de-rotated sample to feed the moving-average arm.
 */
JM_FORCEINLINE JM_HOT float complex
carrier_nda_wipeoff(carrier_nda_state_t *s, float complex x)
{
    /* De-rotate through the NCO's control port: the LO advances by its centre
     * frequency (phase_inc) plus the loop's last control (ctl_cyc, set by
     * carrier_nda_steer). The LO owns the phase accumulation and scaling. */
    return x * conjf(lo_step_ctrl(&s->nco, s->ctl_cyc));
}

/**
 * @brief Slide the moving-average arm by one sample; discriminate the output.
 *
 * The arm is a free-running boxcar **moving average** of the last `arm_len`
 * de-rotated samples — one output per input sample, **no rate change** (not a
 * decimating integrate-and-dump). It updates the running window sum in O(1)
 * (add @p d, subtract the sample leaving the window), runs the M-th-power
 * discriminator on the AGC-normalized window average, writes @p pe and @p lock,
 * and returns 1 every call.
 *
 * @param s     Carrier loop state.  Must be non-NULL.
 * @param d     One de-rotated sample (from carrier_nda_wipeoff).
 * @param pe    Receives the phase error.
 * @param lock  Receives the lock signal.
 * @return Always 1 (one discriminator output per input sample).
 */
JM_FORCEINLINE JM_HOT int
carrier_nda_arm_step(carrier_nda_state_t *s, float complex d, double *pe,
                     double *lock)
{
    /* Slide the boxcar moving average by one sample (unit gain — pure I/Q
     * average), then normalize that window sample to unit average power with the
     * embedded AGC so the loop gain is amplitude-invariant (the role the old
     * per-sample |z| divide served, now as a slow feedback loop). agc_step is
     * the exact per-sample AGC — gain-apply, power detector, dB loop filter and
     * square clip in one call. The arm is in the *fast* carrier loop, so the AGC
     * runs per sample (no decimation, no block latency in the feedback path);
     * its own slowness (loop_bw = 0.01*bn, ~100x below the carrier loop) is what
     * keeps it tracking the overall level only — never the carrier dynamics or
     * the within-symbol pulse envelope. The square clip (clip_db) saturates the
     * peak (constructive-ISI) samples while constant-modulus samples pass
     * through, so the raw M-th-power discriminator keeps its squaring-loss
     * advantage. */
    float complex y  = boxcar_step(&s->arm, d);
    float complex zn = agc_step(&s->agc, y);
    carrier_nda_disc(zn, s->m, s->lock_scale, pe, lock);
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
carrier_nda_steer(carrier_nda_state_t *s, double pe)
{
    s->last_error = pe;
    /* The PI loop filter output (integ + kp*pe) is the NCO frequency command.
     * config_loop folds the rad->cycle constant (1/2*pi) into kp/ki, so the
     * output is already in cycles/sample — store it directly as the control the
     * next wipeoff feeds to the LO's control port (no per-sample conversion).
     * The LO does the cycles->phase scaling and phase accumulation, so the loop
     * never touches the integer phase. The loop filter is init'd with t = 1 (the
     * MA arm updates every sample), so bn is cycles/sample and n-invariant — n
     * only sets the window length. lf.integ is thus the carrier frequency
     * correction in cycles/sample (read back by carrier_nda_get_norm_freq). */
    s->ctl_cyc = loop_filter_step(&s->lf, pe);
}

/**
 * @brief Create an NDA carrier loop instance.
 *
 * @param bn              Loop noise bandwidth (default 0.01).
 * @param zeta            Damping factor (default 0.707).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample (default 0.0).
 * @param sps             Samples per symbol (default 8).
 * @param n               MA window divisor: window = sps/n (default 4; sps%n==0).
 * @param m               Constellation order M, 2/4/8 (default 4 = QPSK).
 * @return Heap-allocated state, or NULL on invalid args / allocation failure.
 * @note Caller must call carrier_nda_destroy() when done.
 */
carrier_nda_state_t *carrier_nda_create(double bn, double zeta, double init_norm_freq, size_t sps, int n, int m);

/**
 * @brief Destroy an NDA carrier loop instance and release all memory.
 * @param state  May be NULL.
 */
void carrier_nda_destroy(carrier_nda_state_t *state);

/**
 * @brief Re-seed the loop to its create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void carrier_nda_reset(carrier_nda_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop exactly.
 */
#define CARRIER_NDA_STATE_MAGIC DP_FOURCC('C', 'N', 'D', 'A')
#define CARRIER_NDA_STATE_VERSION 2u /* v2: moving-average arm (ring + sum) */

/** @brief Serialized-state byte size. */
size_t carrier_nda_state_bytes(const carrier_nda_state_t *state);
/** @brief Serialize the full loop state into @p blob. */
void carrier_nda_get_state(const carrier_nda_state_t *state, void *blob);
/** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects. */
int carrier_nda_set_state(carrier_nda_state_t *state, const void *blob);

size_t carrier_nda_steps_max_out(carrier_nda_state_t *state);
size_t carrier_nda_steps(carrier_nda_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
double carrier_nda_get_norm_freq(const carrier_nda_state_t *state);
void carrier_nda_set_norm_freq(carrier_nda_state_t *state, double val);
double carrier_nda_get_lock(const carrier_nda_state_t *state);
double carrier_nda_get_last_error(const carrier_nda_state_t *state);
double carrier_nda_get_bn(const carrier_nda_state_t *state);
void carrier_nda_set_bn(carrier_nda_state_t *state, double val);
int carrier_nda_get_m(const carrier_nda_state_t *state);
int carrier_nda_get_n(const carrier_nda_state_t *state);
size_t carrier_nda_get_sps(const carrier_nda_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_NDA_CORE_H */
