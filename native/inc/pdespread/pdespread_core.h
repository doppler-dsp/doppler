/**
 * @file pdespread_core.h
 * @brief Partial-correlation despreader — sub-epoch despread + non-coherent
 *        code tracking for an *asynchronous* data-symbol clock.
 *
 * When the data-symbol rate is on the order of the code-epoch rate but
 * asynchronous to it, a coherent integrate-and-dump over one full code epoch
 * straddles data transitions and collapses (see
 * docs/design/async-symbol-despreader.md). This despreader instead splits each
 * code epoch into @c k sub-epoch **partial** correlations and:
 *
 *   1. emits the @c k partial prompts per epoch — a stream sampled at ~`k`
 *      samples per symbol, from which a downstream symbol loop (a boxcar symbol
 *      matched filter + @ref symsync_state_t) recovers the independent symbol
 *      clock; and
 *   2. tracks the code **non-coherently** — the early/late discriminator is
 *      `(sum_k|E_k| - sum_k|L_k|) / (sum_k|E_k| + sum_k|L_k|)`. A data flip
 *      changes a partial's *sign*, not its magnitude, so only the one straddling
 *      segment degrades; this keeps the code loop locked through the data the
 *      coherent-epoch discriminator could not survive.
 *
 * It composes the code loop by embedding a @ref dll_state_t by value: the DLL
 * supplies the integer/fractional code NCO, the fractional-boundary E/P/L
 * correlators (dll_accumulate), and the 2nd-order @ref loop_filter_state_t. The
 * input is carrier-wiped (a Costas loop, upstream, removes the carrier).
 *
 * Lifecycle: pdespread_create -> [steps / reset]* -> pdespread_destroy.
 *
 * @code
 * uint8_t code[127] = { ... };  // one code period, 0/1 chips
 * pdespread_state_t *p = pdespread_create(code, 127, 8, 4,    // k = 4 partials
 *                                         0.0, 0.002, 0.707, 0.5);
 * float complex part[256];
 * size_t k = pdespread_steps(p, rx, rx_len, part, 256);  // k partials / epoch
 * pdespread_destroy(p);
 * @endcode
 */
#ifndef PDESPREAD_CORE_H
#define PDESPREAD_CORE_H

#include "clib_common.h"
#include "dll/dll_core.h"
#include "jm_perf.h"
#include <complex.h>
#include "loop_filter/loop_filter_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Partial-correlation despreader state.
 *
 * Allocate with pdespread_create() (copies the code). Embeds the code loop
 * @c dll by value (it borrows the owned code copy). The per-epoch non-coherent
 * envelope sums and the segment index are internal carry.
 */
typedef struct {
    dll_state_t dll;     /**< embedded code loop (NCO + correlators + filter). */
    uint8_t *code_copy;  /**< owned spreading code; the DLL borrows it.        */
    size_t k;            /**< partial correlations per code epoch (>= 2).      */
    double seg_chips;    /**< code phase per partial segment = sf / k, chips.  */
    double seg_norm;     /**< nominal samples per segment, for prompt scaling. */
    size_t seg_idx;      /**< current partial index within the epoch, [0, k).  */
    double sum_e;        /**< non-coherent early envelope sum over the epoch.  */
    double sum_l;        /**< non-coherent late envelope sum over the epoch.   */
    double last_error;   /**< last non-coherent discriminator output.          */
} pdespread_state_t;

/**
 * @brief Create a partial-correlation despreader (COPIES @p code).
 *
 * @param code       Spreading code (0/1 chips), one period; copied internally.
 * @param code_len   Code length (chips per period); must be >= 1.
 * @param sps        Samples per chip.
 * @param k          Partial correlations per code epoch; must be >= 1.
 * @param init_chip  Seed code phase, chips.
 * @param bn         Code-loop noise bandwidth (per code epoch).
 * @param zeta       Damping factor (0.707 = critically damped).
 * @param spacing    Early/late tap offset, chips (0.5 = half-chip).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call pdespread_destroy() when done.
 */
pdespread_state_t *pdespread_create(const uint8_t *code, size_t code_len,
                                    size_t sps, size_t k, double init_chip,
                                    double bn, double zeta, double spacing);

/**
 * @brief Destroy a despreader and release all memory (incl. the code copy).
 * @param state  May be NULL.
 */
void pdespread_destroy(pdespread_state_t *state);

/**
 * @brief Re-seed the loop to its create-time code phase; keep config.
 * @param state  Must be non-NULL.
 */
void pdespread_reset(pdespread_state_t *state);

/**
 * @brief Despread a carrier-wiped block; emit @c k partial prompts per epoch.
 *
 * Correlates each input sample against the early/prompt/late code taps
 * (dll_accumulate), dumping a partial prompt every `sf/k` chips and steering the
 * code NCO once per epoch on the non-coherent early-late discriminator.
 *
 * @param state    Despreader state.  Must be non-NULL.
 * @param x        Carrier-wiped input samples.
 * @param x_len    Number of input samples.
 * @param out      Output partial-prompt buffer.
 * @param max_out  Capacity of @p out.
 * @return Number of partial prompts written (<= max_out).
 */
size_t pdespread_steps(pdespread_state_t *state, const float complex *x,
                       size_t x_len, float complex *out, size_t max_out);

/** Output bound for pdespread_steps (0 = caller sizes to the input length). */
size_t pdespread_steps_max_out(pdespread_state_t *state);

/** @brief Tracked code phase, chips. */
double pdespread_get_code_phase(const pdespread_state_t *state);
/** @brief Tracked code rate (chips advanced per nominal chip, ~1.0). */
double pdespread_get_code_rate(const pdespread_state_t *state);
/** @brief Last non-coherent discriminator output (loop stress). */
double pdespread_get_last_error(const pdespread_state_t *state);
/** @brief Partial correlations per code epoch. */
size_t pdespread_get_k(const pdespread_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* PDESPREAD_CORE_H */
