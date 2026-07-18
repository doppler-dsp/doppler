/**
 * @file costas_core.h
 * @brief Costas carrier-tracking loop (integer-NCO de-rotation + PI loop).
 *
 * A continuous BPSK carrier-recovery loop: per sample it de-rotates the input
 * with the integer-phase `lo` NCO (carrier wipe-off); every `tsamps` samples it
 * dumps the coherent integrate-and-dump accumulator, runs a decision-directed
 * Costas phase discriminator, filters the error through an embedded 2nd-order
 * @ref loop_filter_state_t, and steers the NCO frequency + phase.  It tracks a
 * small *residual* carrier offset (the bulk Doppler is removed upstream by FFT
 * acquisition); the steering NCO is `lo`, so the phase is bounded and exactly
 * reproducible (no double-accumulator drift).
 *
 * The block API (costas_steps) is the Python face; the JM_FORCEINLINE
 * costas_wipeoff()/costas_update() are the C composition API a despreader /
 * tracking channel inlines into its own sample loop.
 *
 * Lifecycle: `costas_create -> (steps / configure / reset)* -> costas_destroy`,
 * or embed by value with costas_init().
 *
 * Set `bn_fll > 0` to enable FLL assist (a wide-pull-in frequency-lock loop
 * aiding the PLL) for large or fast-moving residuals; `bn_fll = 0` is a pure
 * Costas PLL.
 *
 * @code
 * costas_state_t *c = costas_create(0.05, 0.707, 0.01, 64, 0.0);
 * float complex sym[16];
 * size_t k = costas_steps(c, rx, rx_len, sym, 16);  // one prompt per symbol
 * double f = c->nco.norm_freq;                       // tracked residual
 * costas_destroy(c);
 * @endcode
 */
#ifndef COSTAS_CORE_H
#define COSTAS_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define COSTAS_EPS 1e-12f
/* EMA smoothing for the |Re P|/|P| lock metric (status diagnostic). */
#define COSTAS_LOCK_ALPHA 0.1

/**
 * @brief Telemetry attachment: a borrowed context + this object's probe
 *        ids.  NULL ctx (the default) means detached — every probe site
 *        is then a single predicted-not-taken branch per symbol.  Zeroed
 *        in state blobs and preserved across set_state
 *        (DP_DEFINE_POD_STATE_TLM).
 */
typedef struct {
    dp_tlm_t *ctx;     /**< NULL = detached                        */
    int32_t id_lock;   /**< "<prefix>.lock" — lock-metric EMA      */
    int32_t id_e;      /**< "<prefix>.e"    — PLL discriminator    */
    int32_t id_freq;   /**< "<prefix>.freq" — tracked NCO freq     */
    int32_t id_locked; /**< "<prefix>.locked" — lockdet flag 0/1   */
} costas_tlm_t;

/**
 * @brief Costas loop state.
 *
 * Allocate with costas_create(), or embed by value and costas_init().
 * The carrier NCO (`nco`) and PI loop (`lf`) are public sub-components so the
 * inline composition helpers can drive them; treat the integrate-and-dump and
 * diagnostic fields as internal.
 */
typedef struct {
    lo_state_t nco;          /**< integer carrier NCO (uint32 phase).      */
    loop_filter_state_t lf;  /**< 2nd-order carrier PI loop (PLL).         */
    size_t tsamps;           /**< samples per symbol (integrate-and-dump). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    double bn_fll;           /**< FLL-assist bandwidth (0 = pure PLL).     */
    double k_fll;            /**< derived FLL gain (per-symbol freq pull).  */
    float complex acc;       /**< running coherent I&D accumulator.        */
    size_t acc_n;            /**< samples accumulated into `acc`.          */
    float complex prev;      /**< previous symbol's prompt (FLL cross).    */
    int have_prev;           /**< prev valid (skip FLL on the 1st symbol). */
    double lock_metric;      /**< EMA of |Re P|/|P| (1 = locked).          */
    lockdet_state_t lock;    /**< decision rule on lock_metric: thresholds
                                  + verify counters, stepped per symbol.   */
    double last_error;       /**< last PLL discriminator (loop stress).    */
    costas_tlm_t tlm;        /**< live telemetry attachment; zeroed in blobs */
} costas_state_t;

/**
 * @brief Initialise a Costas loop in place (no allocation).
 *
 * The by-value counterpart to costas_create(): a tracking channel that embeds
 * a costas_state_t initialises it here.  Seeds the NCO at @p init_norm_freq and
 * the loop integrator to the matching per-symbol frequency so de-rotation is
 * correct from the first sample.
 *
 * @param s               State to initialise.  Must be non-NULL.
 * @param bn              Loop noise bandwidth, normalised to the symbol rate.
 * @param zeta            Damping factor (0.707 = critically damped).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param tsamps          Samples per symbol (the integrate-and-dump period).
 * @param bn_fll          FLL-assist bandwidth (0 = pure PLL).
 */
