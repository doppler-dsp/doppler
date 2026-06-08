/**
 * @file i8_to_f32_core.h
 * @brief I8ToF32 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * i8_to_f32_state_t *obj = i8_to_f32_create(128.0f);
 * float y = i8_to_f32_step(obj, 0);
 * i8_to_f32_destroy(obj);
 * @endcode
 */
#ifndef I8_TO_F32_CORE_H
#define I8_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I8ToF32 state.
 *
 * Allocate with i8_to_f32_create().
 */
typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} i8_to_f32_state_t;

/**
 * @brief Create a i8_to_f32 instance.
 *
 * @param scale  scale (default: 128.0f).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call i8_to_f32_destroy() when done.
 */
i8_to_f32_state_t *i8_to_f32_create(float scale);

/**
 * @brief Destroy a i8_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void i8_to_f32_destroy(i8_to_f32_state_t *state);

/**
 * @brief Reset I8ToF32 to its post-create state.
 * @param state  Must be non-NULL.
 */
void i8_to_f32_reset(i8_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (int8_t).
 * @return Output sample (float).
 */
JM_FORCEINLINE JM_HOT float
i8_to_f32_step(const i8_to_f32_state_t *state, int8_t x)
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
void i8_to_f32_steps(
    i8_to_f32_state_t *state,
    const int8_t    *input,
    float          *output,
    size_t               n);





#ifdef __cplusplus
}
#endif

#endif /* I8_TO_F32_CORE_H */
