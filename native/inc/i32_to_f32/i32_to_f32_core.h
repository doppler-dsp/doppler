/**
 * @file i32_to_f32_core.h
 * @brief I32ToF32 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * i32_to_f32_state_t *obj = i32_to_f32_create(2147483648.0f);
 * float y = i32_to_f32_step(obj, 0);
 * i32_to_f32_destroy(obj);
 * @endcode
 */
#ifndef I32_TO_F32_CORE_H
#define I32_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I32ToF32 state.
 *
 * Allocate with i32_to_f32_create().
 */
typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i32_to_f32_state_t;

/**
 * @brief Create a i32_to_f32 instance.
 *
 * @param scale  scale (default: 2147483648.0f).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call i32_to_f32_destroy() when done.
 */
i32_to_f32_state_t *i32_to_f32_create(float scale);

/**
 * @brief Destroy a i32_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void i32_to_f32_destroy(i32_to_f32_state_t *state);

/**
 * @brief Reset I32ToF32 to its post-create state.
 * @param state  Must be non-NULL.
 */
void i32_to_f32_reset(i32_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (int32_t).
 * @return Output sample (float).
 */
JM_FORCEINLINE JM_HOT float
i32_to_f32_step(const i32_to_f32_state_t *state, int32_t x)
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
void i32_to_f32_steps(
    i32_to_f32_state_t *state,
    const int32_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I32_TO_F32_CORE_H */
