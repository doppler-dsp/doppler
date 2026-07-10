/**
 * @file dll_core.h
 * @brief Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking.
 *
 * Tracks the code phase of a continuous, repeating spreading code (e.g. a PN /
 * Gold sequence) on a *carrier-wiped* sample stream. Per sample it correlates
 * the input against three taps of the local code — early (`+spacing` chips),
 * prompt, late (`-spacing` chips) — accumulating an integrate-and-dump over one
 * code period; per period it runs the non-coherent envelope discriminator
 * `(|E| - |L|) / (|E| + |L|)`, filters it through an embedded 2nd-order
 * @ref loop_filter_state_t, and steers the code rate + phase.
 *
 * It pairs with the carrier loop (costas_core.h): the carrier loop wipes the
 * carrier, the DLL wipes the code. The block API (dll_steps) is the Python face;
 * the
 * JM_FORCEINLINE dll_accumulate()/dll_update() are the C composition API a
 * tracking channel inlines into its own sample loop.
 *
 * Lifecycle: dll_create -> [steps / configure / reset]* -> dll_destroy, or embed
 * by value with dll_init() (which BORROWS the caller-owned code).
 *
 * @code
 * uint8_t code[31] = { ... };  // 0/1 chips, one period
 * dll_state_t *d = dll_create(code, 31, 2, 0.0, 0.01, 0.707, 0.5);
 * float complex sym[16];
 * size_t k = dll_steps(d, rx, rx_len, sym, 16);  // one prompt per period
 * double phase = d->chip_pos;                     // tracked code phase, chips
 * dll_destroy(d);
 * @endcode
 */
#ifndef DLL_CORE_H
#define DLL_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lockdet/lockdet_core.h"
#include "loop_filter/loop_filter_core.h"
#include "telemetry/telemetry.h"
#include <complex.h>
#include "detection/detection_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

/**
 * @brief Telemetry attachment: a borrowed context + this object's probe
 *        ids.  NULL ctx (the default) means detached — every probe site
 *        is then a single predicted-not-taken branch per code epoch.
 *        Zeroed in state blobs and preserved across set_state (the
 *        hand-written triplet treats it like the borrowed `code`).
 */
typedef struct {
    dp_tlm_t *ctx;     /**< NULL = detached                          */
    int32_t id_e;      /**< "<prefix>.e"    — E-L discriminator      */
    int32_t id_rate;   /**< "<prefix>.rate" — tracked code rate      */
    int32_t id_lock;   /**< "<prefix>.lock" — CFAR lock statistic R  */
    int32_t _pad;
} dll_tlm_t;

/**
 * @brief DLL state.
 *
 * Allocate with dll_create() (copies the code), or embed by value and
 * dll_init() (borrows the caller's code). The loop filter `lf` is a public
 * sub-component so the inline composition helpers can drive it; treat the
 * correlator accumulators and code-phase fields as internal.
 */
