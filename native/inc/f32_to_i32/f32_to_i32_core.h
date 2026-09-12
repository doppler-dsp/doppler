/**
 * @file f32_to_i32_core.h
 * @brief Scale-and-saturate float-to-int32 converter.
 *
 * Multiplies the input by @c scale, rounds to the nearest integer, and
 * saturates (clamps) the result to the int32 range
 * `[-2147483648, 2147483647]`.  The default scale of 2147483648.0 (2^31) maps
 * a normalised `[-1, +1]` float to the full 32-bit integer range, making it
 * the exact counterpart of I32ToF32.
 * A sticky @c clipped flag is raised on any sample that saturates and is
 * cleared only by reset().
 *
 * The full scale is 2^31, not 2^31-1: the code grid a converter actually has
 * is 2^32 equally spaced steps of 1/2^31, so scaling by 2^31 maps the
 * normalised range onto that grid exactly and a dyadic input round-trips with
 * no error.  An input of exactly +1.0 lands on 2^31, one past INT32_MAX by
 * construction, and saturating it is what this mapping means rather than a
 * failure of it.
 *
 * Note the source is a float32, so only 24 significant bits survive the
 * multiply — the low bits of a full-scale code are not meaningful, and two
 * inputs a float apart can produce codes hundreds apart.  That is a property
 * of the input type, not of this conversion; I32ToF32 documents the mirror.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToI32
 * >>> import numpy as np
 * >>> obj = F32ToI32(scale=2147483648.0)
 * >>> obj.step(0.5)
 * 1073741824
 * >>> obj.step(-1.0)
 * -2147483648
 * >>> obj.clipped
 * False
 * >>> obj.step(1.0)
 * 2147483647
 * >>> obj.clipped
 * True
 * >>> obj.reset()
 * >>> obj.clipped
 * False
 * >>> x = np.array([0.0, 0.25, -0.5], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [0, 536870912, -1073741824]
 * @endcode
 */
#ifndef F32_TO_I32_CORE_H
#define F32_TO_I32_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI32 state.
 *
 * Allocate with f32_to_i32_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-2147483648, 2147483647]`; cleared only by
 * reset().
 */
typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_i32_state_t;

/**
 * @brief Create a f32_to_i32 instance.
 *
 * Allocates state and stores @p scale.  The @c clipped flag is initialised
 * to 0.  Returns NULL for a non-positive scale, which is the only failure a
 * caller can cause; the allocation itself aborts on exhaustion (dp_xcalloc)
 * rather than handing back an unwind path no test can reach.
 *
 * @param scale  Multiply factor applied before rounding and saturation
 *               (default: 2147483648.0f).  Use 2^31 to convert a normalised
 *               `[-1, +1]` signal to the full 32-bit range.
 * @return Heap-allocated state, or NULL if @p scale is not positive.
 * @note Caller must call f32_to_i32_destroy() when done.
 */
f32_to_i32_state_t *f32_to_i32_create(float scale);

/**
 * @brief Destroy a f32_to_i32 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_i32_destroy(f32_to_i32_state_t *state);

/**
 * @brief Clear the sticky clip flag, starting a fresh saturation history.
 *
 * Zeroes @c clipped so a subsequent clipped query reflects only samples seen
 * after this call; the immutable @c scale is preserved. Call it at a buffer or
 * segment boundary so a saturation on one block does not leak into the next.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.cvt import F32ToI32
 * >>> c = F32ToI32()
 * >>> c.step(9.0)          # out of range -> saturates, latches clipped
 * 2147483647
 * >>> c.reset()            # forget the clip history
 * >>> c.clipped
 * False
 *
 * @endcode
 */
void f32_to_i32_reset(f32_to_i32_state_t *state);

/**
 * @brief Scale one float sample by @c scale, round, and saturate to int32.
 *
 * Computes @c round(x * scale), clamps to the int32 range
 * `[-2147483648, 2147483647]`, and latches the sticky @c clipped flag if the
 * scaled value fell outside that range before clamping.  At the default scale
 * of 2^31 a normalised `[-1, +1]` input maps to the full 32-bit code range.
 *
 * Unlike its int8 and int16 siblings this works in @c double throughout.
 * INT32_MAX is not representable as a float — 2147483647.0f rounds UP to
 * 2^31 — so a float `fminf(s, 2147483647.0f)` clamps to a value one past the
 * range it is trying to enforce, and the following lround() overflows.  The
 * float32 input still bounds the useful precision at 24 bits; the double is
 * there to make the CLAMP exact, not to invent significance.
 *
 * @param state  Must be non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Saturated int32 code in `[-2147483648, 2147483647]`.
 *
 * @code
 * >>> from doppler.cvt import F32ToI32
 * >>> c = F32ToI32(scale=2147483648.0)  # normalised float -> full-scale
 * >>> c.step(0.5)                       # 0.5 * 2**31
 * 1073741824
 * >>> c.step(2.0)                       # beyond +1.0 -> saturates to max
 * 2147483647
 * >>> c.clipped                         # sticky flag latched by the clip
 * True
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT int32_t
f32_to_i32_step(f32_to_i32_state_t *state, float x)
{
    double s = (double)state->scale * (double)x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 2147483647.0 || s < -2147483648.0);
    if (s > 2147483647.0)
        s = 2147483647.0;
    if (s < -2147483648.0)
        s = -2147483648.0;
    return (int32_t)lround(s);
}

/**
 * @brief Process a block of float samples to int32.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block — a single saturating sample raises it
 * for the entire call.  Accepts an optional pre-allocated output array;
 * allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output int32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import F32ToI32
 * >>> import numpy as np
 * >>> x = np.array([0.0, 0.25, -1.0], dtype=np.float32)
 * >>> F32ToI32().steps(x).tolist()   # scale=2**31 -> full-scale int32
 * [0, 536870912, -2147483648]
 *
 * @endcode
 */
void f32_to_i32_steps(
    f32_to_i32_state_t *state,
    const float    *input,
    int32_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
#define F32_TO_I32_STATE_MAGIC DP_FOURCC ('F','2','3','2')
#define F32_TO_I32_STATE_VERSION 1u
size_t f32_to_i32_state_bytes (const f32_to_i32_state_t *state);
void f32_to_i32_get_state (const f32_to_i32_state_t *state, void *blob);
int f32_to_i32_set_state (f32_to_i32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I32_CORE_H */
