/**
 * @file f32_to_i16u32_core.h
 * @brief Scale-and-saturate float to Q15-in-uint32 converter.
 *
 * Converts a float to a saturated int16, then zero-extends the 16-bit two's
 * complement bit pattern into the lower 16 bits of a uint32 (upper 16 bits
 * are always zero).  The upper bits leave room for a consumer that
 * accumulates in place.  The CIC does not take this format: it takes CF32
 * and encodes to UQ16 internally (`cic_core.h`).
 *
 *   input  +1.0 → int16  32767 → uint32 0x00007FFF
 *   input  -1.0 → int16 -32768 → uint32 0x00008000
 *
 * The default scale of 32768.0 maps `[-1, +1]` float to Q15 range.  A sticky
 * @c clipped flag is raised on saturation and cleared only by reset().
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToI16U32
 * >>> import numpy as np
 * >>> obj = F32ToI16U32(scale=32768.0)
 * >>> hex(obj.step(-1.0))
 * '0x8000'
 * >>> hex(obj.step(1.0))
 * '0x7fff'
 * >>> obj.step(0.0)
 * 0
 * >>> x = np.array([-1.0, 0.0, 1.0], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [32768, 0, 32767]
 * @endcode
 */
#ifndef DP_F32_TO_I16U32_CORE_H
#define DP_F32_TO_I16U32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI16U32 state.
 *
 * Allocate with dp_f32_to_i16u32_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /**< Multiply factor applied before saturation; set
                          by create (> 0), replaced only by set_state() */
    uint8_t clipped; /**< Sticky: 1 once any sample has been saturated,
                          0 otherwise; written by step/steps, cleared
                          by reset (running state) */
} dp_f32_to_i16u32_state_t;

/**
 * @brief Create a f32_to_i16u32 instance.
 *
 * Stores @p scale and initialises the sticky @c clipped flag to 0.
 *
 * @param scale  Multiply factor applied before quantisation and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert normalised
 *               `[-1, +1]` samples to Q15 packed into a uint32.  Must be
 *               positive and finite.
 * @return Heap-allocated state, or NULL if @p scale is not positive and
 *         finite, or allocation fails.
 * @note Caller must call dp_f32_to_i16u32_destroy() when done.
 */
dp_f32_to_i16u32_state_t *dp_f32_to_i16u32_create(float scale);

/**
 * @brief Destroy a f32_to_i16u32 instance and release all memory.
 * @param state  May be NULL.
 */
void dp_f32_to_i16u32_destroy(dp_f32_to_i16u32_state_t *state);

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
 * >>> from doppler.cvt import F32ToI16U32
 * >>> c = F32ToI16U32()
 * >>> c.step(5.0)          # out of range -> saturates, latches clipped
 * 32767
 * >>> c.reset()            # forget the clip history
 * >>> c.clipped
 * False
 *
 * @endcode
 */
void dp_f32_to_i16u32_reset(dp_f32_to_i16u32_state_t *state);

/**
 * @brief Scale one float sample to a saturated Q15 code packed in a uint32.
 *
 * Computes @c round(x * scale), saturates to `[-32768, 32767]`, then
 * zero-extends the 16-bit two's-complement pattern into the lower 16 bits of a
 * uint32 (upper 16 bits are always zero). Latches the sticky @c clipped
 * flag on saturation. A NaN input maps to 0x8000 (-32768) without
 * setting @c clipped.
 *
 * A NaN input counts as clipped: it latches @c clipped and returns the
 * negative full-scale code, -32768 (0x8000).
 *
 * @param state  Must be non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Q15 code in the low 16 bits of a uint32; e.g. -32768 -> 0x8000.
 *
 * @code
 * >>> from doppler.cvt import F32ToI16U32
 * >>> c = F32ToI16U32(scale=32768.0)
 * >>> c.step(0.5)              # 0.5 -> Q15 16384, upper 16 bits zero
 * 16384
 * >>> hex(c.step(-1.0))        # -32768 as an unsigned low-16 pattern
 * '0x8000'
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT uint32_t
dp_f32_to_i16u32_step(dp_f32_to_i16u32_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    /* Written as "not inside" so a NaN, which fails every comparison,
     * latches the flag too (fmaxf then maps it to the lower bound). */
    state->clipped |= (uint8_t)!(s <= 32767.0f && s >= -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint32_t)(uint16_t)v;
}

/**
 * @brief Process a block of float samples to Q15-in-uint32.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block.  In C the caller supplies @p output; the
 * Python binding accepts an optional pre-allocated @c out array and
 * allocates a fresh one when it is omitted.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param[out] output  Output uint32 array; must be non-NULL and contain at
 *                least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import F32ToI16U32
 * >>> import numpy as np
 * >>> F32ToI16U32().steps(
 * ...     np.array([0.0, 0.5], dtype=np.float32)).tolist()
 * [0, 16384]
 *
 * @endcode
 */
void dp_f32_to_i16u32_steps(
    dp_f32_to_i16u32_state_t *state,
    const float    *input,
    uint32_t          *output,
    size_t               n);





/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the sticky clip flag resumes exactly into an
 * identically-built instance. */
/** Envelope magic ("FU32") that tags an f32_to_i16u32 state blob. */
#define F32_TO_I16U32_STATE_MAGIC DP_FOURCC ('F','U','3','2')
/** Payload layout version of an f32_to_i16u32 state blob. */
#define F32_TO_I16U32_STATE_VERSION 1u
/**
 * @brief Bytes dp_f32_to_i16u32_get_state() writes (envelope + payload).
 * @param state  State; not read (the size is fixed).
 * @return sizeof(dp_state_hdr_t) + sizeof(dp_f32_to_i16u32_state_t).
 */
size_t dp_f32_to_i16u32_state_bytes (const dp_f32_to_i16u32_state_t *state);
/**
 * @brief Snapshot the whole state struct (scale and clipped) into @p blob.
 * @param state  State.  Must be non-NULL.  Not modified.
 * @param[out] blob   Caller-owned buffer of at least
 *               dp_f32_to_i16u32_state_bytes() bytes.
 */
void dp_f32_to_i16u32_get_state (const dp_f32_to_i16u32_state_t *state, void *blob);
/**
 * @brief Restore the whole state struct from @p blob.
 *
 * The envelope (magic, version, endianness, size) is validated first; on
 * success both @c scale and @c clipped are overwritten from the blob, so
 * the instance takes the source's scale too.
 *
 * @param state  State.  Must be non-NULL; unchanged on rejection.
 * @param blob   Blob from dp_f32_to_i16u32_get_state().
 * @return DP_OK, or DP_ERR_INVALID if the envelope rejects.
 */
int dp_f32_to_i16u32_set_state (dp_f32_to_i16u32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16U32_CORE_H */
