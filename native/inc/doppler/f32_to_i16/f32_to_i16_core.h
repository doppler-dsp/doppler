/**
 * @file f32_to_i16_core.h
 * @brief Scale-and-saturate float-to-int16 converter.
 *
 * Multiplies the input by @c scale, rounds to the nearest integer, and
 * saturates (clamps) the result to the int16 range `[-32768, 32767]`.
 * The default scale of 32768.0 maps a normalised `[-1, +1]` float to the full
 * Q15 integer range, making it the natural pair for I16ToF32.
 * A sticky @c clipped flag is raised on any sample that saturates and is
 * cleared only by reset().
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToI16
 * >>> import numpy as np
 * >>> obj = F32ToI16(scale=32768.0)
 * >>> obj.step(0.5)
 * 16384
 * >>> obj.step(-0.5)
 * -16384
 * >>> obj.clipped
 * False
 * >>> obj.step(1.0)
 * 32767
 * >>> obj.clipped
 * True
 * >>> obj.reset()
 * >>> obj.clipped
 * False
 * >>> x = np.array([0.5, -0.5, 1.0], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [16384, -16384, 32767]
 * @endcode
 */
#ifndef DP_F32_TO_I16_CORE_H
#define DP_F32_TO_I16_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI16 state.
 *
 * Allocate with dp_f32_to_i16_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /**< Multiply factor applied before rounding and
                          saturation; set at create, replaced only by
                          set_state(). */
    uint8_t clipped; /**< Sticky: 1 once any sample has saturated, else 0.
                          Written by step/steps; cleared by reset. */
} dp_f32_to_i16_state_t;

/**
 * @brief Create a f32_to_i16 instance.
 *
 * Allocates state and stores @p scale.  The @c clipped flag is initialised
 * to 0.  The only validation is that @p scale must be positive and finite.
 *
 * @param scale  Multiply factor applied before rounding and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert a normalised
 *               `[-1, +1]` signal to full Q15 range.
 * @return Heap-allocated state, or NULL if @p scale is not positive and
 *         finite, or on allocation failure.
 * @note Caller must call dp_f32_to_i16_destroy() when done.
 */
dp_f32_to_i16_state_t *dp_f32_to_i16_create(float scale);

/**
 * @brief Destroy a f32_to_i16 instance and release all memory.
 * @param state  May be NULL.
 */
void dp_f32_to_i16_destroy(dp_f32_to_i16_state_t *state);

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
 * >>> from doppler.cvt import F32ToI16
 * >>> c = F32ToI16()
 * >>> c.step(9.0)          # out of range -> saturates, latches clipped
 * 32767
 * >>> c.reset()            # forget the clip history
 * >>> c.clipped
 * False
 *
 * @endcode
 */
void dp_f32_to_i16_reset(dp_f32_to_i16_state_t *state);

/**
 * @brief Scale one float sample by @c scale, round, and saturate to int16.
 *
 * Computes @c round(x * scale), clamps to the int16 range `[-32768, 32767]`,
 * and latches the sticky @c clipped flag if the scaled value fell outside that
 * range before clamping. At the default scale of 32768 a normalised `[-1, +1]`
 * input maps to the full Q15 code range.
 *
 * A NaN input counts as clipped: it latches @c clipped and returns the
 * negative full-scale code, -32768.
 *
 * @param state  Must be non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Saturated int16 code in `[-32768, 32767]`.
 *
 * @code
 * >>> from doppler.cvt import F32ToI16
 * >>> c = F32ToI16(scale=32768.0)   # normalised float -> full-scale Q15
 * >>> c.step(0.5)                    # 0.5 * 32768
 * 16384
 * >>> c.step(2.0)                 # beyond +1.0 -> saturates to max
 * 32767
 * >>> c.clipped                      # sticky flag latched by the clip
 * True
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT int16_t
dp_f32_to_i16_step(dp_f32_to_i16_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    /* Written as "not inside" so a NaN, which fails every comparison,
     * latches the flag too (fmaxf then maps it to the lower bound). */
    state->clipped |= (uint8_t)!(s <= 32767.0f && s >= -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    return (int16_t)lroundf(s);
}

/**
 * @brief Process a block of float samples to int16.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block — a single saturating sample raises it
 * for the entire call.  The C function writes into the caller's @p output
 * and never allocates; the Python binding allocates a fresh array when no
 * `out=` buffer is passed.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output int16 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import F32ToI16
 * >>> import numpy as np
 * >>> x = np.array([0.0, 0.5, -1.0, 0.999], dtype=np.float32)
 * >>> F32ToI16().steps(x).tolist()   # scale=32768 -> full-scale i16
 * [0, 16384, -32768, 32735]
 *
 * @endcode
 */
void dp_f32_to_i16_steps(
    dp_f32_to_i16_state_t *state,
    const float    *input,
    int16_t          *output,
    size_t               n);





/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
/** FourCC type tag ("F216") stamped in an F32ToI16 state blob's header. */
#define F32_TO_I16_STATE_MAGIC DP_FOURCC ('F','2','1','6')
/** Layout version of the F32ToI16 state blob; a mismatch is rejected. */
#define F32_TO_I16_STATE_VERSION 1u
/**
 * @brief Serialized-state byte size.
 * @param state  Converter state (unused: the size is fixed).
 * @return `sizeof(dp_state_hdr_t) + sizeof(dp_f32_to_i16_state_t)`.
 */
size_t dp_f32_to_i16_state_bytes (const dp_f32_to_i16_state_t *state);
/**
 * @brief Serialize the state (scale and sticky clip flag) into @p blob.
 * @param state  Must be non-NULL.
 * @param blob   Destination; at least dp_f32_to_i16_state_bytes() bytes.
 */
void dp_f32_to_i16_get_state (const dp_f32_to_i16_state_t *state, void *blob);
/**
 * @brief Restore state from a blob written by dp_f32_to_i16_get_state().
 *
 * Replaces the whole struct, including @c scale; nothing changes if the
 * envelope is rejected.
 *
 * @param state  Must be non-NULL.
 * @param blob   Source blob of dp_f32_to_i16_state_bytes() bytes.
 * @return DP_OK, or DP_ERR_INVALID if the envelope (magic, version,
 *         endianness, size) does not match.
 */
int dp_f32_to_i16_set_state (dp_f32_to_i16_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16_CORE_H */
