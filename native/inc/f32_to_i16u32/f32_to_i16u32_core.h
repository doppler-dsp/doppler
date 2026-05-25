/**
 * @file f32_to_i16u32_core.h
 * @brief Scale-and-saturate float to Q15-in-uint32 converter.
 *
 * Converts a float to a saturated int16, then zero-extends the 16-bit two's
 * complement representation into the lower 16 bits of a uint32 (upper 16
 * bits are always zero).  This is the format expected by the CIC filter's
 * integer input path, which uses the upper bits as headroom for bit-growth
 * through the integrator cascade.
 *
 *   input  +1.0 → int16  32767 → uint32 0x00007FFF
 *   input  -1.0 → int16 -32768 → uint32 0x00008000
 *
 * The default scale of 32768.0 maps `[-1, +1]` float to Q15 range.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * f32_to_i16u32_state_t *obj = f32_to_i16u32_create(32768.0f);
 * uint32_t y = f32_to_i16u32_step(obj, -1.0f);  // y == 0x00008000
 * f32_to_i16u32_destroy(obj);
 * @endcode
 */
#ifndef F32_TO_I16U32_CORE_H
#define F32_TO_I16U32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToI16U32 state.
 *
 * Allocate with f32_to_i16u32_create().
 */
typedef struct {
    float scale; /* multiply factor applied before saturation */
} f32_to_i16u32_state_t;

/**
 * @brief Create a f32_to_i16u32 instance.
 *
 * @param scale  scale (default: 32768.0f).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call f32_to_i16u32_destroy() when done.
 */
f32_to_i16u32_state_t *f32_to_i16u32_create(float scale);

/**
 * @brief Destroy a f32_to_i16u32 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_i16u32_destroy(f32_to_i16u32_state_t *state);

/**
 * @brief Reset f32_to_i16u32 to its post-create state.
 * @param state  Must be non-NULL.
 */
void f32_to_i16u32_reset(f32_to_i16u32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (float).
 * @return Output sample (uint32_t).
 */
JM_FORCEINLINE JM_HOT uint32_t
f32_to_i16u32_step(const f32_to_i16u32_state_t *state, float x)
{
    /* Saturate to int16, then zero-extend into the lower 16 bits of uint32. */
    float s = state->scale * x;
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint32_t)(uint16_t)v;
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
 */
void f32_to_i16u32_steps(
    f32_to_i16u32_state_t *state,
    const float    *input,
    uint32_t          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* F32_TO_I16U32_CORE_H */
