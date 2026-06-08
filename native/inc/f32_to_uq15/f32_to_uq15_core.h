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
 * This is the unsigned wire format used by some DAC and file-container
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
#ifndef F32_TO_UQ15_CORE_H
#define F32_TO_UQ15_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToUQ15 state.
 *
 * Allocate with f32_to_uq15_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_uq15_state_t;

/**
 * @brief Create a f32_to_uq15 instance.
 *
 * Stores @p scale and initialises the sticky @c clipped flag to 0.
 *
 * @param scale  Multiply factor applied before quantisation and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert normalised
 *               [-1, +1] floats to the full UQ15 range [0, 65535].
 *               Must be > 0; returns NULL otherwise.
 * @return Heap-allocated state, or NULL on invalid args or allocation failure.
 * @note Caller must call f32_to_uq15_destroy() when done.
 */
f32_to_uq15_state_t *f32_to_uq15_create(float scale);

/**
 * @brief Destroy a f32_to_uq15 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_uq15_destroy(f32_to_uq15_state_t *state);

/**
 * @brief Reset f32_to_uq15 to its post-create state.
 *
 * Clears the sticky @c clipped flag.  The @c scale is preserved.
 *
 * @param state  Must be non-NULL.
 */
void f32_to_uq15_reset(f32_to_uq15_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Computes @c round(x * scale), clamps to [-32768, 32767], then adds 32768
 * to produce the offset-binary uint16 result.  Sets @c clipped if
 * saturation occurred before clamping.
 *
 * @param state  Must be non-NULL.
 * @param x      Normalised float input sample.
 * @return Offset-binary uint16 in [0, 65535]:
 *         x = -1.0 → 0x0000, x = 0.0 → 0x8000, x ≈ +1.0 → 0xFFFF.
 */
JM_FORCEINLINE JM_HOT uint16_t
f32_to_uq15_step(f32_to_uq15_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
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
 * cumulatively across the block.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output uint16 offset-binary array; must contain at least
 *                @p n elements.
 * @param n       Number of samples to process.
 */
void f32_to_uq15_steps(
    f32_to_uq15_state_t *state,
    const float    *input,
    uint16_t          *output,
    size_t               n);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_UQ15_CORE_H */
