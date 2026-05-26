/**
 * @file uq15_to_f32_core.h
 * @brief UQ15 (offset-binary uint16) to float converter.
 *
 * Decodes an offset-binary uint16 (UQ15) sample back to a normalised float:
 *
 * @code
 *   x̂ = ((int32_t)u - 32768) * (1 / scale)
 * @endcode
 *
 * This is the exact inverse of F32ToUQ15 with the same scale.
 *
 * The inverse scale is pre-computed at construction time so the step path
 * is a single subtract and multiply.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * uq15_to_f32_state_t *obj = uq15_to_f32_create(32768.0f);
 * float y = uq15_to_f32_step(obj, 32768U);  // y == 0.0f  (mid-scale)
 * float z = uq15_to_f32_step(obj, 0U);      // z == -1.0f (negative FS)
 * uq15_to_f32_destroy(obj);
 * @endcode
 */
#ifndef UQ15_TO_F32_CORE_H
#define UQ15_TO_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UQ15ToF32 state.
 *
 * Allocate with uq15_to_f32_create().
 */
typedef struct {
    float iscale; /* 1.0f / scale, pre-computed for single-multiply step */
} uq15_to_f32_state_t;

/**
 * @brief Create a uq15_to_f32 instance.
 *
 * @param scale  Denominator scale applied after bias removal (default: 32768.0f).
 *               Must be > 0; returns NULL otherwise.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call uq15_to_f32_destroy() when done.
 */
uq15_to_f32_state_t *uq15_to_f32_create(float scale);

/**
 * @brief Destroy a uq15_to_f32 instance and release all memory.
 * @param state  May be NULL.
 */
void uq15_to_f32_destroy(uq15_to_f32_state_t *state);

/**
 * @brief Reset uq15_to_f32 to its post-create state.
 * @param state  Must be non-NULL.
 */
void uq15_to_f32_reset(uq15_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 * @param state  Must be non-NULL.
 * @param x      UQ15 offset-binary sample (uint16_t):
 *               0x0000 → -1.0f, 0x8000 → 0.0f, 0xFFFF → +32767/32768.
 * @return Decoded float sample.
 */
JM_FORCEINLINE JM_HOT float
uq15_to_f32_step(const uq15_to_f32_state_t *state, uint16_t x)
{
    /* Remove offset-binary bias in int32_t to avoid UB from int16 overflow */
    return (float)((int32_t)x - 32768) * state->iscale;
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (not mutated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
 */
void uq15_to_f32_steps(
    uq15_to_f32_state_t *state,
    const uint16_t    *input,
    float          *output,
    size_t               n);

#ifdef __cplusplus
}
#endif

#endif /* UQ15_TO_F32_CORE_H */
