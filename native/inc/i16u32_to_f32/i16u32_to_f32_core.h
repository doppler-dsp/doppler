/**
 * @file i16u32_to_f32_core.h
 * @brief I16U32ToF32 component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * i16u32_to_f32_state_t *obj = i16u32_to_f32_create(32768.0f);
 * float y = i16u32_to_f32_step(obj, 0U);
 * i16u32_to_f32_destroy(obj);
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
 * @param scale  scale (default: 32768.0f).
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
 * @param state  Must be non-NULL.
 */
void i16u32_to_f32_reset(i16u32_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      Input sample (uint32_t).
 * @return Output sample (float).
 */
JM_FORCEINLINE JM_HOT float
i16u32_to_f32_step(const i16u32_to_f32_state_t *state, uint32_t x)
{
    /* Extract lower 16 bits as signed int16, then scale to float. */
    int16_t v = (int16_t)(uint16_t)(x & 0xFFFFu);
    return (float)v * state->iscale;
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
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
