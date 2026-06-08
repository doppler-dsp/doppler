/**
 * @file f32_to_i16u64_core.h
 * @brief Scale-and-saturate float to Q15-in-uint64 converter.
 *
 * Identical semantics to F32ToI16U32 but the zero-extended Q15 result
 * occupies the lower 16 bits of a uint64, providing 48 bits of upper
 * headroom.  This is the input format for the NCO's uint64 phase accumulator
 * pipeline, where the upper bits carry phase increment headroom across
 * accumulations.
 *
 *   input  +1.0 → int16  32767 → uint64 0x0000000000007FFF
 *   input  -1.0 → int16 -32768 → uint64 0x0000000000008000
 *
 * The default scale of 32768.0 maps [-1, +1] float to Q15 range.  A sticky
 * @c clipped flag is raised on saturation and cleared only by reset().
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import F32ToI16U64
 * >>> import numpy as np
 * >>> obj = F32ToI16U64(scale=32768.0)
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
#ifndef F32_TO_I16U64_CORE_H
#define F32_TO_I16U64_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI16U64 state.
 *
 * Allocate with f32_to_i16u64_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_i16u64_state_t;

/**
 * @brief Create a f32_to_i16u64 instance.
 *
 * Stores @p scale and initialises the sticky @c clipped flag to 0.
 *
 * @param scale  Multiply factor applied before quantisation and saturation
 *               (default: 32768.0f).  Use 32768.0 to convert normalised
 *               [-1, +1] samples to Q15 packed into the low 16 bits of
 *               a uint64.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call f32_to_i16u64_destroy() when done.
 */
f32_to_i16u64_state_t *f32_to_i16u64_create(float scale);

/**
 * @brief Destroy a f32_to_i16u64 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_i16u64_destroy(f32_to_i16u64_state_t *state);

/**
 * @brief Reset f32_to_i16u64 to its post-create state.
 *
 * Clears the sticky @c clipped flag.  The @c scale is preserved.
 *
 * @param state  Must be non-NULL.
 */
void f32_to_i16u64_reset(f32_to_i16u64_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Computes @c round(x * scale), saturates to [-32768, 32767], then
 * zero-extends the int16 bit pattern into the lower 16 bits of a uint64.
 * The @c clipped flag is set if saturation occurred.
 *
 * @param state  Must be non-NULL.
 * @param x      Normalised float input sample.
 * @return Q15 value packed into the lower 16 bits of a uint64.
 */
JM_FORCEINLINE JM_HOT uint64_t
f32_to_i16u64_step(f32_to_i16u64_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint64_t)(uint16_t)v;
}

/**
 * @brief Process a block of float samples to Q15-in-uint64.
 *
 * Applies step() to every element.  The @c clipped flag is updated
 * cumulatively across the block.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output uint64 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 */
void f32_to_i16u64_steps(
    f32_to_i16u64_state_t *state,
    const float    *input,
    uint64_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16U64_CORE_H */