typedef struct {
    loop_filter_state_t lf;  /**< 2nd-order code PI loop.                  */
    const uint8_t *code;     /**< spreading code, one period (0/1 chips).  */
    size_t sf;               /**< code length (chips per period).          */
    size_t sps;              /**< samples per chip.                        */
    double inv_sps;          /**< 1 / sps (per-sample chip advance scale).  */
    double spacing;          /**< early/late tap offset, chips (e.g. 0.5).  */
    double chip_pos;         /**< current prompt code phase, chips.        */
    double code_rate;        /**< chips advanced per nominal chip (~1.0).  */
    double seed_chip;        /**< create-time code phase, for reset.       */
    double bn;               /**< loop noise bandwidth (retained).         */
    double zeta;             /**< damping factor (retained).               */
    float complex acc_e;     /**< early correlator accumulator.            */
    float complex acc_p;     /**< prompt correlator accumulator.           */
    float complex acc_l;     /**< late correlator accumulator.             */
    double last_error;       /**< last discriminator output (loop stress). */
    size_t segments;         /**< partial correlations per epoch (1 = full). */
    double seg_chips;        /**< code phase per partial segment = sf/segments.*/
    double seg_norm;         /**< nominal samples per segment (prompt scale). */
    size_t seg_idx;          /**< current partial index within the epoch.   */
    double sum_e;            /**< non-coherent early sum over the epoch.     */
    double sum_l;            /**< non-coherent late sum over the epoch.      */
    /* ── lock detector (always on): offset-tap CFAR noise ref + N-look test  */
    float complex acc_o;     /**< offset (noise) correlator accumulator.     */
    double off_chips;        /**< this look's offset code phase, whole chips. */
    double noise_guard;      /**< chips around P/E/L the offset must avoid.   */
    uint32_t rng;            /**< xorshift32 state for the random offset.    */
    double noise_ema;        /**< EMA of offset power; estimates E|O|^2.      */
    double lock_alpha;       /**< EMA coefficient 1/L_eff (L_eff >> n_looks). */
    double lock_sum;         /**< running sum|P_k|^2 over the current window. */
    size_t lock_count;       /**< looks accumulated in the current window.    */
    size_t n_looks;          /**< non-coherent integration depth N.          */
    double lock_stat;        /**< last statistic R = sqrt(2 sum|P|^2/E|O|^2). */
    size_t lock_nz;          /**< noise looks folded in (cumulative-mean boot).*/
    lockdet_state_t lock;    /**< decision rule: thresholds + verify counters
                                  stepped on R at each N-look decision.       */
    int owns_code;           /**< 1 if dll_destroy() frees `code`.         */
    dll_tlm_t tlm;           /**< live telemetry attachment; zeroed in blobs */
} dll_state_t;

/** 0/1 chip -> +1/-1 BPSK sign. */
JM_FORCEINLINE float
dll_chip_sign(uint8_t c)
{
    return (c & 1u) ? -1.0f : 1.0f;
}

/**
 * @brief Sub-chip code replica at fractional code phase @p c (one tap).
 *
 * Evaluates the ±1 code at chip phase @p c over the chip-phase extent
 * `[c, c + adv)` swept by one input sample (`adv = code_rate / sps`). Away from
 * a chip transition this is just `sign(code[floor(c)])`. When the sample's
 * extent straddles the boundary into the next chip, it returns the
 * overlap-weighted blend of the two chip signs — `frac` of the sample lies in
 * chip `floor(c)`, `1 - frac` in the next chip. Because the chips are ±1 and
 * constant away from a transition, this is the *exact* matched-filter integral
 * over a window whose edge falls at a fractional sample position: it makes the
 * correlation (hence the E-L discriminator) vary continuously with sub-sample
 * code phase instead of stepping in integer-sample quanta, with no loss of
 * despread SNR (the chip interior is still fully integrated). The blend is the
 * linear (trapezoidal) interpolant of the correlation; for ±1 chips no signal
 * interpolation is needed — only the lone straddling sample is reweighted.
 *
 * @param s    DLL state (for the code and period length).
 * @param c    Code phase of the tap, chips, in [0, sf).
 * @param adv  Chip-phase advance per input sample (`code_rate / sps`).
 * @return Blended ±1 replica value for this tap and sample.
 */
JM_FORCEINLINE float
dll_replica(const dll_state_t *s, double c, double adv)
{
    size_t i = (size_t)c;
    if (i >= s->sf)
        i = s->sf - 1;
    /* Distance from this sample to the next chip boundary. When it is at least
       one sample (adv), no transition falls inside the sample and the replica
       is a clean single chip sign — the common path, no divide. */
    double rem = (double)(i + 1) - c;
    if (rem >= adv)
        return dll_chip_sign(s->code[i]);
    /* Rare: the sample straddles the boundary; blend by the in-chip fraction. */
    double frac = rem / adv;
    size_t j = (i + 1 >= s->sf) ? 0 : i + 1; /* next chip, wraps the period */
    return (float)(frac * dll_chip_sign(s->code[i])
                   + (1.0 - frac) * dll_chip_sign(s->code[j]));
}

/**
 * @brief Initialise a DLL in place (no allocation); BORROWS @p code.
 *
 * The by-value counterpart to dll_create(): a tracking channel that embeds a
 * dll_state_t initialises it here and retains ownership of @p code (it is not
 * copied or freed). @p code must hold @p code_len chips for the loop's lifetime.
 *
 * @param s          State to initialise.  Must be non-NULL.
 * @param code       Spreading code (0/1 chips), one period; borrowed.
 * @param code_len   Code length (chips per period); must be >= 1.
 * @param sps        Samples per chip.
 * @param init_chip  Seed code phase, chips.
 * @param bn         Loop noise bandwidth, normalised to the code-period rate.
 * @param zeta       Damping factor (0.707 = critically damped).
 * @param spacing    Early/late tap offset, chips (0.5 = half-chip).
 */
