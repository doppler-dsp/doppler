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
typedef struct {
    uint64_t poly; /* Galois feedback polynomial (taps) */
    uint64_t seed; /* initial register state (for reset) */
    uint64_t reg;  /* current LFSR register */
    uint64_t mask; /* (1 << length) - 1; all ones when length == 64 */
} pn_state_t;

/**
 * @brief Create a pn instance.
 *
 * @param poly  poly (default: 96).
 * @param seed  seed (default: 1).
 * @param length  length (default: 7).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call pn_destroy() when done.
 */
pn_state_t *pn_create(uint64_t poly, uint64_t seed, uint32_t length);

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
 * Galois m-sequence step: output = register LSB, shift right, XOR the tap
 * polynomial on a 1. Inline so per-sample modulators (e.g. synth's bpsk/qpsk
 * data source) can pull chips in a hot loop without a block call.
 * @param state  Must be non-NULL.
 */
JM_FORCEINLINE uint8_t
pn_step(pn_state_t *state)
{
    uint8_t bit = (uint8_t)(state->reg & 1u);
    state->reg >>= 1;
    if (bit)
        state->reg ^= state->poly;
    state->reg &= state->mask;
    return bit;
}








size_t pn_generate_max_out(pn_state_t *state);
size_t pn_generate(pn_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* PN_CORE_H */
