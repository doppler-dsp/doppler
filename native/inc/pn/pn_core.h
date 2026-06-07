/**
 * @file pn_core.h
 * @brief PN component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * pn_state_t *obj = pn_create(96, 1, 7);
 * uint8_t y = pn_step(obj);
 * pn_destroy(obj);
 * @endcode
 */
#ifndef PN_CORE_H
#define PN_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PN state.
 *
 * Allocate with pn_create().
 */
/** LFSR realization: Galois (internal XOR) or Fibonacci (external XOR). */
enum { PN_GALOIS = 0, PN_FIBONACCI = 1 };

typedef struct {
    uint64_t poly;     /* Galois feedback polynomial (taps) */
    uint64_t seed;     /* initial register state (for reset) */
    uint64_t reg;      /* current LFSR register */
    uint64_t mask;     /* (1 << length) - 1; all ones when length == 64 */
    int kind;          /* PN_GALOIS or PN_FIBONACCI */
    uint64_t fib_taps; /* Fibonacci feedback taps (canonical poly & mask) */
    uint32_t topshift; /* length-1: position the Fibonacci feedback enters */
} pn_state_t;

/**
 * @brief Create a pn instance.
 *
 * @param poly  Galois tap polynomial (default: 96). The Fibonacci taps are
 *              derived from it, so both realizations share the same primitive
 *              polynomial (and period).
 * @param seed  seed (default: 1).
 * @param length  register length, 1..64 (default: 7).
 * @param lfsr  PN_GALOIS (0, default) or PN_FIBONACCI (1).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call pn_destroy() when done.
 */
pn_state_t *pn_create(uint64_t poly, uint64_t seed, uint32_t length, int lfsr);

/**
 * @brief Destroy a pn instance and release all memory.
 * @param state  May be NULL.
 */
void pn_destroy(pn_state_t *state);

/**
 * @brief Reset PN to its post-create state.
 * @param state  Must be non-NULL.
 */
void pn_reset(pn_state_t *state);

/**
 * @brief Advance the LFSR one step; return the output chip (0 or 1).
 *
 * Both realizations output the register LSB and shift right. Galois XORs the
 * tap polynomial on a 1 (internal feedback); Fibonacci feeds the parity of the
 * tapped bits into the top (external feedback). Same primitive polynomial, same
 * period. Inline so per-sample modulators (e.g. synth's bpsk/qpsk data source)
 * can pull chips in a hot loop without a block call.
 * @param state  Must be non-NULL.
 */
JM_FORCEINLINE uint8_t
pn_step(pn_state_t *state)
{
    uint8_t bit = (uint8_t)(state->reg & 1u);
    if (state->kind == PN_FIBONACCI) {
        uint64_t fb = (uint64_t)__builtin_parityll(state->reg & state->fib_taps);
        state->reg = (state->reg >> 1) | (fb << state->topshift);
    } else {
        state->reg >>= 1;
        if (bit)
            state->reg ^= state->poly;
        state->reg &= state->mask;
    }
    return bit;
}








size_t pn_generate_max_out(pn_state_t *state);
size_t pn_generate(pn_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* PN_CORE_H */