void dll_init(dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
              double init_chip, double bn, double zeta, double spacing);

/**
 * @brief Per-sample early/prompt/late correlate + code-phase advance.
 *
 * Correlates the carrier-wiped sample @p d against the early, prompt and late
 * code taps (wrapped over the periodic code) and advances the code phase by
 * `code_rate / sps` chips. Inline, zero call overhead.
 *
 * @param s  DLL state.  Must be non-NULL.
 * @param d  One carrier-wiped input sample.
 */
JM_FORCEINLINE JM_HOT void
dll_accumulate(dll_state_t *s, float complex d)
{
    double adv = s->code_rate * s->inv_sps;
    double cp = s->chip_pos;
    double sfd = (double)s->sf;
    double ce = cp + s->spacing;
    if (ce >= sfd)
        ce -= sfd;
    double cl = cp - s->spacing;
    if (cl < 0.0)
        cl += sfd;
    /* Fractional-boundary integrate-and-dump: each tap's replica blends across a
       chip transition that falls inside the sample (dll_replica), so the E/P/L
       correlations vary continuously with sub-sample code phase. */
    s->acc_p += d * dll_replica(s, cp, adv);
    s->acc_e += d * dll_replica(s, ce, adv);
    s->acc_l += d * dll_replica(s, cl, adv);
    s->chip_pos += adv;
}

/**
 * @brief Per-period code discriminator + loop update + phase wrap.
 *
 * Runs the non-coherent early-minus-late envelope discriminator on the dumped
 * accumulators, filters it, updates the code rate, and wraps the prompt phase
 * to the next period (plus a proportional phase nudge). Call at a period
 * boundary after reading the prompt; the caller resets the accumulators. Inline.
 *
 * @param s  DLL state.  Must be non-NULL.
 */
JM_FORCEINLINE JM_HOT void
dll_update(dll_state_t *s)
{
    float me = cabsf(s->acc_e), ml = cabsf(s->acc_l);
    double e = (double)(me - ml) / ((double)(me + ml) + DLL_EPS);
    s->last_error = e;
    loop_filter_step(&s->lf, e);
    s->code_rate = 1.0 + s->lf.integ;
    s->chip_pos -= (double)s->sf;
    s->chip_pos += s->lf.kp * e; /* proportional phase nudge, chips */
}

/**
 * @brief Create a DLL instance (COPIES @p code).
 *
 * @param code       Spreading code (0/1 chips), one period; copied internally.
 * @param code_len   Code length (chips per period).
 * @param sps        Samples per chip (default 2).
 * @param init_chip  Seed code phase, chips (default 0.0).
 * @param bn         Loop noise bandwidth (default 0.01).
 * @param zeta       Damping factor (default 0.707).
 * @param spacing    Early/late tap offset, chips (default 0.5).
 * @param segments   Partial correlations per code epoch (default 1). 1 = a
 *                   coherent full-epoch integrate-and-dump (one prompt/period).
 *                   >1 splits each epoch into that many sub-epoch partials: it
 *                   emits that many partial prompts/period and tracks the code
 *                   non-coherently across them (robust to an asynchronous
 *                   data-symbol clock). segments/epoch ~ samples/symbol at a
 *                   downstream SymbolSync when the symbol rate is near the code
 *                   rate, so choose >= 2 for symbol-timing recovery.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call dll_destroy() when done.
 */
dll_state_t *dll_create(const uint8_t *code, size_t code_len, size_t sps, double init_chip, double bn, double zeta, double spacing, size_t segments);

/**
 * @brief Destroy a DLL instance and release all memory (incl. the code copy).
 * @param state  May be NULL.
 */
void dll_destroy(dll_state_t *state);

/**
 * @brief Re-seed the loop to its create-time code phase; keep config.
 * @param state  Must be non-NULL.
 */
void dll_reset(dll_state_t *state);

