/**
 * @file dll_core.h
 * @brief Delay-lock loop (DLL) — non-coherent early/prompt/late code tracking.
 *
 * Tracks the code phase of a continuous, repeating spreading code (e.g. a PN /
 * Gold sequence) on a *carrier-wiped* sample stream. Per sample it correlates
 * the input against three taps of a 2-samples/chip interpolated local-code
 * replica — early (`+spacing` chips), prompt, late (`-spacing` chips) —
 * accumulating an integrate-and-dump over one code period; per period it runs
 * the power-domain non-coherent early-minus-late discriminator
 * `0.5 * (|E|^2 - |L|^2) / |P|^2`, filters it through an embedded 2nd-order
 * @ref loop_filter_state_t, and steers an embedded fixed-point NCO
 * (@ref nco_state_t) that tracks the code phase — a 32-bit phase accumulator,
 * exact integer wraparound, no open-ended floating-point drift.
 *
 * It pairs with the carrier loop (costas_core.h): the carrier loop wipes the
 * carrier, the DLL wipes the code. The block API (dll_steps) is the Python face;
 * the
 * JM_FORCEINLINE dll_accumulate()/dll_update() are the C composition API a
 * tracking channel inlines into its own sample loop.
 *
 * Lifecycle: `dll_create -> (steps / configure / reset)* -> dll_destroy`, or
 * embed by value with dll_init() (which BORROWS the caller-owned code, and
 * always runs with `segments == 1` — there is no by-value counterpart to
 * `dll_create()`'s `segments` parameter).
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
#include "nco/nco_core.h"
#include "telemetry/telemetry.h"
#include <complex.h>
#include <math.h>
#include "detection/detection_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Margin the segments>1 lookback search must beat the natural (unshifted)
 * window's power by before preferring a shifted, previous-epoch-combined
 * candidate -- an amplitude-domain ratio, ~0.5 dB (10**(0.5/20)), the
 * reference design's own example async-correlation-loss budget. Without
 * this margin, comparing two epochs' windows under a still-converging
 * (never exactly 1.0) code_rate destabilizes the loop on pure numerical
 * noise even with zero real data transitions -- validated empirically in
 * the Python prototype across a 0.1-3.0 dB sweep. */
#define DLL_LOOKBACK_MARGIN 1.06

/* Numerical guard on the early+late envelope sum (not tunable). */
#define DLL_EPS 1e-12

/* Clamp on the discriminator output |e| before it reaches the loop filter.
 * The old magnitude-domain discriminator (|E|-|L|)/(|E|+|L|) was always
 * inherently bounded in [-1, 1] (|E|-|L| <= |E|+|L| identically); the
 * power-domain 0.5*(Ep-Lp)/Pp form is NOT -- a data transition landing
 * badly enough to collapse the prompt power Pp near zero (the lookback
 * failing to find a clean candidate that epoch, a real, reachable case,
 * not a hypothetical one -- observed directly while porting this design)
 * can blow e up arbitrarily, injecting a huge phase nudge that cascades
 * into a runaway. Clamping restores the old design's inherent safety
 * property without changing behaviour in the overwhelming common case
 * (|e| well under 1 at any reasonable lock). */
