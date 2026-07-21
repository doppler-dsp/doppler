/**
 * @file gold_core.h
 * @brief Gold code component API.
 *
 * CCSDS Command Link Gold Code Generator (CCSDS 415.0-G-1, section 5.2.2.4,
 * Figure 5-1): two same-clocked Fibonacci LFSRs ("Register A" and
 * "Register B"), each with its own fixed feedback-tap polynomial, XOR-
 * combined chip-by-chip into a single 1023-chip (length=10) Gold code. The
 * two m-sequences form a genuine "preferred pair" — their XOR family has a
 * strict three-valued periodic autocorrelation/cross-correlation set
 * {-1, -65, 63} (verified: see native/tests/test_gold_core.c). Register A's
 * initial condition is "User dependent" per the standard — varying it walks
 * the whole Gold-code family (2^length members); Register B's taps and
 * initial condition are both fixed by the standard.
 *
 * Lifecycle: create -> generate/reset (repeatable) -> destroy
 *
 * Example:
 * @code
 * gold_state_t *obj = gold_create(934, 350, 567, 73, 10);
 * uint8_t chips[16];
 * gold_generate(obj, 16, chips);
 * gold_destroy(obj);
 * @endcode
 */
#ifndef GOLD_CORE_H
#define GOLD_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gold state.
 *
 * Allocate with gold_create().
 */
typedef struct {
    uint64_t reg_a;   /* Register A: current LFSR register */
    uint64_t reg_b;   /* Register B: current LFSR register */
    uint64_t taps_a;  /* Register A: fixed feedback-tap mask (bit k = stage k+1) */
    uint64_t taps_b;  /* Register B: fixed feedback-tap mask (bit k = stage k+1) */
    uint64_t seed_a;  /* Register A: initial value (for reset); user-dependent */
    uint64_t seed_b;  /* Register B: initial value (for reset); fixed by CCSDS */
    uint64_t mask;    /* (1 << length) - 1; all ones when length == 64 */
    uint32_t length;  /* register width in bits (stage count); CCSDS uses 10 */
} gold_state_t;

/**
 * @brief Allocate and initialise a CCSDS-style Gold code generator.
 * Two independent Fibonacci LFSRs of the same ``length`` free-run in
 * lock-step; each output chip is the XOR of both registers' current top-bit
 * (stage ``length``, i.e. bit ``length-1``). Both registers shift left one
 * bit per chip: the new bit (parity of the tapped stages, read *before* the
 * shift) enters at stage 1 (bit 0), and the old stage-``length`` bit is
 * discarded after being XORed into the output. The sequence period is
 * ``2^length - 1`` for primitive ``taps_a``/``taps_b``.
 *
 * @param taps_a  Register A feedback-tap mask; bit k set means stage k+1 is
 *              XORed into the feedback. Default 934 (stages 2,3,6,8,9,10 —
 *              the CCSDS-fixed Register A polynomial
 *              x^10+x^9+x^8+x^6+x^3+x^2+1).
 * @param seed_a  Register A initial value; must be non-zero. Per CCSDS this
 *              is "User dependent" — any nonzero value selects a different
 *              member of the 1024-code Gold family. Default 350 is the
 *              worked example from CCSDS 415.0-G-1 Figure 5-2 (PN Code
 *              Library Table 1, Code Number 365).
 * @param taps_b  Register B feedback-tap mask, same bit convention as
 *              ``taps_a``. Default 567 (stages 1,2,3,5,6,10 — the
 *              CCSDS-fixed Register B polynomial).
 * @param seed_b  Register B initial value; must be non-zero. Default 73
 *              (stages 1,4,7 — CCSDS's fixed Register B initial value
 *              1001001000, unique per the standard, not user-selectable).
 * @param length  Register width in bits, 1..64. CCSDS command link uses 10
 *              (period 1023). Default 10.
 * @return Heap-allocated state, or NULL on allocation failure or invalid
 *              arguments (zero seed, zero/out-of-range length).
 * @note Caller must call gold_destroy() when done.
 * @code
 * >>> from doppler.wfm import Gold
 * >>> import numpy as np
 * >>> g = Gold()
 * >>> chips = g.generate(1023)
 * >>> chips.dtype
 * dtype('uint8')
 * >>> chips[:15].tolist()   # CCSDS Code #365 worked example
 * [0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1]
 * >>> int(chips.sum()), int((1 - chips).sum())   # balanced: 512 ones, 511 zeros
 * (512, 511)
 * @endcode
 */
