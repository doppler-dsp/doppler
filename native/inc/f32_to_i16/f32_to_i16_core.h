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
#ifndef F32_TO_I16_CORE_H
#define F32_TO_I16_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI16 state.
 *
 * Allocate with f32_to_i16_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_i16_state_t;

/**
 * @brief Create a f32_to_i16 instance.
 *
 * Allocates state and stores @p scale.  The @c clipped flag is initialised
 * to 0.  Returns NULL only on malloc failure; no parameter validation is
 * performed (any finite float is a valid scale).
 *
 * @param scale  Multiply factor applied before rounding and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert a normalised
 *               `[-1, +1]` signal to full Q15 range.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call f32_to_i16_destroy() when done.
 */
f32_to_i16_state_t *f32_to_i16_create(float scale);

/**
 * @brief Destroy a f32_to_i16 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_i16_destroy(f32_to_i16_state_t *state);

/**
 * @brief Reset f32_to_i16 to its post-create state.
 *
 * Clears the sticky @c clipped flag.  The @c scale is preserved.
 *
 * @param state  Must be non-NULL.
 */
void f32_to_i16_reset(f32_to_i16_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Computes @c round(x * scale), saturates to `[-32768, 32767]`, and sets the
 * sticky @c clipped flag if saturation occurred.
 *
 * @param state  Must be non-NULL.
 * @param x      Normalised float input sample.
 * @return Saturated int16 output in `[-32768, 32767]`.
 */
JM_FORCEINLINE JM_HOT int16_t
f32_to_i16_step(f32_to_i16_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    return (int16_t)lroundf(s);
}

/**
 * @brief Process a block of float samples to int16.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block — a single saturating sample raises it
 * for the entire call.  Accepts an optional pre-allocated output array;
 * allocates a fresh one when @p output is NULL.
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
 * >>> F32ToI16().steps(x).tolist()   # default scale=32768 -> full-scale int16
 * [0, 16384, -32768, 32735]
 *
 * @endcode
 */
void f32_to_i16_steps(
    f32_to_i16_state_t *state,
    const float    *input,
    int16_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16_CORE_H */
