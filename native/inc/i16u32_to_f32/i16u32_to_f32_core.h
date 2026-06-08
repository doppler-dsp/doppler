/**
 * @file i16u32_to_f32_core.h
 * @brief Q15-in-uint32 to float converter.
 *
 * Extracts the lower 16 bits of a uint32, re-interprets them as a signed
 * int16 (two's complement), then multiplies by @c 1/scale to produce a
 * normalised float.  This is the exact inverse of F32ToI16U32: a value
 * written by that converter can be recovered here with the same scale.
 *
 *   uint32 0x00008000 → int16 -32768 → float -1.0
 *   uint32 0x00007FFF → int16  32767 → float ~+1.0
 *   uint32 0x00000000 → int16      0 → float  0.0
 *
 * Upper 16 bits of the uint32 are masked off and ignored, so values
 * carrying CIC bit-growth headroom in those bits are handled correctly.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import I16U32ToF32
 * >>> import numpy as np
 * >>> obj = I16U32ToF32(scale=32768.0)
 * >>> float(obj.step(0x8000))
 * -1.0
 * >>> float(obj.step(0x0000))
 * 0.0
 * >>> x = np.array([0x8000, 0x0000, 0x7FFF], dtype=np.uint32)
 * >>> [round(v, 6) for v in obj.steps(x).tolist()]
 * [-1.0, 0.0, 0.999969]
 * @endcode
 */
#ifndef I16U32_TO_F32_CORE_H
#define I16U32_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I16U32ToF32 state.
 *
 * Allocate with i16u32_to_f32_create().
 */
typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i16u32_to_f32_state_t;

/**
 * @brief Create a i16u32_to_f32 instance.
 *
 * Pre-computes @c iscale = 1.0f / @p scale so the hot step path is a
 * single multiply after the lower-16-bit extraction.
 *
 * @param scale  Denominator scale; 1/scale is applied after sign-extension
 *               (default: 32768.0f).  Use 32768.0 to match F32ToI16U32 at
 *               its default scale.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call i16u32_to_f32_destroy() when done.
 */
i16u32_to_f32_state_t *i16u32_to_f32_create(float scale);

/**
 * @brief Destroy a i16u32_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void i16u32_to_f32_destroy(i16u32_to_f32_state_t *state);

/**
 * @brief Reset i16u32_to_f32 to its post-create state.
 *
 * No mutable state exists beyond the immutable @c iscale; reset is a no-op
 * provided for lifecycle symmetry.
 *
 * @param state  Must be non-NULL.
 */
void i16u32_to_f32_reset(i16u32_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Masks the lower 16 bits, sign-extends to int16, then multiplies by iscale.
 * Upper 16 bits are ignored.
 *
 * @param state  Must be non-NULL.
 * @param x      uint32 carrying a Q15 sample in its lower 16 bits.
 * @return Scaled float32 output.
 */
JM_FORCEINLINE JM_HOT float
i16u32_to_f32_step(const i16u32_to_f32_state_t *state, uint32_t x)
{
    /* Extract lower 16 bits as signed int16, then scale to float. */
    int16_t v = (int16_t)(uint16_t)(x & 0xFFFFu);
    return (float)v * state->iscale;
}

/**
 * @brief Process a block of Q15-in-uint32 samples to float32.
 *
 * Applies step() to every element.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input uint32 array (Q15 packed in lower 16 bits);
 *                must contain at least @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 */
void i16u32_to_f32_steps(
    i16u32_to_f32_state_t *state,
    const uint32_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16U32_TO_F32_CORE_H */
