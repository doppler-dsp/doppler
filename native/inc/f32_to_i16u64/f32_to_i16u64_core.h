/**
 * @file f32_to_i16u64_core.h
 * @brief Scale-and-saturate float to Q15-in-uint64 converter.
 *
 * Identical semantics to F32ToI16U32 but the zero-extended result occupies
 * the lower 16 bits of a uint64, providing 48 bits of upper headroom.  This
 * is the input format for the NCO's uint64 phase accumulator pipeline, where
 * the upper bits carry phase increment headroom across accumulations.
 *
 *   input  +1.0 → int16  32767 → uint64 0x0000000000007FFF
 *   input  -1.0 → int16 -32768 → uint64 0x0000000000008000
 *
 * The default scale of 32768.0 maps [-1, +1] float to Q15 range.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * f32_to_i16u64_state_t *obj = f32_to_i16u64_create(32768.0f);
 * uint64_t y = f32_to_i16u64_step(obj, -1.0f);  // y == 0x0000000000008000
 * f32_to_i16u64_destroy(obj);
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
 */
typedef struct {
    float scale; /* multiply factor applied before saturation */
} f32_to_i16u64_state_t;

/**
 * @brief Create a f32_to_i16u64 instance.
 *
 * @param scale  scale (default: 32768.0f).
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
 * @param state  Must be non-NULL.
 */
void f32_to_i16u64_reset(f32_to_i16u64_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (float).
 * @return Output sample (uint64_t).
 */
JM_FORCEINLINE JM_HOT uint64_t
f32_to_i16u64_step(const f32_to_i16u64_state_t *state, float x)
{
    /* Saturate to int16, then zero-extend into the lower 16 bits of uint64. */
    float s = state->scale * x;
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    return (uint64_t)(uint16_t)v;
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
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