void costas_init(costas_state_t *s, double bn, double zeta,
                 double init_norm_freq, size_t tsamps, double bn_fll);

/**
 * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
 *
 * `x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the
 * matched-filter integrate-and-dump.  Inline, zero call overhead.
 *
 * @param s  Costas state.  Must be non-NULL.
 * @param x  One input sample.
 * @return The de-rotated sample to feed the integrator.
 */
JM_FORCEINLINE JM_HOT float complex
costas_wipeoff(costas_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

/**
 * @brief Per-symbol carrier update: discriminator -> loop filter -> steer NCO.
 *
 * Runs the decision-directed BPSK Costas discriminator on the prompt @p P,
 * filters it, and writes the new frequency (lo_set_norm_freq) plus a
 * proportional phase nudge into the NCO.  Updates the lock metric and
 * last_error (the instantaneous loop stress).  Inline for composition.
 *
 * @param s  Costas state.  Must be non-NULL.
 * @param P  The dumped integrate-and-dump prompt for this symbol.
 */
JM_FORCEINLINE JM_HOT void
costas_update(costas_state_t *s, float complex P)
{
    float reP = crealf(P), imP = cimagf(P);
    float aP = cabsf(P) + COSTAS_EPS;
    double e = (double)(((reP >= 0.0f) ? imP : -imP) / aP);
    s->last_error = e;
    /* FLL assist: a decision-directed cross-product frequency discriminator
     * has a far wider linear range than the phase discriminator, so it pulls
     * the loop's frequency integrator onto a large/moving residual the bare
     * PLL cannot. Both prompts are data-wiped (multiplied by their Re sign)
     * so a BPSK bit flip between symbols does not corrupt the cross product.
     * The result (~Delta-phase per symbol, rad) nudges integ directly. */
    if (s->k_fll > 0.0 && s->have_prev)
    {
        float rpr = crealf(s->prev), ipr = cimagf(s->prev);
        float sc = (reP >= 0.0f) ? 1.0f : -1.0f;
        float sp = (rpr >= 0.0f) ? 1.0f : -1.0f;
        float ic = reP * sc, qc = imP * sc;       /* data-wiped current */
        float ip = rpr * sp, qp = ipr * sp;       /* data-wiped previous */
        float cross = ip * qc - qp * ic;          /* Im(conj(prev)*cur) */
        float apr = cabsf(s->prev) + COSTAS_EPS;
        double freq_err = (double)cross / ((double)aP * (double)apr);
        s->lf.integ += s->k_fll * freq_err;
    }
    s->prev = P;
    s->have_prev = 1;
    loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> cycles -> uint32 phase
     * delta, via the one shared primitive (a bare truncating cast here
     * is UB on a negative value -- see nco_norm_to_inc()'s own doc). */
    s->nco.phase += nco_norm_to_inc ((s->lf.kp * e) / (2.0 * M_PI));
    /* lock metric: |Re|/|P| EMA (1 = phase-locked BPSK, ~0 = no carrier) */
    double inst = (double)(fabsf(reP) / aP);
    s->lock_metric += COSTAS_LOCK_ALPHA * (inst - s->lock_metric);
    /* verify-counted decision on the smoothed metric (lockdet_core.h):
     * hysteresis keeps a metric grazing the threshold from chattering
     * `locked`. Inline POD step — no call, one branch per symbol. */
    (void)lockdet_step(&s->lock, s->lock_metric);
}

/**
 * @brief Create a Costas instance.
 *
 * @param bn              Loop noise bandwidth (default 0.05).
 * @param zeta            Damping factor (default 0.707).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample (default 0.0).
 * @param tsamps          Samples per symbol (default 64).
 * @param bn_fll          FLL-assist bandwidth (default 0.0 = pure PLL).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call costas_destroy() when done.
 */
costas_state_t *costas_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll);

/**
 * @brief Destroy a Costas instance and release all memory.
 * @param state  May be NULL.
 */
void costas_destroy(costas_state_t *state);

/**
 * @brief Re-seed the loop to its create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void costas_reset(costas_state_t *state);

/**
 * @brief Emit the carrier loop's telemetry records for the symbol just
 * dumped.
 *
 * Out-of-line on purpose: the emit machinery must not inline into a
 * per-sample hot loop (inlined ring-write expansions bloat the loop body
 * and an extern call site forces per-iteration state reloads — both
 * measured ~20% slower detached on other loops). Callers gate on
 * `s->tlm.ctx` and call this once per dumped symbol. Records
 * "<prefix>.lock" (the |Re P|/|P| lock-metric EMA), "<prefix>.e" (the
 * last PLL discriminator — the loop stress) and "<prefix>.freq" (the
 * tracked NCO frequency, cycles/sample). A composing tracking channel
 * (the DSSS despreader) calls this from its own per-epoch update.
 *
 * @param s  State with a non-NULL tlm.ctx (caller-checked).
 */
void costas_tlm_flush(const costas_state_t *s);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct (embedded NCO + loop filter + I&D accumulators), so
 * a whole-struct snapshot resumes the loop exactly. */
#define COSTAS_STATE_MAGIC DP_FOURCC('C', 'S', 'T', 'S')
#define COSTAS_STATE_VERSION 3u /* v3: lockdet decision rule */

/** @brief Serialized-state byte size. */
size_t costas_state_bytes(const costas_state_t *state);
/** @brief Serialize the full loop state into @p blob. */
void costas_get_state(const costas_state_t *state, void *blob);
/** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects. */
int costas_set_state(costas_state_t *state, const void *blob);

size_t costas_steps_max_out(costas_state_t *state);
size_t costas_steps(costas_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void costas_configure(costas_state_t *state, double bn, double zeta);
double costas_get_bn(const costas_state_t *state);
void costas_set_bn(costas_state_t *state, double val);
double costas_get_norm_freq(const costas_state_t *state);
void costas_set_norm_freq(costas_state_t *state, double val);
double costas_get_lock_metric(const costas_state_t *state);
double costas_get_last_error(const costas_state_t *state);
double costas_get_bn_fll(const costas_state_t *state);
void costas_set_bn_fll(costas_state_t *state, double val);

/**
 * @brief Re-tune the carrier lock detector's thresholds and verify counts.
 *
 * The always-on lock decision steps a verify-counted detector
 * (lockdet_core.h) on the |Re P|/|P| lock-metric EMA once per dumped
 * symbol: `locked` flips up after @p n_up consecutive symbols with the
 * metric above @p up_thresh and drops after @p n_down consecutive symbols
 * below @p down_thresh. The defaults derive from the metric's own H0
 * statistics — with no carrier, |Re P|/|P| = |cos(theta)| for a uniform
 * theta, whose mean is 2/pi (~0.637) and per-symbol std ~0.31; the
 * COSTAS_LOCK_ALPHA = 0.1 EMA reduces that to ~0.071, so the default
 * declare threshold 0.85 sits ~3 sigma above the no-carrier mean, with
 * the drop threshold at 0.78 for level hysteresis and 8-up/32-down verify
 * counts for time hysteresis (declare fast, drop reluctantly — the EMA
 * already correlates adjacent looks, so the counts guard against
 * band-edge dwell rather than compounding i.i.d. probabilities). A live
 * lock survives the re-tune; the in-flight verify run restarts.
 *
 * @param state        Costas state.  Must be non-NULL.
 * @param up_thresh    Declare threshold on the lock-metric EMA.
 * @param down_thresh  Drop threshold (<= up_thresh for level hysteresis).
 * @param n_up         Consecutive above-threshold symbols to declare;
 *                     clamped to >= 1.
 * @param n_down       Consecutive below-threshold symbols to drop;
 *                     clamped to >= 1.
 * @code
 * >>> from doppler.track import Costas
 * >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
 * >>> c.locked
 * False
 * >>> c.configure_lock(0.9, 0.8, 4, 16)   # tighter declare, faster drop
 *
 * @endcode
 */
void costas_configure_lock(costas_state_t *state, double up_thresh,
                           double down_thresh, uint32_t n_up,
                           uint32_t n_down);

/** @brief Current carrier lock decision (1 = locked, 0 = not), from the
 *         verify-counted detector on the lock-metric EMA (see
 *         costas_configure_lock). */
int costas_get_locked(const costas_state_t *state);

/**
 * @brief Attach (or detach) a telemetry context and register the carrier
 * loop's probes on it.
 * Registers four probes, emitted once per dumped symbol and further
 * thinned by decim: "<prefix>.lock" (the |Re P|/|P| lock-metric EMA, 1 =
 * phase-locked), "<prefix>.e" (the PLL discriminator output — the loop
 * stress), "<prefix>.freq" (the tracked NCO frequency, cycles/sample) and
 * "<prefix>.locked" (the verify-counted lock decision, 0/1 — see
 * costas_configure_lock). Passing NULL detaches.  Setup path, never hot:
 * call before the producer thread starts stepping; the context is
 * borrowed and must outlive the attachment (SPSC rules in
 * telemetry/telemetry.h).
 * @param state  Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "car" or "ch0.car".
 * @param decim  Emit every decim-th symbol; >= 1.
 * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
 *         four probes (the attach fails whole; the object stays
 *         detached).
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import Costas
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
 * >>> c.set_telemetry(tlm, "car")
 * >>> sorted(tlm.probe_names())
 * ['car.e', 'car.freq', 'car.lock', 'car.locked']
 * >>> x = np.ones(64 * 100, dtype=np.complex64)
 * >>> _ = c.steps(x)
 * >>> recs = tlm.read()   # four records per dumped symbol
 * >>> len(recs) == 4 * 100
 * True
 *
 * @endcode
 */
int costas_set_telemetry(costas_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);
#ifdef __cplusplus
}
#endif

#endif /* COSTAS_CORE_H */
