/**
 * @file adc_core.h
 * @brief Signed two's-complement ADC model.
 *
 * Models an N-bit ADC with configurable full-scale reference (dBFS) and
 * optional TPDF dither.  A normalised float input is scaled, optionally
 * dithered, rounded, and clamped to the signed integer range
 * [-(2^(bits-1)), 2^(bits-1)-1], then returned as int64_t.
 *
 * Scale derivation:
 * @code
 *   scale = 2^(bits-1) * 10^(-dbfs / 20)
 * @endcode
 * An input of amplitude 10^(dbfs/20) uses the full ADC range.  With the
 * default dbfs=-10.0, a -10 dBFS signal fills the converter exactly.
 *
 * double precision is used for the intermediate computation so that
 * converters beyond 23 bits (float mantissa limit) are modelled correctly.
 *
 * TPDF dither (dithering != 0): two xorshift32 uniform draws are summed to
 * produce a triangular PDF over [-1, +1] LSB before rounding.  The dither
 * eliminates correlated quantisation noise at the cost of a slight noise
 * floor increase.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * Example:
 * @code
 * adc_state_t *adc = adc_create(16, -10.0f, 0);
 * int64_t y = adc_step(adc, 0.0f);   // y == 0
 * adc_destroy(adc);
 * @endcode
 */
#ifndef ADC_CORE_H
#define ADC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC state.
 *
 * Allocate with adc_create().
 *
 * @c clipped is sticky — set on any sample that saturates; cleared only
 * by reset().
 */
typedef struct {
    double   scale;     /* 2^(bits-1) * 10^(-dbfs/20) */
    int64_t  clip_max;  /* 2^(bits-1) - 1              */
    int64_t  clip_min;  /* -2^(bits-1)                 */
    int      bits;      /* ADC resolution, 1..64        */
    float    dbfs;      /* full-scale reference level   */
    int      dithering; /* 0 = off; non-zero = TPDF     */
    uint8_t  clipped;   /* sticky saturation flag       */
    uint32_t rng;       /* xorshift32 PRNG state        */
} adc_state_t;

/**
 * @brief Create an ADC instance.
 *
 * @param bits       ADC resolution in bits (1..64).
 * @param dbfs       Full-scale reference level in dBFS (typically negative).
 *                   A -10 dBFS signal at amplitude 10^(dbfs/20) fills the
 *                   ADC range.
 * @param dithering  0 = no dither; non-zero = TPDF dither enabled.
 * @return Heap-allocated state, or NULL on invalid args or allocation failure.
 * @note Caller must call adc_destroy() when done.
 */
adc_state_t *adc_create(int bits, float dbfs, int dithering);

/**
 * @brief Destroy an ADC instance and release all memory.
 * @param state  May be NULL.
 */
void adc_destroy(adc_state_t *state);

/**
 * @brief Reset ADC to its post-create state.
 *
 * Clears the sticky @c clipped flag and re-seeds the PRNG.
 * @param state  Must be non-NULL.
 */
void adc_reset(adc_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Scales @p x by the precomputed factor, adds TPDF dither when enabled,
 * clamps to [clip_min, clip_max], and returns the quantised int64_t.
 *
 * @param state  Must be non-NULL.
 * @param x      Normalised float input sample.
 * @return Quantised signed integer in [-(2^(bits-1)), 2^(bits-1)-1].
 */
JM_FORCEINLINE JM_HOT int64_t
adc_step(adc_state_t *state, float x)
{
        double s = (double)x * state->scale;
        if (state->dithering) {
            /* TPDF dither: two xorshift32 draws summed → triangular PDF
             * over [-1, +1] LSB.  Each uniform is in [-0.5, +0.5] LSB. */
            state->rng ^= state->rng << 13;
            state->rng ^= state->rng >> 17;
            state->rng ^= state->rng << 5;
            uint32_t r2 = state->rng * 2654435761u;
            s += ((double)(int32_t)state->rng + (double)(int32_t)r2)
                 * (1.0 / 4294967296.0);
        }
        state->clipped |= (uint8_t)(s > (double)state->clip_max
                                  || s < (double)state->clip_min);
        int64_t v = llround(s);
        if (v > state->clip_max) v = state->clip_max;
        if (v < state->clip_min) v = state->clip_min;
        return (int64_t)v;






}

/**
 * @brief Process a block of samples.
 *
 * When dithering is disabled the float-to-scale multiply is widened with
 * SIMD (jm_simd.h); the int64_t conversion and clamp remain scalar.
 * When dithering is enabled the loop is scalar to preserve sequential PRNG
 * state.
 *
 * @param state   Component state (mutated; clipped flag updated).
 * @param input   Input array (length >= n).
 * @param output  Output array (length >= n).
 * @param n       Number of samples.
 */
void adc_steps(
    adc_state_t       *state,
    const float       *input,
    int64_t           *output,
    size_t             n);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CORE_H */
