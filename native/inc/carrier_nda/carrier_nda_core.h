/**
 * @file carrier_nda_core.h
 * @brief Non-data-aided (NDA) M-th-power carrier-tracking loop.
 *
 * A carrier-recovery loop that locks **without data and without symbol timing**
 * — the cold-start / acquisition counterpart to the decision-directed
 * @ref carrier_mpsk_state_t loop. Per sample it de-rotates the input with the
 * integer-phase @ref lo_state_t NCO (carrier wipe-off); it integrates the
 * de-rotated samples in an I/Q arm integrate-and-dump that **dumps N times per
 * symbol** (every `sps/n` samples), and on each dump runs the **M-th-power**
 * phase discriminator, filters the error through an embedded
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
 * @code
 * // QPSK NDA carrier loop, 8 samples/symbol, 4 arm dumps/symbol
 * carrier_nda_state_t *c = carrier_nda_create(0.01, 0.707, 0.0, 8, 4, 4);
 * float complex derot[1024];
 * size_t k = carrier_nda_steps(c, rx, rx_len, derot, 1024);
 * double f = c->nco.norm_freq;   // tracked carrier (cycles/sample)
 * carrier_nda_destroy(c);
 * @endcode
 */
#ifndef CARRIER_NDA_CORE_H
#define CARRIER_NDA_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the arm-sample magnitude (not tunable). */
#define CARRIER_NDA_EPS 1e-12
/* EMA smoothing for the lock metric (status diagnostic / handover input). */
#define CARRIER_NDA_LOCK_ALPHA 0.05

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
    int n;                   /**< arm integrate-and-dump dumps per symbol. */
    size_t arm_len;          /**< samples per arm dump (= sps / n).        */
    double lock_scale;       /**< per-M lock-signal scale (1/0.619/0.412). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    float complex arm_acc;   /**< running I/Q arm integrate-and-dump.      */
    size_t arm_cnt;          /**< samples accumulated into `arm_acc`.      */
    double lock;             /**< EMA of the lock signal (1 = locked).     */
    double last_error;       /**< last phase discriminator (loop stress).  */
} carrier_nda_state_t;

/**
 * @brief The M-th-power discriminator on a unit-normalized arm sample.
 *
 * Runs the repeated-squaring recursion `z²`→`z⁴`→`z⁸` on the magnitude-
 * normalized arm sample @p z and writes the phase error (= scaled `Im(z^M)`) and
 * lock signal. Amplitude-normalized so the loop gain is independent of the arm
 * amplitude (the project's `|·|`-normalized convention).
 *
 * @param z      Arm integrate-and-dump sample (any magnitude).
 * @param m      Constellation order (2, 4, 8).
 * @param scale  Per-M lock scale (1 / 0.619 / 0.412).
 * @param pe     Receives the phase error.
 * @param lock   Receives the lock signal.
 */
JM_FORCEINLINE void
carrier_nda_disc(float complex z, int m, double scale, double *pe, double *lock)
{
    double mag = (double)cabsf(z) + CARRIER_NDA_EPS;
    double i = (double)crealf(z) / mag; /* unit-magnitude I */
    double q = (double)cimagf(z) / mag; /* unit-magnitude Q */
    double bl = i * i - q * q;          /* Re(z^2) */
    double be = 2.0 * i * q;            /* Im(z^2) */
    if (m == 2)
    {
        *pe = be;
        *lock = scale * bl;
        return;
    }
    double ql = bl * bl - be * be; /* Re(z^4)        */
    double qe = be * bl;           /* Im(z^4) / 2    */
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
 * @param bn              Loop noise bandwidth (per arm-update rate).
 * @param zeta            Damping factor (0.707 = critically damped).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param sps             Samples per symbol.
 * @param n               Arm integrate-and-dump dumps per symbol (sps % n == 0).
 * @param m               Constellation order M (2, 4, 8).
 */
void carrier_nda_init(carrier_nda_state_t *s, double bn, double zeta,
                      double init_norm_freq, size_t sps, int n, int m);

/**
 * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
 * @param s  Carrier loop state.  Must be non-NULL.
 * @param x  One input sample.
 * @return The de-rotated sample to feed the arm integrate-and-dump.
 */
JM_FORCEINLINE JM_HOT float complex
carrier_nda_wipeoff(carrier_nda_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

/**
 * @brief Accumulate a de-rotated sample into the arm I/D; discriminate on dump.
 *
 * Adds @p d to the arm accumulator; every `arm_len` samples it dumps, runs the
 * M-th-power discriminator on the dump, writes @p pe (phase error) and @p lock
 * (lock signal), resets the accumulator, and returns 1. Otherwise returns 0.
 *
 * @param s     Carrier loop state.  Must be non-NULL.
 * @param d     One de-rotated sample (from carrier_nda_wipeoff).
 * @param pe    Receives the phase error on a dump.
 * @param lock  Receives the lock signal on a dump.
 * @return 1 on an arm dump (pe/lock written), 0 otherwise.
 */
JM_FORCEINLINE JM_HOT int
carrier_nda_arm_step(carrier_nda_state_t *s, float complex d, double *pe,
                     double *lock)
{
    s->arm_acc += d;
    if (++s->arm_cnt < s->arm_len)
        return 0;
    float complex z = s->arm_acc / (float)s->arm_len;
    carrier_nda_disc(z, s->m, s->lock_scale, pe, lock);
    s->arm_acc = 0.0f;
    s->arm_cnt = 0;
    return 1;
}

/**
 * @brief Steer the shared NCO with a phase error through the loop filter.
 *
 * Filters @p pe and updates the NCO frequency (per arm-update) + a proportional
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
    loop_filter_step(&s->lf, pe);
    /* lf.integ is rad per arm-update period (arm_len samples) -> rad/sample. */
    double car_w = s->lf.integ / (double)s->arm_len;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    s->nco.phase += (uint32_t)((s->lf.kp * pe) / (2.0 * M_PI) * 4294967296.0);
}

/**
 * @brief Create an NDA carrier loop instance.
 *
 * @param bn              Loop noise bandwidth (default 0.01).
 * @param zeta            Damping factor (default 0.707).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample (default 0.0).
 * @param sps             Samples per symbol (default 8).
 * @param n               Arm dumps per symbol (default 4; sps % n == 0).
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
