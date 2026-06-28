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
#include "loop_filter/loop_filter_core.h"
#include <complex.h>
#include "detection/detection_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

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
    double lock_thresh;      /**< CFAR threshold eta on R (det_threshold_nc). */
    double lock_stat;        /**< last statistic R = sqrt(2 sum|P|^2/E|O|^2). */
    size_t lock_nz;          /**< noise looks folded in (cumulative-mean boot).*/
    int locked;              /**< last lock decision (R > eta).               */
    int owns_code;           /**< 1 if dll_destroy() frees `code`.         */
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
 * @brief Configure the always-on code-lock detector.
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
 *   R = sqrt(2 * S / E|O|^2)  >  @p threshold
 *
 * which under H0 has `P(R > threshold) = marcum_q(n_looks, 0, threshold)` — so a
 * caller sizes @p threshold = det_threshold_noncoherent(pfa, n_looks) and
 * @p n_looks = det_n_noncoh(snr, ...) to meet a target (Pfa, Pd). The threshold
 * is passed in (not derived) so the core stays dependency-free; the Python
 * binding converts a `pfa` via the detection module. The EMA must average many
 * more cells than the test integrates (`1/alpha >> n_looks`) or the noise
 * estimate's own variance inflates Pfa; the binding defaults `1/alpha` to
 * `max(1024, 32*n_looks)`.
 *
 * @param state      DLL state.  Must be non-NULL.
 * @param threshold  CFAR threshold eta on the statistic R.
 * @param n_looks    Non-coherent integration depth N (looks); clamped to >= 1.
 * @param alpha      EMA coefficient for the noise reference, in (0, 1].
 */
void dll_configure_lock(dll_state_t *state, double threshold, size_t n_looks,
                        double alpha);

/** @brief Last lock decision (1 = locked, 0 = not). */
int dll_get_locked(const dll_state_t *state);

/** @brief Last lock test statistic R (compare against the configured eta). */
double dll_get_lock_stat(const dll_state_t *state);

/** @brief Current CFAR noise-power estimate E|O|^2 (offset-tap EMA). */
double dll_get_noise_est(const dll_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition+field-wise: loop_filter child (POD-embedded) + running
 * correlators/loop/lock state; borrowed `code` pointer restored by create. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 1u
size_t dll_state_bytes (const dll_state_t *state);
void dll_get_state (const dll_state_t *state, void *blob);
int dll_set_state (dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