#define DLL_DISC_CLAMP 1.0

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
    int32_t id_locked; /**< "<prefix>.locked" — lockdet decision 0/1 */
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
    nco_state_t code_nco;    /**< fixed-point code-phase NCO (phase/phase_inc).*/
    const uint8_t *code;     /**< spreading code, one period (0/1 chips).  */
    size_t sf;               /**< code length (chips per period).          */
    size_t sps;              /**< samples per chip.                        */
    double inv_sps;          /**< 1 / sps (per-sample chip advance scale).  */
    double spacing;          /**< early/late tap offset, chips (e.g. 0.5).  */
    double chip_pos;         /**< current prompt code phase, chips; DERIVED
                                  from code_nco.phase on every dll_accumulate,
                                  never independently accumulated.          */
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
    size_t seg_idx;          /**< samples integrated into the current chunk.*/
    /* ── segments>1 chunked output + one-epoch-deep lookback (heap-owned,
     *    length `segments`; NULL when segments==1 -- dll_init()'s embedded/
     *    borrowed path is always segments==1, so this never needs a
     *    deinit contract there, same lifecycle class as `code`/owns_code). */
    float complex *chunk_p;         /**< this epoch's per-chunk prompt sums. */
    float complex *chunk_e;         /**< this epoch's per-chunk early sums.  */
    float complex *chunk_l;         /**< this epoch's per-chunk late sums.   */
    float complex *last_backward_p; /**< prev epoch's reversed-cumsum prompt.*/
    float complex *last_e;          /**< prev epoch's per-chunk early sums.  */
    float complex *last_l;          /**< prev epoch's per-chunk late sums.   */
    int have_prev_epoch;     /**< 0 until one full epoch has completed.    */
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
 * The code is treated as held at a fixed 2 samples/chip, sampled at
 * chip-relative positions {0.25, 0.75} (a quarter and three-quarters into
 * each chip) and linearly interpolated at any fractional position — NOT
 * at {0, 0.5}, which would confine the whole linear-interpolation
 * transition zone to one side of each chip boundary (found during the
 * initial port: a real bug, not a cosmetic asymmetry — it broke E/L
 * symmetry around P badly enough to leave a large discriminator offset
 * at perfect lock). Quarter/three-quarter placement centers the
 * transition zone symmetrically on each boundary (spanning
 * `[chip-0.25, chip+0.25)`), and both quarter-chip flat regions are
 * pure `sign(code[chip])` (no table is materialised — the 2x-oversampled
 * index converts straight back to a chip index via `>> 1`, same as an
 * unshifted grid would, only the phase origin moves). A point-sample
 * interpolation, not a dwell-width-aware blend: this replaces the earlier
 * exact matched-filter integral (which varied its blend width with the
 * sample's chip-phase dwell time) with the simpler, validated design from
 * `docs/design/async-despreader-working-design.md` — the dwell-integral
 * model was mathematically fancier but did not, on its own, fix the
 * long-run false-lock that motivated this redesign; this one does.
 *
 * @param s  DLL state (for the code and period length).
 * @param c  Code phase of the tap, chips (any real value; wrapped mod sf).
 * @return Linearly-interpolated ±1 replica value for this tap and sample.
 */
JM_FORCEINLINE float
dll_replica(const dll_state_t *s, double c)
{
    double sfd2 = 2.0 * (double)s->sf;
    double p = fmod(c * 2.0 - 0.5, sfd2);
    if (p < 0.0)
        p += sfd2;
    size_t i = (size_t)p;
    double mu = p - (double)i;
    size_t j = (i + 1 >= (size_t)sfd2) ? 0 : i + 1;
    float v0 = dll_chip_sign(s->code[i >> 1]);
    float v1 = dll_chip_sign(s->code[j >> 1]);
    return (float)((1.0 - mu) * v0 + mu * v1);
}

/**
 * @brief Floor-normalize @p cycles into [0, 1) then scale to a u32 phase
 * delta, `llround`-not-truncated. Mirrors the already-hardened
 * `lo_core.c` `norm_to_inc()` pattern: floor-normalizing before the cast
 * (rather than truncating a possibly-negative double directly to
 * uint32_t) avoids undefined behaviour and gives the correct modular
 * wraparound for a signed phase nudge (a small negative `cycles` maps to
 * a large positive delta that, added mod 2^32, is equivalent to
 * subtracting the small magnitude).
 *
 * @param cycles  Any real number of cycles (code periods); only the
 *                fractional part matters.
 * @return Phase delta in [0, 2^32).
 */
JM_FORCEINLINE uint32_t
dll_cycles_to_phase_delta(double cycles)
{
    double d = cycles - floor(cycles);
    return (uint32_t)llround(d * 4294967296.0);
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
 * @brief This sample's dwell-CENTER code phase, chips.
 *
 * A received sample is a zero-order hold over its dwell interval
 * `[chip_pos, chip_pos + step)`, `step` = one `phase_inc` in chip units
 * (~1/sps). Evaluating the replica taps at the dwell's START (the raw
 * pre-advance `chip_pos`) treats every sample as landing at the very
 * first instant of its hold interval rather than at the interval's
 * continuous-time representative point — this biases the correlation by
 * half a sample's worth of chip phase (0.5/sps chips). Found via a
 * direct symmetry check: the autocorrelation of a real signal must
 * satisfy R(tau) = R(-tau), and the S-curve didn't — it was offset from
 * tau=0 by exactly 0.5/sps, vanishing when taps are evaluated at the
 * dwell midpoint instead. Advancing by half of `phase_inc` before
 * deriving the chip position gives that midpoint.
 *
 * @param s  DLL state.
 * @return   Dwell-center code phase, chips.
 */
JM_FORCEINLINE double
dll_dwell_center_chip_pos(const dll_state_t *s)
{
    uint32_t mid = s->code_nco.phase + (s->code_nco.phase_inc >> 1);
    return ((double)mid / 4294967296.0) * (double)s->sf;
}

/**
 * @brief Per-sample early/prompt/late correlate + fixed-point code-phase
 * advance.
 *
 * Correlates the carrier-wiped sample @p d against the early, prompt and late
 * code taps at this sample's dwell-CENTER phase (@ref
 * dll_dwell_center_chip_pos, wrapped over the periodic code), then advances
 * the embedded fixed-point NCO by one sample and re-derives `chip_pos` from
 * its POST-advance (dwell-START-of-next-sample) phase (never independently
 * accumulated — see @ref dll_state_t::chip_pos). Inline, zero call overhead.
 *
 * @param s  DLL state.  Must be non-NULL.
 * @param d  One carrier-wiped input sample.
 * @return 1 if this sample's advance wrapped the code period (a period
 *         boundary), 0 otherwise. A plain return value, not a persistent
 *         struct field — a stored wrap flag that a caller forgets to
 *         consume is exactly the class of bug an earlier attempt at this
 *         redesign hit (a stale flag causing an infinite loop under
 *         segments>1 stress); a local value cannot go stale.
 */
JM_FORCEINLINE JM_HOT int
dll_accumulate(dll_state_t *s, float complex d)
{
    double sfd = (double)s->sf;
    double cp = dll_dwell_center_chip_pos(s);
    double ce = cp + s->spacing;
    if (ce >= sfd)
        ce -= sfd;
    double cl = cp - s->spacing;
    if (cl < 0.0)
        cl += sfd;
    s->acc_p += d * dll_replica(s, cp);
    s->acc_e += d * dll_replica(s, ce);
    s->acc_l += d * dll_replica(s, cl);
    uint32_t old_phase = s->code_nco.phase;
    uint64_t sum = (uint64_t)old_phase + (uint64_t)s->code_nco.phase_inc;
    s->code_nco.phase = (uint32_t)sum;
    s->chip_pos = ((double)s->code_nco.phase / 4294967296.0) * sfd;
    return (sum >> 32) != 0;
}

/**
 * @brief Per-sample offset (noise) tap for the always-on lock detector.
 *
 * The composition sibling of dll_accumulate(): correlates the input against
 * the code at this epoch's random off-peak offset, feeding the CFAR noise
 * reference. Call it on the same sample stream as dll_accumulate() and
 * BEFORE it (both taps evaluate this sample's dwell-CENTER chip phase, @ref
 * dll_dwell_center_chip_pos — dll_accumulate() hasn't advanced the NCO yet,
 * so the two calls see the same phase). A composer that skips this (and
 * dll_lock_look()/dll_lock_epoch()) simply leaves the lock detector idle —
 * locked stays 0, lock_stat/noise_est stay 0.
 *
 * @param s  DLL state.  Must be non-NULL.
 * @param d  One carrier-wiped input sample (same sample as dll_accumulate).
 */
JM_FORCEINLINE JM_HOT void
dll_lock_accumulate(dll_state_t *s, float complex d)
{
    double co = dll_dwell_center_chip_pos(s) + s->off_chips;
    if (co >= (double)s->sf)
        co -= (double)s->sf;
    s->acc_o += d * dll_replica(s, co);
}

/**
 * @brief Fold one look into the lock detector; clear the offset tap.
 *
 * Normalises the prompt and offset accumulators by @p norm (the number of
 * samples integrated into them — one full period for a full-epoch composer),
 * folds the offset power into the CFAR noise reference and the prompt power
 * into the running N-look sum, and — at every n_looks-th look — forms the
 * statistic R and steps the verify-counted lock detector. Call at each look
 * boundary BEFORE zeroing the correlator accumulators (it reads acc_p and
 * acc_o; acc_o is cleared here). Out of line: per-look rate, never hot.
 *
 * @param s     DLL state.  Must be non-NULL.
 * @param norm  Samples integrated into acc_p/acc_o this look (> 0).
 */
void dll_lock_look(dll_state_t *s, double norm);

/**
 * @brief Per-epoch lock-detector housekeeping: re-draw the noise offset.
 *
 * Call once per code epoch (after the period's dll_lock_look()) so the next
 * epoch's noise tap lands at a fresh random off-peak code phase.
 *
 * @param s  DLL state.  Must be non-NULL.
 */
void dll_lock_epoch(dll_state_t *s);

/**
 * @brief Per-period code discriminator + loop update + NCO steer.
 *
 * Runs the power-domain non-coherent early-minus-late discriminator
 * `0.5 * (|E|^2 - |L|^2) / |P|^2` on the dumped accumulators (the prompt
 * power is the normalizing "signal + noise power" reference — the
 * validated design from `docs/design/async-despreader-working-design.md`,
 * not a magnitude-domain `(|E|-|L|)/(|E|+|L|)` ratio), filters it, and
 * steers `phase_inc` (sample-and-hold — held constant until the next
 * call, exactly one epoch later) from BOTH the integrator (`code_rate`,
 * a sustained rate) and the proportional term, spread smoothly across
 * the whole next period rather than kicked directly into `phase`. Two
 * things were tried and rejected while porting this design: (1) folding
 * the loop filter's full combined control (`integ + kp*e`) into
 * `phase_inc` as a single rate (mirroring `symsync_core.h`) massively
 * over-corrects — a rate held for a whole `sf*sps`-sample period turns a
 * `kp*e`-chip correction into a `kp*e*sf`-chip one, unstable at any
 * non-tiny `bn`; (2) kicking `phase` directly once per period (mirroring
 * `costas_core.c`) is unsafe right after a wrap (when `phase` is near
 * zero) — a negative kick pushes phase backward across the just-crossed
 * boundary, and the very next sample's forward step re-crosses it,
 * registering a second, spurious wrap. Spreading the *same total*
 * `kp*e`-chip correction over the next period's `phase_inc` (not
 * `phase` directly) reproduces the original double-accumulator design's
 * `chip_pos += kp*e` exactly, without either failure mode. The period
 * wrap itself is NOT handled here — it falls out of dll_accumulate()'s
 * own NCO advance (which wraps mod 2^32 on its own); call this at a
 * period boundary (dll_accumulate() returned 1) after reading the
 * prompt, then the caller resets the accumulators. Inline.
 *
 * @param s  DLL state.  Must be non-NULL.
 */
JM_FORCEINLINE JM_HOT void
dll_update(dll_state_t *s)
{
    float me = cabsf(s->acc_e), ml = cabsf(s->acc_l), mp = cabsf(s->acc_p);
    double ep = (double)me * me, lp = (double)ml * ml, pp = (double)mp * mp;
    double e = 0.5 * (ep - lp) / (pp + DLL_EPS);
    if (e > DLL_DISC_CLAMP)
        e = DLL_DISC_CLAMP;
    else if (e < -DLL_DISC_CLAMP)
        e = -DLL_DISC_CLAMP;
    s->last_error = e;
    loop_filter_step(&s->lf, e);
    s->code_rate = 1.0 + s->lf.integ;
    /* Rate from the integrator alone, PLUS the proportional term spread
       smoothly over the whole next period rather than kicked directly
       into `phase` (see the comment above): kp*e chips of total
       correction over sf*sps samples is kp*e/(sf*sf*sps) extra cycles
       per sample -- the same total chip-domain correction the original
       double-accumulator design applied as `chip_pos += kp*e`, just
       distributed through the rate instead of a discrete phase jump. */
    s->code_nco.phase_inc = dll_cycles_to_phase_delta(
        s->code_rate / ((double)s->sf * (double)s->sps)
        + s->lf.kp * e
              / ((double)s->sf * (double)s->sf * (double)s->sps));
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
 * The detector needs an off-peak code phase to sample noise from: with a
 * very short code (fewer than ~2*(spacing+2)+1 chips, i.e. sf <= 6 at the
 * default spacing) no offset clears the prompt/early/late lobe, the noise
 * tap aliases the prompt, and the statistic pins below threshold — locked
 * stays 0 (fail-closed) no matter the signal. Use a code of >= 7 chips
 * (real spreading codes are far longer) for a meaningful lock decision.
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
 * "<prefix>.rate" (the tracked code rate, chips per nominal chip),
 * "<prefix>.lock" (the CFAR lock statistic R, refreshed every n_looks
 * looks) and "<prefix>.locked" (the verify-counted lockdet decision,
 * 0/1 — plotted against .lock it shows exactly where the declare/drop
 * rule fired). A composing tracking channel (the DSSS despreader) calls
 * this from its own per-epoch update.
 *
 * @param s  State with a non-NULL tlm.ctx (caller-checked).
 */
void dll_tlm_flush(const dll_state_t *s);

/**
 * @brief Attach (or detach) a telemetry context and register the code
 * loop's probes on it.
 * Registers four probes, emitted once per code epoch (period) and
 * further thinned by decim: "<prefix>.e" (the early-minus-late envelope
 * discriminator — the loop stress), "<prefix>.rate" (the tracked code
 * rate, chips advanced per nominal chip, ~1.0 at lock), "<prefix>.lock"
 * (the CFAR lock statistic R; compare against the configured threshold)
 * and "<prefix>.locked" (the verify-counted lock decision, 0/1 — the
 * lockdet output, so a consumer sees where the declare/drop rule fired
 * without re-deriving it from the statistic).  Passing NULL detaches.
 * Setup path, never hot: call before the producer thread starts
 * stepping; the context is borrowed and must outlive the attachment
 * (SPSC rules in telemetry/telemetry.h).
 * @param state  Must be non-NULL.
 * @param tlm    Telemetry context to attach, or NULL to detach.
 * @param prefix Probe-name prefix, e.g. "code" or "ch0.code".
 * @param decim  Emit every decim-th epoch; >= 1.
 * @return DP_OK, or DP_ERR_INVALID when the probe table cannot take all
 *         four probes (the attach fails whole; the object stays
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
 * ['code.e', 'code.lock', 'code.locked', 'code.rate']
 * >>> x = np.ones(31 * 2 * 50, dtype=np.complex64)
 * >>> _ = d.steps(x)
 * >>> recs = tlm.read()   # four records per code epoch
 * >>> len(recs) > 0 and len(recs) % 4 == 0
 * True
 *
 * @endcode
 */
int dll_set_telemetry(dll_state_t *state, dp_tlm_t * tlm, const char * prefix, uint32_t decim);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * composition+field-wise: loop_filter child (POD-embedded) + embedded NCO
 * (POD) + running correlators/loop/lock state; borrowed `code` pointer
 * restored by create; the segments>1 chunk/lookback buffers (heap-owned,
 * pointers, NOT part of the whole-struct snapshot) are packed/restored
 * field-wise when segments > 1. */
#define DLL_STATE_MAGIC DP_FOURCC ('D','L','L',' ')
#define DLL_STATE_VERSION 4u /* v4: fixed-point code_nco + segments>1 chunked
                                lookback buffers (see dll_core.c) */
size_t dll_state_bytes (const dll_state_t *state);
void dll_get_state (const dll_state_t *state, void *blob);
int dll_set_state (dll_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
