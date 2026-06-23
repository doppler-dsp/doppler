/**
 * @file uq15_to_f32_core.h
 * @brief UQ15 (offset-binary uint16) to float converter.
 *
 * Decodes an offset-binary uint16 (UQ15) sample back to a normalised float
 * by removing the +32768 bias and dividing by @p scale:
 *
 * @code
 *   x̂ = ((int32_t)u - 32768) * (1 / scale)
 * @endcode
 *
 * This is the exact inverse of F32ToUQ15 with the same scale.  The bias
 * removal uses int32_t arithmetic to avoid signed overflow for the u=0
 * (full-negative) case.  The inverse scale is pre-computed at construction
 * time so the step path is a single subtract and multiply.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import UQ15ToF32
 * >>> import numpy as np
 * >>> obj = UQ15ToF32(scale=32768.0)
 * >>> float(obj.step(32768))
 * 0.0
 * >>> float(obj.step(0))
 * -1.0
 * >>> x = np.array([0, 32768, 65535], dtype=np.uint16)
 * >>> [round(v, 6) for v in obj.steps(x).tolist()]
 * [-1.0, 0.0, 0.999969]
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
 * Pre-computes @c iscale = 1.0f / @p scale so the hot step path is a
 * single subtract and multiply.
 *
 * @param scale  Denominator applied after offset-binary bias removal
 *               (default: 32768.0f).  Use 32768.0 to recover normalised
 *               `[-1, +1]` floats from UQ15 data written by F32ToUQ15.
 *               Must be > 0; returns NULL otherwise.
 * @return Heap-allocated state, or NULL on invalid args or allocation failure.
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
 *
 * No mutable state exists beyond the immutable @c iscale; reset is a no-op
 * provided for lifecycle symmetry with other converters.
 *
 * @param state  Must be non-NULL.
 */
void uq15_to_f32_reset(uq15_to_f32_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Computes @c ((int32_t)x - 32768) * iscale.  The int32_t cast prevents
 * signed overflow when @p x is 0 (which yields -32768 after bias removal).
 *
 * @param state  Must be non-NULL.
 * @param x      UQ15 offset-binary uint16 sample:
 *               0x0000 → -1.0f, 0x8000 → 0.0f, 0xFFFF → +32767/32768.
 * @return Decoded float sample in `[-1.0, ~+1.0)`.
 */
JM_FORCEINLINE JM_HOT float
uq15_to_f32_step(const uq15_to_f32_state_t *state, uint16_t x)
{
    /* Remove offset-binary bias in int32_t to avoid UB from int16 overflow */
    return (float)((int32_t)x - 32768) * state->iscale;
}

/**
 * @brief Process a block of UQ15 samples to float32.
 *
 * Applies step() to every element.  State is not mutated (no clipped flag).
 * Accepts an optional pre-allocated output array; allocates a fresh one when
 * @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input uint16 offset-binary array; must contain at least
 *                @p n elements.
 * @param output  Output float32 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import UQ15ToF32
 * >>> import numpy as np
 * >>> UQ15ToF32().steps(np.array([0, 32768], dtype=np.uint16)).tolist()
 * [-1.0, 0.0]
 *
 * @endcode
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
