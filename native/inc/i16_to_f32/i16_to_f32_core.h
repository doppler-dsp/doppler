/**
 * @file i16_to_f32_core.h
 * @brief int16-to-float converter with configurable inverse scale.
 *
 * Multiplies the signed int16 sample by @c 1/scale. The default scale of
 * 32768.0 maps the full Q15 range [-32768, 32767] into [-1.0, ~1.0), which
 * is the inverse of F32ToI16 with its default scale.
 *
 * The inverse scale is pre-computed at construction time so the step path
 * is a single multiply.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * i16_to_f32_state_t *obj = i16_to_f32_create(32768.0f);
 * float y = i16_to_f32_step(obj, -32768);  // y == -1.0f
 * i16_to_f32_destroy(obj);
 * @endcode
 */
#ifndef I16_TO_F32_CORE_H
#define I16_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I16ToF32 state.
 *
 * Allocate with i16_to_f32_create().
 */
typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i16_to_f32_state_t;

/**
 * @brief Create a i16_to_f32 instance.
 *
 * @param scale  scale (default: 32768.0f).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call i16_to_f32_destroy() when done.
 */
i16_to_f32_state_t *i16_to_f32_create(float scale);

/**
 * @brief Destroy a i16_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void i16_to_f32_destroy(i16_to_f32_state_t *state);

/**
 * @brief Reset i16_to_f32 to its post-create state.
 * @param state  Must be non-NULL.
 */
void i16_to_f32_reset(i16_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (int16_t).
 * @return Output sample (float).
 */
JM_FORCEINLINE JM_HOT float
i16_to_f32_step(const i16_to_f32_state_t *state, int16_t x)
{
    return (float)x * state->iscale;
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
 */
void i16_to_f32_steps(
    i16_to_f32_state_t *state,
    const int16_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I16_TO_F32_CORE_H */
