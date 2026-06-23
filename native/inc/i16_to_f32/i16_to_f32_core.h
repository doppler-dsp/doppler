/**
 * @file i16_to_f32_core.h
 * @brief int16-to-float converter with configurable inverse scale.
 *
 * Multiplies the signed int16 sample by @c 1/scale.  The default scale of
 * 32768.0 maps the full Q15 range `[-32768, 32767]` to `[-1.0, ~+1.0)`, making
 * it the exact inverse of F32ToI16 at its default scale.
 * The inverse scale is pre-computed at construction time so each step is
 * a single multiply with no division on the hot path.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import I16ToF32
 * >>> import numpy as np
 * >>> obj = I16ToF32(scale=32768.0)
 * >>> float(obj.step(-32768))
 * -1.0
 * >>> float(obj.step(0))
 * 0.0
 * >>> x = np.array([-32768, 0, 32767], dtype=np.int16)
 * >>> [round(v, 6) for v in obj.steps(x).tolist()]
 * [-1.0, 0.0, 0.999969]
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
 * Pre-computes @c iscale = 1.0f / @p scale so the hot step path is a
 * single multiply.  Any non-zero finite float is a valid scale value.
 *
 * @param scale  Denominator scale; 1/scale is applied to each sample
 *               (default: 32768.0f).  Use 32768.0 to recover normalised
 *               `[-1, +1]` floats from a Q15 int16 stream.
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
 *
 * This converter has no accumulating state beyond the immutable @c iscale
 * field, so reset is a no-op in practice; it exists for lifecycle symmetry.
 *
 * @param state  Must be non-NULL.
 */
void i16_to_f32_reset(i16_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Returns @c (float)x * iscale.  No saturation or clipping possible — the
 * int16 range maps cleanly to float32.
 *
 * @param state  Must be non-NULL.
 * @param x      Signed int16 input sample.
 * @return Scaled float output.
 */
JM_FORCEINLINE JM_HOT float
i16_to_f32_step(const i16_to_f32_state_t *state, int16_t x)
{
    return (float)x * state->iscale;
}

/**
 * @brief Process a block of int16 samples to float32.
 *
 * Applies step() to every element.  Accepts an optional pre-allocated output
 * array; allocates a fresh one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input int16 array; must contain at least @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import I16ToF32
 * >>> import numpy as np
 * >>> I16ToF32().steps(np.array([0, 16384, -32768], dtype=np.int16)).tolist()
 * [0.0, 0.5, -1.0]
 *
 * @endcode
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