size_t dll_steps_max_out(dll_state_t *state);
size_t dll_steps(dll_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void dll_configure(dll_state_t *state, double bn, double zeta);
double dll_get_bn(const dll_state_t *state);
void dll_set_bn(dll_state_t *state, double val);
double dll_get_code_phase(const dll_state_t *state);
double dll_get_code_rate(const dll_state_t *state);
double dll_get_last_error(const dll_state_t *state);
size_t dll_get_segments(const dll_state_t *state);

/**
 * @brief Tune the always-on code-lock detector to a target (pfa, n_looks).
 *
 * The DLL carries a lock detector that reuses acquisition's non-coherent test
 * statistic. Every emitted look (a partial in segments mode, or the full-epoch
 * prompt when segments == 1) is also correlated at a *random off-peak* code
 * phase — re-drawn each epoch and kept `noise_guard` chips clear of the
 * prompt/early/late lobe — to give a signal-free CFAR noise sample (valid for a
 * low-sidelobe code, e.g. Gold). The offset power feeds an EMA reference
 * `E|O|^2`; the prompt powers of @p n_looks consecutive looks are summed into
 * `S = sum|P_k|^2`, and the detector declares lock when
 *
 *   R = sqrt(2 * S / E|O|^2)  >  det_threshold_noncoherent(pfa, n_looks)
 *
 * which under H0 has `P(R > eta) = marcum_q(n_looks, 0, eta)`. Size
 * @p n_looks with det_n_noncoh(snr, ...) for the operating C/N0.
 *
 * The noise-reference EMA bandwidth is sized probabilistically via
 * det_ema_alpha(): the signal-free `|O|^2` samples are exponential (0 dB
 * estimator SNR per sample — a DC level in fluctuation of equal power), and
 * @p ref_snr_db chooses the EMA output's estimator SNR (mean^2/variance).
 * Passing 0 derives it from @p n_looks: the reference's relative std is held
 * to an eighth of the statistic's intrinsic H0 spread (`1/sqrt(N)`), floored
 * at ~33 dB — which reproduces the classic `1/alpha = max(1024, 32*N)`
 * sizing exactly, now as a consequence instead of a constant.
 *
 * The decision itself runs through an embedded lock detector
 * (lockdet_core.h) rather than a single-comparison latch: `locked` flips up
 * only after det_verify_count(pfa, pfa*1e-3) CONSECUTIVE above-threshold
 * decisions (the false-declare budget held three decades under the
 * per-decision @p pfa — 2 straight for the default 1e-3), and drops only
 * after 2 straight below-threshold decisions, so a statistic grazing the
 * threshold cannot chatter the flag. Full control of the verify counts and
 * a split declare/drop threshold pair is C-only via
 * dll_configure_lock_raw().
 *
 * @param state       DLL state.  Must be non-NULL.
 * @param pfa         Per-decision false-alarm probability, in (0, 1).
 * @param n_looks     Non-coherent integration depth N (looks); clamped >= 1.
 * @param ref_snr_db  Noise-reference estimator SNR in dB (> 0), or 0 to
 *                    derive from @p n_looks as above.
 * @return DP_OK, or DP_ERR_INVALID when @p pfa is outside (0, 1).
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import Dll
 * >>> d = Dll(code=np.zeros(31, dtype=np.uint8), sps=2)
 * >>> d.configure_lock(1e-3, 20)
 * >>> d.locked
 * False
 * >>> d.configure_lock(1e-3, 20, ref_snr_db=20.0)   # ~50-look reference
 * >>> d.configure_lock(2.0, 20)
 * Traceback (most recent call last):
 *     ...
 * ValueError: configure_lock failed (rc=-4)
 *
 * @endcode
 */
int dll_configure_lock(dll_state_t *state, double pfa, size_t n_looks, double ref_snr_db);

/**
 * @brief Set the lock detector's raw geometry directly.
 *
 * The escape hatch under dll_configure_lock() for a composing C caller that
 * derives its own threshold/EMA/hysteresis geometry — the full lockdet
 * decision rule is exposed: a split declare/drop threshold pair (level
 * hysteresis) and both verify counts (time hysteresis; size them with
 * det_verify_count()). Re-tuning clears the in-flight statistic and drops
 * the lock so the next decision uses only looks gathered under the new
 * config.
 *
 * @param state        DLL state.  Must be non-NULL.
 * @param up_thresh    Declare threshold on the statistic R (e.g. the CFAR
 *                     eta from det_threshold_noncoherent()).
 * @param down_thresh  Drop threshold on R; choose <= up_thresh for level
 *                     hysteresis.
 * @param n_looks      Non-coherent integration depth N (looks); clamped >= 1.
 * @param alpha        EMA coefficient for the noise reference, in (0, 1].
 * @param n_up         Consecutive above-threshold decisions to declare
 *                     lock; clamped to >= 1.
 * @param n_down       Consecutive below-threshold decisions to drop it;
 *                     clamped to >= 1.
 */
void dll_configure_lock_raw(dll_state_t *state, double up_thresh,
                            double down_thresh, size_t n_looks, double alpha,
                            uint32_t n_up, uint32_t n_down);

/** @brief Current lock decision (1 = locked, 0 = not), with the configured
 *         verify-count / hysteresis rule applied (see dll_configure_lock). */
int dll_get_locked(const dll_state_t *state);

/** @brief Last lock test statistic R (compare against the configured eta). */
double dll_get_lock_stat(const dll_state_t *state);

/** @brief Current CFAR noise-power estimate E|O|^2 (offset-tap EMA). */
double dll_get_noise_est(const dll_state_t *state);

/**
 * @brief Emit the code loop's telemetry records for the epoch just closed.
 *
 * Out-of-line on purpose: the emit machinery must not inline into the
 * per-sample correlator loop (inlined ring-write expansions bloat the
 * loop body and an extern call site forces per-iteration state reloads —
 * both measured ~20% slower detached on other loops). Callers gate on
 * `s->tlm.ctx` and call this once per code-epoch update. Records
 * "<prefix>.e" (the E-L envelope discriminator — the loop stress),
 * "<prefix>.rate" (the tracked code rate, chips per nominal chip) and
 * "<prefix>.lock" (the CFAR lock statistic R, refreshed every n_looks
 * looks). A composing tracking channel (the DSSS despreader) calls this
 * from its own per-epoch update.
 *
 * @param s  State with a non-NULL tlm.ctx (caller-checked).
 */
void dll_tlm_flush(const dll_state_t *s);

/**
 * @brief Attach (or detach) a telemetry context and register the code
 * loop's probes on it.
 * Registers three probes, emitted once per code epoch (period) and
 * further thinned by decim: "<prefix>.e" (the early-minus-late envelope
 * discriminator — the loop stress), "<prefix>.rate" (the tracked code
 * rate, chips advanced per nominal chip, ~1.0 at lock) and
 * "<prefix>.lock" (the CFAR lock statistic R; compare against the
 * configured threshold).  Passing NULL detaches.  Setup path, never hot:
 * call before the producer thread starts stepping; the context is
 * borrowed and must outlive the attachment (SPSC rules in
 * telemetry/telemetry.h).
 * @param state  Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "code" or "ch0.code".
 * @param decim  Emit every decim-th epoch; >= 1.
 * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
 *         three probes (the attach fails whole; the object stays
 *         detached).
 * @code
 * >>> import numpy as np
 * >>> from doppler.track import Dll
 * >>> from doppler.telemetry import Telemetry
 * >>> tlm = Telemetry(1 << 12)
 * >>> code = np.zeros(31, dtype=np.uint8)
 * >>> d = Dll(code=code, sps=2)
 * >>> d.set_telemetry(tlm, "code")
 * >>> sorted(tlm.probe_names())
 * ['code.e', 'code.lock', 'code.rate']
 * >>> x = np.ones(31 * 2 * 50, dtype=np.complex64)
 * >>> _ = d.steps(x)
 * >>> recs = tlm.read()   # three records per code epoch
 * >>> len(recs) > 0 and len(recs) % 3 == 0
 * True
 *
 * @endcode
 */
int dll_set_telemetry(dll_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition+field-wise: loop_filter child (POD-embedded) + running
 * correlators/loop/lock state; borrowed `code` pointer restored by create. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 3u /* v3: lockdet decision rule (verify counters) */
size_t dll_state_bytes (const dll_state_t *state);
void dll_get_state (const dll_state_t *state, void *blob);
int dll_set_state (dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
