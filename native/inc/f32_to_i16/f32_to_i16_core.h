/**
 * @file f32_to_i16_core.h
 * @brief Scale-and-saturate float-to-int16 converter.
 *
 * Multiplies by @c scale then round-to-nearest and clamps to `[-32768, 32767]`.
 * The default scale of 32768.0 converts a `[-1, +1]` normalised float to full
 * Q15 range.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * f32_to_i16_state_t *obj = f32_to_i16_create(32768.0f);
 * int16_t y = f32_to_i16_step(obj, 1.0f);  // y == 32767
 * f32_to_i16_destroy(obj);
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
 * @param scale  scale (default: 32768.0f).
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
 * @param state  Must be non-NULL.
 */
void f32_to_i16_reset(f32_to_i16_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (float).
 * @return Output sample (int16_t).
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
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
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
