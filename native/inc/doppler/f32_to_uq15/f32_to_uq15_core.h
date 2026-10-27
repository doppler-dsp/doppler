/**
 * @file f32_to_uq15_core.h
 * @brief Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter.
 *
 * Converts a normalised float sample to offset-binary uint16 (UQ15 format).
 * The Q15 quantised value is biased by +32768 so that the full unsigned range
 * maps to the signed float domain:
 *   -1.0  → uint16    0  (0x0000)
 *    0.0  → uint16 32768 (0x8000)
 *   +1.0  → uint16 65535 (0xFFFF)
 *
 * Encoding:
 * @code
 *   v_Q15 = clamp(round(x * scale), -32768, 32767)
 *   u     = (uint16_t)((int32_t)v_Q15 + 32768)
 * @endcode
 *
 * This is the unsigned wire format used by some DAC and file-format
 * conventions that cannot represent negative integer values.  UQ15ToF32
 * performs the exact inverse.  A sticky @c clipped flag is raised on
 * saturation and cleared only by reset().
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToUQ15
 * >>> import numpy as np
 * >>> obj = F32ToUQ15(scale=32768.0)
 * >>> obj.step(0.0)
 * 32768
 * >>> obj.step(-1.0)
 * 0
 * >>> obj.clipped
 * False
 * >>> obj.step(1.0)
 * 65535
 * >>> obj.clipped
 * True
 * >>> obj.reset()
 * >>> x = np.array([-1.0, 0.0, 1.0], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [0, 32768, 65535]
 * @endcode
 */
#ifndef DP_F32_TO_UQ15_CORE_H
#define DP_F32_TO_UQ15_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToUQ15 state.
 *
 * Allocate with dp_f32_to_uq15_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /**< Multiply factor applied before rounding and
                          saturation (config, > 0); set by create and
                          replaced only by set_state(). */
    uint8_t clipped; /**< Sticky clip flag (running state): 1 once any
                          scaled sample fell outside -32768..32767,
                          else 0; cleared only by reset(). */
} dp_f32_to_uq15_state_t;

/**
 * @brief Create a f32_to_uq15 instance.
 *
 * Stores @p scale and initialises the sticky @c clipped flag to 0.
 *
 * @param scale  Multiply factor applied before quantisation and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert normalised
 *               `[-1, +1]` floats to the full UQ15 range `[0, 65535]`.
 *               Must be positive and finite; NULL otherwise.
 * @return Heap-allocated state, or NULL on invalid args or allocation failure.
 * @note Caller must call dp_f32_to_uq15_destroy() when done.
 */
dp_f32_to_uq15_state_t *dp_f32_to_uq15_create(float scale);

/**
 * @brief Destroy a f32_to_uq15 instance and release all memory.
 * @param state  May be NULL.
 */
void dp_f32_to_uq15_destroy(dp_f32_to_uq15_state_t *state);

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
 * >>> from doppler.cvt import F32ToUQ15
 * >>> c = F32ToUQ15()
 * >>> c.step(2.0)       # out of range -> saturates 0xFFFF, latches
 * 65535
 * >>> c.reset()            # forget the clip history
 * >>> c.clipped
 * False
 *
 * @endcode
 */
void dp_f32_to_uq15_reset(dp_f32_to_uq15_state_t *state);

/**
 * @brief Scale one float sample to an offset-binary UQ15 uint16 code.
 *
 * Computes @c round(x * scale), clamps to `[-32768, 32767]`, then adds the
 * 32768 offset-binary bias so the signed float domain maps onto the full
 * unsigned uint16 range. Latches the sticky @c clipped flag if the scaled
 * value saturated before clamping. Suits DAC and file formats that store only
 * unsigned integers.
 *
 * A NaN input counts as clipped: it latches @c clipped and returns the
 * negative full-scale code, 0 (offset-binary -32768).
 *
 * @param state  Must be non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Offset-binary uint16 in `[0, 65535]`: -1.0 -> 0, 0.0 -> 32768,
 *         +1.0 -> 65535.
 *
 * @code
 * >>> from doppler.cvt import F32ToUQ15
 * >>> c = F32ToUQ15(scale=32768.0)
 * >>> c.step(0.0)          # midscale maps to the offset-binary bias
 * 32768
 * >>> c.step(-1.0)         # full-negative maps to code 0
 * 0
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT uint16_t
dp_f32_to_uq15_step(dp_f32_to_uq15_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    /* Written as "not inside" so a NaN, which fails every comparison,
     * latches the flag too (fmaxf then maps it to the lower bound). */
    state->clipped |= (uint8_t)!(s <= 32767.0f && s >= -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    /* Shift Q15 to offset-binary: 0 → 32768, -32768 → 0, 32767 → 65535 */
    return (uint16_t)((int32_t)v + 32768);
}

/**
 * @brief Process a block of float samples to UQ15 uint16.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block.  The C function writes into the
 * caller's @p output, which must be non-NULL; the Python binding allocates
 * a fresh array when no output array is passed.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output uint16 offset-binary array; must contain at least
 *                @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import F32ToUQ15
 * >>> import numpy as np
 * >>> F32ToUQ15().steps(
 * ...     np.array([-1.0, 0.0, 0.999], dtype=np.float32)).tolist()
 * [0, 32768, 65503]
 *
 * @endcode
 */
void dp_f32_to_uq15_steps(
    dp_f32_to_uq15_state_t *state,
    const float    *input,
    uint16_t          *output,
    size_t               n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
/** @brief Blob magic ("FU15") that dp_f32_to_uq15_set_state() requires. */
#define F32_TO_UQ15_STATE_MAGIC DP_FOURCC ('F','U','1','5')
/** @brief Blob layout version; a blob of any other version is rejected. */
#define F32_TO_UQ15_STATE_VERSION 1u
/**
 * @brief Size in bytes of the blob dp_f32_to_uq15_get_state() writes.
 * @param state  Must be non-NULL (its contents do not affect the size).
 * @return sizeof(dp_state_hdr_t) + sizeof(dp_f32_to_uq15_state_t).
 */
size_t dp_f32_to_uq15_state_bytes (const dp_f32_to_uq15_state_t *state);
/**
 * @brief Serialize the converter (scale and clip flag) into @p blob.
 * @param state  Instance to snapshot; not modified.
 * @param[out] blob   Caller buffer of at least
 *               dp_f32_to_uq15_state_bytes() bytes.
 */
void dp_f32_to_uq15_get_state (const dp_f32_to_uq15_state_t *state, void *blob);
/**
 * @brief Restore the converter from a blob made by
 * dp_f32_to_uq15_get_state().
 *
 * The whole struct is copied, so the blob's @c scale replaces the one this
 * instance was created with.
 *
 * @param state  Instance to overwrite.
 * @param blob   Blob to restore from; its envelope (magic, version,
 *               endianness, size) is validated before anything is copied.
 * @return DP_OK on success; DP_ERR_INVALID if the envelope does not
 *         match, in which case @p state is left unchanged.
 */
int dp_f32_to_uq15_set_state (dp_f32_to_uq15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_UQ15_CORE_H */
