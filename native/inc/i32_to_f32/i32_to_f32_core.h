/**
 * @file i32_to_f32_core.h
 * @brief int32-to-float converter with configurable inverse scale.
 *
 * Multiplies each int32 sample by @c 1/scale and returns a float32 result.
 * The default scale of 2147483648.0 (2^31) maps the full int32 range
 * `[-2147483648, 2147483647]` to `[-1.0, ~+1.0)`, recovering the normalised
 * float representation from a 32-bit fixed-point stream.
 * Note: float32 has 23 mantissa bits, so int32 values beyond ±16777217
 * will be rounded to the nearest representable float.  Use I32ToF32 when
 * only the magnitude matters or the source is genuinely 32-bit fixed-point.
 * The inverse scale is pre-computed at construction time.
 *
 * Lifecycle: create -> `[step / steps / reset]*` -> destroy
 *
 * @code
 * >>> from doppler.cvt import I32ToF32
 * >>> import numpy as np
 * >>> obj = I32ToF32(scale=2147483648.0)
 * >>> float(obj.step(-2147483648))
 * -1.0
 * >>> float(obj.step(0))
 * 0.0
 * >>> x = np.array([-2147483648, 0, 2147483647], dtype=np.int32)
 * >>> obj.steps(x).tolist()
 * [-1.0, 0.0, 1.0]
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
 * Pre-computes @c iscale = 1.0f / @p scale.  Any non-zero finite float is a
 * valid scale.
 *
 * @param scale  Denominator scale; 1/scale is applied to each sample
 *               (default: 2147483648.0f).  Use 2^31 to recover normalised
 *               floats from a full-range int32 stream.
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
 *
 * No mutable state exists beyond the immutable @c iscale; reset is a no-op
 * provided for lifecycle symmetry with other converters.
 *
 * @param state  Must be non-NULL.
 */
void i32_to_f32_reset(i32_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Returns @c (float)x * iscale.
 *
 * @param state  Must be non-NULL.
 * @param x      Signed int32 input sample.
 * @return Scaled float32 output.
 */
JM_FORCEINLINE JM_HOT float
i32_to_f32_step(const i32_to_f32_state_t *state, int32_t x)
{
    return (float)x * state->iscale;
}

/**
 * @brief Process a block of int32 samples to float32.
 *
 * Applies step() to every element.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input int32 array; must contain at least @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import I32ToF32
 * >>> import numpy as np
 * >>> I32ToF32().steps(np.array([0, 2**30, -2**31], dtype=np.int32)).tolist()
 * [0.0, 0.5, -1.0]
 *
 * @endcode
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
