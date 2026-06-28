/**
 * @file pn_core.h
 * @brief PN component API.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
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
#include "dp_state.h"
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
 * @brief Allocate and initialise a maximal-length-sequence LFSR.
 * The register is seeded from ``seed`` and will produce a pseudo-random
 * binary sequence with period 2^length - 1 for any primitive ``poly``.
 * Both Galois and Fibonacci realizations share the same primitive polynomial
 * and therefore the same period; they differ only in chip ordering/phase.
 *
 * @param poly  Galois feedback tap polynomial (right-shift convention).
 *              The LSB is the tap at position 0 (always 1 for a primitive
 *              poly); bit k=1 means tap at position k. Default 96 (0x60)
 *              is primitive for length=7, giving period 127. The Fibonacci
 *              taps are derived automatically so you only supply one value.
 * @param seed  Initial LFSR register state; must be non-zero (the all-zero
 *              state is a fixed point). Default 1.
 * @param length  Register width in bits, 1..64. The sequence period is
 *              2^length - 1 for a primitive polynomial. Default 7.
 * @param lfsr  Realization: PN_GALOIS (0, default) or PN_FIBONACCI (1).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call pn_destroy() when done.
 * @code
 * >>> from doppler.wfm import PN
 * >>> import numpy as np
 * >>> p = PN(poly=96, seed=1, length=7)
 * >>> chips = p.generate(127)
 * >>> chips.dtype
 * dtype('uint8')
 * >>> int(chips.sum())   # 64 ones per MLS period (2^(n-1))
 * 64
 * @endcode
 */
pn_state_t *pn_create(uint64_t poly, uint64_t seed, uint32_t length, int lfsr);

/**
 * @brief Destroy a pn instance and release all memory.
 * Idempotent when ``state`` is NULL; safe to call at any point in the
 * lifecycle.  After return the pointer is dangling — do not dereference it.
 *
 * @param state  Pointer to heap-allocated state; may be NULL (no-op).
 * @code
 * >>> from doppler.wfm import PN
 * >>> p = PN(poly=96, seed=1, length=7)
 * >>> p.destroy()   # explicit teardown; no exception
 * @endcode
 */
void pn_destroy(pn_state_t *state);

/**
 * @brief Reset PN to its post-create state.
 * Reloads the LFSR register from the original seed so the sequence restarts
 * from chip 0.  Useful for reproducible captures without re-allocating.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> from doppler.wfm import PN
 * >>> import numpy as np
 * >>> p = PN(poly=96, seed=1, length=7)
 * >>> a = p.generate(8).copy()
 * >>> p.reset()
 * >>> np.array_equal(a, p.generate(8))
 * True
 * @endcode
 */
void pn_reset(pn_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Only the running LFSR register is serialized; poly / seed / mask / kind /
 * fib_taps / topshift are config restored by the constructor.
 * Envelope: [dp_state_hdr_t][u64 reg]. */
#define PN_STATE_MAGIC DP_FOURCC('P', 'N', '_', '_')
#define PN_STATE_VERSION 1u

/** @brief Serialized-state byte size. */
size_t pn_state_bytes(const pn_state_t *state);
/** @brief Serialize the LFSR register into @p blob. */
void pn_get_state(const pn_state_t *state, void *blob);
/** @brief Restore the register; DP_OK, or DP_ERR_INVALID if rejected. */
int pn_set_state(pn_state_t *state, const void *blob);

/**
 * @brief Advance the LFSR one step and return the output chip (0 or 1).
 * Both realizations output the register LSB and then shift right. Galois
 * XORs the tap polynomial on a 1 output bit (internal feedback); Fibonacci
 * computes the parity of all tapped positions and inserts it at the top
 * (external feedback). Same primitive polynomial, same period. Inlined so
 * per-sample modulators (e.g. synth's bpsk/qpsk data source) can pull chips
 * in a tight hot loop without call overhead.
 *
 * @param state  Must be non-NULL.
 * @return Output chip: 0 or 1 (register LSB before the shift).
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

/**
 * @brief Generate ``n`` chips into ``out`` and advance the LFSR by ``n``
 * positions.  Each element of ``out`` is 0 or 1.  Requesting more than one
 * MLS period is valid — the sequence simply wraps around.  The Python
 * binding returns a zero-copy NumPy uint8 view over a pre-allocated buffer;
 * copy the result before calling generate again if you need a snapshot.
 *
 * @param state  Initialised PN state returned by ``pn_create``.
 * @param n      Number of chips to produce.
 * @param out    Output buffer of at least ``n`` uint8 elements; each element
 *               receives 0 or 1.
 * @return ``n`` (the number of chips written; always equal to the request).
 * @code
 * >>> from doppler.wfm import PN
 * >>> import numpy as np
 * >>> p = PN(poly=96, seed=1, length=7)
 * >>> chips = p.generate(127)
 * >>> chips[:8].tolist()
 * [1, 0, 0, 0, 0, 0, 1, 1]
 * >>> int(chips.sum())   # 64 ones per MLS period
 * 64
 * @endcode
 */
size_t pn_generate(pn_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* PN_CORE_H */
