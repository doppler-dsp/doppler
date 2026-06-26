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
 * It pairs with @ref costas_core.h: a carrier loop wipes the carrier, the DLL
 * wipes the code. The block API (dll_steps) is the Python face; the
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
#include "jm_perf.h"
#include "loop_filter/loop_filter_core.h"
#include <complex.h>
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
    int owns_code;           /**< 1 if dll_destroy() frees `code`.         */
} dll_state_t;

/** 0/1 chip -> +1/-1 BPSK sign. */
JM_FORCEINLINE float
dll_chip_sign(uint8_t c)
{
    return (c & 1u) ? -1.0f : 1.0f;
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
    double cp = s->chip_pos;
    double sfd = (double)s->sf;
    size_t pj = (size_t)cp;
    if (pj >= s->sf)
        pj = s->sf - 1;
    double ce = cp + s->spacing;
    if (ce >= sfd)
        ce -= sfd;
    double cl = cp - s->spacing;
    if (cl < 0.0)
        cl += sfd;
    size_t ej = (size_t)ce;
    size_t lj = (size_t)cl;
    if (ej >= s->sf)
        ej = s->sf - 1;
    if (lj >= s->sf)
        lj = s->sf - 1;
    s->acc_p += d * dll_chip_sign(s->code[pj]);
    s->acc_e += d * dll_chip_sign(s->code[ej]);
    s->acc_l += d * dll_chip_sign(s->code[lj]);
    s->chip_pos += s->code_rate * s->inv_sps;
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
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call dll_destroy() when done.
 */
dll_state_t *dll_create(const uint8_t *code, size_t code_len, size_t sps, double init_chip, double bn, double zeta, double spacing);

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
#ifdef __cplusplus
}
#endif

#endif /* DLL_CORE_H */