gold_state_t *gold_create(uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                           uint64_t seed_b, uint32_t length);

/**
 * @brief Destroy a gold instance and release all memory.
 * Idempotent when ``state`` is NULL; safe to call at any point in the
 * lifecycle. After return the pointer is dangling — do not dereference it.
 *
 * @param state  Pointer to heap-allocated state; may be NULL (no-op).
 * @code
 * >>> from doppler.wfm import Gold
 * >>> g = Gold()
 * >>> g.destroy()   # explicit teardown; no exception
 * @endcode
 */
void gold_destroy(gold_state_t *state);

/**
 * @brief Reset Gold to its post-create state.
 * Reloads both LFSR registers from their original seeds so the sequence
 * restarts from chip 0. Useful for reproducible captures without
 * re-allocating.
 *
 * @param state  Must be non-NULL.
 * @code
 * >>> from doppler.wfm import Gold
 * >>> import numpy as np
 * >>> g = Gold()
 * >>> a = g.generate(8).copy()
 * >>> g.reset()
 * >>> np.array_equal(a, g.generate(8))
 * True
 * @endcode
 */
void gold_reset(gold_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Only the two running LFSR registers are serialized; taps / seeds / mask /
 * length are config restored by the constructor.
 * Envelope: [dp_state_hdr_t][u64 reg_a][u64 reg_b]. */
#define GOLD_STATE_MAGIC DP_FOURCC('G', 'O', 'L', 'D')
#define GOLD_STATE_VERSION 1u

/** @brief Serialized-state byte size. */
size_t gold_state_bytes(const gold_state_t *state);
/** @brief Serialize both LFSR registers into @p blob. */
void gold_get_state(const gold_state_t *state, void *blob);
/** @brief Restore both registers; DP_OK, or DP_ERR_INVALID if rejected. */
int gold_set_state(gold_state_t *state, const void *blob);

/**
 * @brief Advance both LFSRs one chip and return the XOR-combined output
 * chip (0 or 1). Each register outputs its current stage-``length`` bit
 * (the top bit), computes its own feedback (parity of the tapped stages),
 * and shifts left with the feedback bit entering at stage 1. Inlined so
 * composing objects (e.g. a DSSS spreader) can pull chips in a tight hot
 * loop without call overhead — mirrors pn_core.h's pn_step().
 *
 * @param state  Must be non-NULL.
 * @return Output chip: 0 or 1.
 */
JM_FORCEINLINE uint8_t
gold_step(gold_state_t *state)
{
    uint64_t a = state->reg_a;
    uint64_t b = state->reg_b;
    uint32_t top = state->length - 1u;
    uint8_t out = (uint8_t)(((a >> top) ^ (b >> top)) & 1u);
    uint64_t fb_a = (uint64_t)__builtin_parityll(a & state->taps_a);
    uint64_t fb_b = (uint64_t)__builtin_parityll(b & state->taps_b);
    state->reg_a = ((a << 1) | fb_a) & state->mask;
    state->reg_b = ((b << 1) | fb_b) & state->mask;
    return out;
}

size_t gold_generate_max_out(gold_state_t *state);

/**
 * @brief Generate ``n`` chips into ``out`` and advance both LFSRs by ``n``
 * positions. Each element of ``out`` is 0 or 1. Requesting more than one
 * period is valid — the sequence simply wraps around. The Python binding
 * returns a zero-copy NumPy uint8 view over a pre-allocated buffer; copy
 * the result before calling generate again if you need a snapshot.
 *
 * @param state  Initialised Gold state returned by ``gold_create``.
 * @param n      Number of chips to produce.
 * @param out    Output buffer of at least ``n`` uint8 elements; each
 *              element receives 0 or 1.
 * @return ``n`` (the number of chips written; always equal to the request).
 * @code
 * >>> from doppler.wfm import Gold
 * >>> import numpy as np
 * >>> g = Gold()
 * >>> chips = g.generate(1023)
 * >>> len(chips)
 * 1023
 * @endcode
 */
size_t gold_generate(gold_state_t *state, size_t n, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* GOLD_CORE_H */
