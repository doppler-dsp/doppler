/**
 * @file u8_to_f32_core.h
 * @brief Offset-binary uint8 to float converter — the RTL-SDR `cu8` front end.
 *
 * An RTL-SDR (RTL2832U) streams interleaved I/Q as UNSIGNED 8-bit
 * offset-binary samples, `cu8`: code 0 is the most negative level, 255 the
 * most positive, and the analog zero sits between codes 127 and 128, at
 * 127.5.  This is not the signed-8 format of I8ToF32 (HackRF's `cs8`) — read
 * as signed, every sample above 127 wraps to a large negative value.
 *
 * The converter works element by element on the FLAT interleaved buffer, so
 * `2N` bytes of `I,Q,I,Q,...` become `2N` floats, which is `N` complex
 * samples in `float _Complex` layout.  From C, pass the complex output
 * buffer cast to `float *`; from Python, `.view(np.complex64)` the result.
 *
 * Two mappings, chosen at construction by @p mode:
 *
 * | mode       | formula                   | range           | cost |
 * | ---------- | ------------------------- | --------------- | ---- |
 * | `shift`    | `(x - 128) * 2^-7`        | `[-1, 127/128]` | integer subtract, convert, power-of-two scale |
 * | `midpoint` | `(x - 127.5) * (1/127.5)` | `[-1, +1]`      | float subtract and multiply |
 *
 * **`shift` (the default) is exact and the fast path.**  Every step is exact
 * in float: the integer subtract, the conversion of a value in `[-128, 127]`,
 * and the power-of-two scale.  Both modes vectorise (GCC 14 on AArch64 emits
 * NEON `scvtf` + `fmul` on four lanes; it does not fold the 2^-7 into the
 * convert's fractional-bits operand, though the instruction has one), so the
 * gap between them is small — 4% on a Cortex-A53-class core, 20% on a
 * desktop x86 (`bench_u8_to_f32_core`).  It is identical,
 * bit for bit, to I8ToF32 at `scale=128` applied to `x ^ 0x80`.  The price
 * is a DC bias: it centres on code 128 while the hardware centres on 127.5,
 * so every sample reads `0.5/128` low — an analog zero dithers between codes
 * 127 and 128, which map to `-1/128` and `0` — a constant at about -48 dBFS.
 * In a
 * receiver that tunes the wanted signal off DC — the normal way to use an
 * RTL-SDR, whose own DC spike sits at the same place — the downstream
 * down-converter's channel filter removes it, so the fast path costs
 * nothing that survives.
 *
 * **`midpoint` is the opt-in unbiased mapping**: centred on 127.5 and
 * symmetric, so a zero-mean input stays zero-mean and both rails map to
 * exactly -1 and +1.  Use it when DC matters: a zero-IF capture, a power
 * measurement taken before any filtering, or a spectrum whose DC bin you
 * intend to read.  The scale is a pre-computed reciprocal, so a result can
 * differ from the true quotient `(x - 127.5)/127.5` in the last bit.
 *
 * Stateless: nothing survives between calls, so there is no state to
 * serialize and reset() is a no-op.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
 *
 * @code
 * >>> from doppler.cvt import U8ToF32
 * >>> import numpy as np
 * >>> iq = np.array([0, 128, 255, 128], dtype=np.uint8)   # I,Q,I,Q
 * >>> U8ToF32().steps(iq).tolist()                        # mode="shift"
 * [-1.0, 0.0, 0.9921875, 0.0]
 * >>> U8ToF32().steps(iq).view(np.complex64).tolist()
 * [(-1+0j), (0.9921875+0j)]
 * >>> U8ToF32(mode="midpoint").steps(iq)[[0, 2]].tolist()  # both rails
 * [-1.0, 1.0]
 * @endcode
 */
#ifndef DP_U8_TO_F32_CORE_H
#define DP_U8_TO_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The two mappings, in the order of the Python `mode` string enum.
 *
 * The Python binding passes the index of `"shift"` / `"midpoint"`, so these
 * values ARE that order; a C caller may use either spelling.
 */
typedef enum {
    U8_TO_F32_SHIFT    = 0, /**< `(x - 128) * 2^-7`: exact, fast, DC-biased */
    U8_TO_F32_MIDPOINT = 1  /**< `(x - 127.5) / 127.5`: symmetric, unbiased */
} u8_to_f32_mode_t;

/**
 * @brief U8ToF32 state.
 *
 * Allocate with dp_u8_to_f32_create().
 */
typedef struct {
    int   mode;   /* u8_to_f32_mode_t, validated at create */
    float iscale; /* 1/127.5, pre-computed for the midpoint multiply */
} dp_u8_to_f32_state_t;

