/**
 * @file f32_to_uq15_core.h
 * @brief Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter.
 *
 * Converts a normalised float sample in [-1, +1] to offset-binary uint16
 * (UQ15 format): the Q15 quantised value is shifted by +32768 so that
 * -1.0 maps to 0, 0.0 maps to 32768, and +1.0 maps to 65535.
 *
 * Encoding:
 * @code
 *   v_Q15 = round(x * scale);            // Q15 in [-32768, 32767]
 *   u     = (uint16_t)((int32_t)v + 32768); // UQ15 in [0, 65535]
 * @endcode
 *
 * The @c clipped flag is set whenever the pre-saturation scaled value
 * falls outside [-32768, 32767].  It is sticky — cleared only by reset().
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * f32_to_uq15_state_t *obj = f32_to_uq15_create(32768.0f);
 * uint16_t y = f32_to_uq15_step(obj, 0.0f);  // y == 32768
 * uint16_t z = f32_to_uq15_step(obj, -1.0f); // z == 0
 * f32_to_uq15_destroy(obj);
 * @endcode
 */
#ifndef F32_TO_UQ15_CORE_H
#define F32_TO_UQ15_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief F32ToUQ15 state.
 *
 * Allocate with f32_to_uq15_create().
 *
 * @c clipped is sticky: set to 1 by the first sample whose pre-saturation
 * scaled value falls outside `[-32768, 32767]`; cleared only by reset().
 */
typedef struct {
    float   scale;   /* multiply factor applied before saturation */
    uint8_t clipped; /* 1 if any sample has been saturated; 0 otherwise */
} f32_to_uq15_state_t;

/**
 * @brief Create a f32_to_uq15 instance.
 *
 * @param scale  Multiply factor applied before quantisation (default: 32768.0f).
 *               Must be > 0; returns NULL otherwise.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call f32_to_uq15_destroy() when done.
 */
f32_to_uq15_state_t *f32_to_uq15_create(float scale);

/**
 * @brief Destroy a f32_to_uq15 instance and release all memory.
 * @param state  May be NULL.
 */
void f32_to_uq15_destroy(f32_to_uq15_state_t *state);

/**
 * @brief Reset f32_to_uq15 to its post-create state (clears clipped).
 * @param state  Must be non-NULL.
 */
void f32_to_uq15_reset(f32_to_uq15_state_t *state);

/**
 * @brief Process one input sample.
 *
 * @param state  Must be non-NULL.
 * @param x      Input sample (float).
 * @return UQ15 offset-binary encoded sample (uint16_t):
 *         x = -1.0 → 0x0000, x = 0.0 → 0x8000, x ≈ +1.0 → 0xFFFF.
 */
JM_FORCEINLINE JM_HOT uint16_t
f32_to_uq15_step(f32_to_uq15_state_t *state, float x)
{
    float s = state->scale * x;
    /* Detect saturation before clamping; set sticky flag. */
    state->clipped |= (uint8_t)(s > 32767.0f || s < -32768.0f);
    s = fmaxf(s, -32768.0f);
    s = fminf(s,  32767.0f);
    int16_t v = (int16_t)lroundf(s);
    /* Shift Q15 to offset-binary: 0 → 32768, -32768 → 0, 32767 → 65535 */
    return (uint16_t)((int32_t)v + 32768);
}

/**
 * @brief Process a block of samples.
 *
 * @param state   Component state (mutated; clipped flag updated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n; may alias input for in-place).
 * @param n       Number of samples.
 */
void f32_to_uq15_steps(
    f32_to_uq15_state_t *state,
    const float    *input,
    uint16_t          *output,
    size_t               n);

#ifdef __cplusplus
}
#endif

#endif /* F32_TO_UQ15_CORE_H */
