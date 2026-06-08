/**
 * @file i8_to_f32_core.h
 * @brief int8-to-float converter with configurable inverse scale.
 *
 * Multiplies each signed int8 sample by @c 1/scale and returns a float32.
 * The default scale of 128.0 maps the full int8 range [-128, 127] to
 * [-1.0, ~+1.0), which is the natural inverse of an 8-bit ADC path.
 * This converter is used in the 8-bit IQ sample pipeline (e.g., RTL-SDR
 * signed-8 I/Q streams) where samples arrive as int8 and must be converted
 * to normalised complex floats.
 * The inverse scale is pre-computed at construction time.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * @code
 * >>> from doppler.cvt import I8ToF32
 * >>> import numpy as np
 * >>> obj = I8ToF32(scale=128.0)
 * >>> float(obj.step(-128))
 * -1.0
 * >>> float(obj.step(0))
 * 0.0
 * >>> x = np.array([-128, 0, 127], dtype=np.int8)
 * >>> [round(v, 7) for v in obj.steps(x).tolist()]
 * [-1.0, 0.0, 0.9921875]
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
 * Pre-computes @c iscale = 1.0f / @p scale.
 *
 * @param scale  Denominator scale; 1/scale is applied to each sample
 *               (default: 128.0f).  Use 128.0 to recover normalised floats
 *               from a signed 8-bit stream.
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
 *
 * No mutable state exists beyond the immutable @c iscale; reset is a no-op
 * provided for lifecycle symmetry with other converters.
 *
 * @param state  Must be non-NULL.
 */
void i8_to_f32_reset(i8_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Returns @c (float)x * iscale.
 *
 * @param state  Must be non-NULL.
 * @param x      Signed int8 input sample.
 * @return Scaled float32 output.
 */
JM_FORCEINLINE JM_HOT float
i8_to_f32_step(const i8_to_f32_state_t *state, int8_t x)
{
    return (float)x * state->iscale;
}

/**
 * @brief Process a block of int8 samples to float32.
 *
 * Applies step() to every element.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input int8 array; must contain at least @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
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
