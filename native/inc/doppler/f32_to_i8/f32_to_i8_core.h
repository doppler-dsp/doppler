/**
 * @file f32_to_i8_core.h
 * @brief Scale-and-saturate float-to-int8 converter.
 *
 * Multiplies the input by @c scale, rounds to the nearest integer, and
 * saturates (clamps) the result to the int8 range `[-128, 127]`.
 * The default scale of 128.0 maps a normalised `[-1, +1]` float to the full
 * 8-bit integer range, making it the exact counterpart of I8ToF32.
 * A sticky @c clipped flag is raised on any sample that saturates and is
 * cleared only by reset().
 *
 * The full scale is 2^7, not 127: the code grid a converter actually has is
 * 2^8 equally spaced steps of 1/2^7, so scaling by 2^7 maps the normalised
 * range onto that grid exactly and a dyadic input round-trips with no error.
 * An input of exactly +1.0 lands on 128, one past INT8_MAX by construction,
 * and saturating it is what this mapping means rather than a failure of it.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToI8
 * >>> import numpy as np
 * >>> obj = F32ToI8(scale=128.0)
 * >>> obj.step(0.5)
 * 64
 * >>> obj.step(-1.0)
 * -128
 * >>> obj.clipped
 * False
 * >>> obj.step(1.0)
 * 127
 * >>> obj.clipped
 * True
 * >>> obj.reset()
 * >>> obj.clipped
 * False
 * >>> x = np.array([0.5, -0.5, 1.0], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [64, -64, 127]
 * @endcode
 */
#ifndef DP_F32_TO_I8_CORE_H
#define DP_F32_TO_I8_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI8 state.
 *
 * Allocate with dp_f32_to_i8_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-128, 127]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /**< Multiply factor applied before rounding and
                          saturation (codes per unit input).  Config, set
                          by create(). */
    uint8_t clipped; /**< Sticky flag: 1 once any scaled sample fell
                          outside `[-128, 127]`, 0 otherwise.  Running
                          state, set by step()/steps(), cleared by
                          reset(). */
} dp_f32_to_i8_state_t;

/**
 * @brief Create a f32_to_i8 instance.
 *
 * Allocates state and stores @p scale.  The @c clipped flag is initialised
 * to 0.  Returns NULL for a scale that is not positive and finite, which
 * is the only failure a caller can cause; the allocation itself aborts on
 * exhaustion (dp_xcalloc) rather than handing back an unwind path no test
 * can reach.
 *
 * @param scale  Multiply factor applied before rounding and saturation
 *               (default: 128.0f).  Use 128.0 to convert a normalised
 *               `[-1, +1]` signal to the full 8-bit range.
 * @return Heap-allocated state, or NULL if @p scale is not positive and
 *         finite (zero, negative, NaN or infinite).
 * @note Caller must call dp_f32_to_i8_destroy() when done.
 */
dp_f32_to_i8_state_t *dp_f32_to_i8_create(float scale);

/**
 * @brief Destroy a f32_to_i8 instance and release all memory.
 * @param state  Instance from dp_f32_to_i8_create(); may be NULL (no-op).
 */
void dp_f32_to_i8_destroy(dp_f32_to_i8_state_t *state);

/**
 * @brief Clear the sticky clip flag, starting a fresh saturation history.
 *
 * Zeroes @c clipped so a subsequent clipped query reflects only samples seen
 * after this call; @c scale is preserved. Call it at a buffer or
 * segment boundary so a saturation on one block does not leak into the next.
 *
 * @param state  Must be non-NULL.
 *
 * @code
 * >>> from doppler.cvt import F32ToI8
 * >>> c = F32ToI8()
 * >>> c.step(9.0)          # out of range -> saturates, latches clipped
 * 127
 * >>> c.reset()            # forget the clip history
 * >>> c.clipped
 * False
 *
 * @endcode
 */
void dp_f32_to_i8_reset(dp_f32_to_i8_state_t *state);