/**
 * @brief Create a u8_to_f32 instance.
 *
 * @param mode  #U8_TO_F32_SHIFT (0) or #U8_TO_F32_MIDPOINT (1); from Python,
 *              the string `"shift"` (default) or `"midpoint"`.
 * @return Heap-allocated state, or NULL for an unknown @p mode.  The
 *         allocation itself cannot fail visibly: it aborts on out-of-memory
 *         (dp_xcalloc), as every fixed-size internal allocation does.
 * @note Caller must call dp_u8_to_f32_destroy() when done.
 */
dp_u8_to_f32_state_t *dp_u8_to_f32_create(int mode);

/**
 * @brief Destroy a u8_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void dp_u8_to_f32_destroy(dp_u8_to_f32_state_t *state);

/**
 * @brief No-op reset, provided only for lifecycle symmetry.
 *
 * The mode and its reciprocal are fixed at construction and nothing else is
 * held, so there is nothing to clear; the method exists so every converter
 * in the module presents the same create / step / reset / destroy
 * lifecycle.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.cvt import U8ToF32
 * >>> c = U8ToF32()
 * >>> c.reset()          # stateless converter -> reset is a no-op
 * >>> c.step(0)
 * -1.0
 *
 * @endcode
 */
void dp_u8_to_f32_reset(dp_u8_to_f32_state_t *state);

/**
 * @brief The `shift` mapping of one code: `(x - 128) * 2^-7`, exactly.
 *
 * The one definition of the fast path; step() and steps() both call it.
 * The subtract is done in `int32_t` rather than by flipping the top bit into
 * an `int8_t`, which gives the same value without C's implementation-defined
 * narrowing conversion.
 *
 * @param x  Offset-binary code in `[0, 255]`.
 * @return `(x - 128) / 128`, in `[-1, 127/128]`.
 */
JM_FORCEINLINE float
u8_to_f32_shift(uint8_t x)
{
    return (float)((int32_t)x - 128) * 0x1p-7f;
}

/**
 * @brief The `midpoint` mapping of one code: `(x - 127.5) * (1/127.5)`.
 *
 * The one definition of the unbiased path; step() and steps() both call it.
 *
 * @param state  Must be non-NULL (supplies the pre-computed reciprocal).
 * @param x      Offset-binary code in `[0, 255]`.
 * @return `(x - 127.5) / 127.5` to within the last bit, in `[-1, +1]`.
 */
JM_FORCEINLINE float
u8_to_f32_midpoint(const dp_u8_to_f32_state_t *state, uint8_t x)
{
    return ((float)x - 127.5f) * state->iscale;
}

/**
 * @brief Convert one offset-binary code to a normalised float.
 *
 * Dispatches on the mode chosen at construction.  For a block, steps()
 * resolves the mode once and runs a branch-free loop instead.
 *
 * @param state  Must be non-NULL.
 * @param x      Offset-binary code in `[0, 255]`.
 * @return The mapped sample (see the table at the top of this file).
 *
 * @code
 * >>> from doppler.cvt import U8ToF32
 * >>> c = U8ToF32()             # mode="shift": (x - 128) / 128, exact
 * >>> c.step(0), c.step(128), c.step(192)
 * (-1.0, 0.0, 0.5)
 * >>> m = U8ToF32(mode="midpoint")
 * >>> m.step(0), m.step(255)    # symmetric: both rails reach full scale
 * (-1.0, 1.0)
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT float
dp_u8_to_f32_step(const dp_u8_to_f32_state_t *state, uint8_t x)
{
    return state->mode == U8_TO_F32_MIDPOINT ? u8_to_f32_midpoint(state, x)
                                             : u8_to_f32_shift(x);
}

/**
 * @brief Convert a block of offset-binary codes to float32.
 *
 * The mode is resolved once for the block, then one branch-free loop runs,
 * so the per-sample work is only the mapping itself.  Feed it the flat
 * interleaved I/Q buffer; the output is then complex samples in
 * `float _Complex` layout.
 *
 * @param state   Must be non-NULL.
 * @param input   Input uint8 array; must contain at least @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples (bytes) to convert.
 *
 * @code
 * >>> from doppler.cvt import U8ToF32
 * >>> import numpy as np
 * >>> cu8 = np.array([128, 0, 192, 64], dtype=np.uint8)  # 2 I/Q pairs
 * >>> U8ToF32().steps(cu8).view(np.complex64).tolist()
 * [-1j, (0.5-0.5j)]
 *
 * @endcode
 */
void dp_u8_to_f32_steps(
    dp_u8_to_f32_state_t *state,
    const uint8_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* U8_TO_F32_CORE_H */