/**
 * @brief Scale one float sample by @c scale, round, and saturate to int8.
 *
 * Computes @c round(x * scale), clamps to the int8 range `[-128, 127]`,
 * and latches the sticky @c clipped flag if the scaled value fell outside that
 * range before clamping (and before rounding, so 127.3 latches it even
 * though it would round to 127). At the default scale of 128 a normalised
 * `[-1, +1]` input maps to the full 8-bit code range.
 *
 * A NaN input counts as clipped: it latches @c clipped and returns the
 * negative full-scale code, -128.
 *
 * @param state  Instance to update (clip flag); must be non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Saturated int8 code in `[-128, 127]`.
 *
 * @code
 * >>> from doppler.cvt import F32ToI8
 * >>> c = F32ToI8(scale=128.0)    # normalised float -> full-scale int8
 * >>> c.step(0.5)                 # 0.5 * 128
 * 64
 * >>> c.step(2.0)                 # beyond +1.0 -> saturates to max
 * 127
 * >>> c.clipped                   # sticky flag latched by the clip
 * True
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT int8_t
dp_f32_to_i8_step(dp_f32_to_i8_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    /* Written as "not inside" so a NaN, which fails every comparison,
     * latches the flag too (fmaxf then maps it to the lower bound). */
    state->clipped |= (uint8_t)!(s <= 127.0f && s >= -128.0f);
    s = fmaxf(s, -128.0f);
    s = fminf(s,  127.0f);
    return (int8_t)lroundf(s);
}

/**
 * @brief Process a block of float samples to int8.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block — a single saturating sample raises it
 * for the entire call.  The C function writes into @p output and never
 * allocates; the Python binding allocates the output array when its
 * optional @c out is omitted.
 *
 * @param state   Instance to update (clip flag); must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output int8 array; must hold at least @p n elements and
 *                must be non-NULL.
 * @param n       Number of samples to process (0 is a no-op).
 *
 * @code
 * >>> from doppler.cvt import F32ToI8
 * >>> import numpy as np
 * >>> x = np.array([0.0, 0.5, -1.0, 0.99], dtype=np.float32)
 * >>> F32ToI8().steps(x).tolist()   # scale=128 -> full-scale int8
 * [0, 64, -128, 127]
 *
 * @endcode
 */
void dp_f32_to_i8_steps(
    dp_f32_to_i8_state_t *state,
    const float    *input,
    int8_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
/** FourCC type tag ("F2_8") stamped in every F32ToI8 state blob. */
#define F32_TO_I8_STATE_MAGIC DP_FOURCC ('F','2','_','8')
/** F32ToI8 state blob format version; another version is rejected. */
#define F32_TO_I8_STATE_VERSION 1u
/**
 * @brief Size in bytes of this instance's serialized state blob.
 *
 * A fixed size: the 16-byte dp_state_hdr_t envelope plus
 * sizeof(dp_f32_to_i8_state_t) (the whole struct is snapshotted).
 *
 * @param state  Instance to size; not dereferenced.
 * @return Blob size in bytes; allocate this much for
 *         dp_f32_to_i8_get_state().
 */
size_t dp_f32_to_i8_state_bytes (const dp_f32_to_i8_state_t *state);
/**
 * @brief Serialize the full state (scale and sticky clip flag).
 *
 * @param state  Instance to snapshot; non-NULL.
 * @param[out] blob  Destination of at least dp_f32_to_i8_state_bytes()
 *                   bytes.
 */
void dp_f32_to_i8_get_state (const dp_f32_to_i8_state_t *state, void *blob);
/**
 * @brief Restore a state blob written by dp_f32_to_i8_get_state().
 *
 * Validates the envelope (magic, version, endianness, size) first and
 * leaves @p state untouched on rejection.  On success the whole struct,
 * @c scale included, is overwritten.
 *
 * @param state  Instance to overwrite; non-NULL.
 * @param blob   Blob of dp_f32_to_i8_state_bytes() bytes.
 * @return DP_OK, or DP_ERR_INVALID if the envelope does not match.
 */
int dp_f32_to_i8_set_state (dp_f32_to_i8_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I8_CORE_H */
