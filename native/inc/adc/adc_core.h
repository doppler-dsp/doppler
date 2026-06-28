/**
 * @file adc_core.h
 * @brief Signed two's-complement ADC model.
 *
 * Models an N-bit ADC with configurable full-scale reference (dBFS) and
 * optional TPDF dither.  A normalised float input is scaled by the
 * pre-computed double-precision factor, optionally dithered with a
 * triangular-PDF noise source, rounded, clamped to the signed integer range
 * `[-(2^(bits-1)), 2^(bits-1)-1]`, and returned as int64_t.
 *
 * Scale derivation:
 * @code
 *   scale = 2^(bits-1) * 10^(-dbfs / 20)
 * @endcode
 *
 * An input of amplitude 10^(dbfs/20) uses the full ADC code range.  With the
 * default dbfs=-10.0 a signal at -10 dBFS fills the converter exactly.
 * double precision is used throughout so converters wider than 23 bits
 * (float32 mantissa limit) are modelled without rounding artefacts.
 *
 * TPDF dither (dithering != 0): two xorshift32 uniform draws are summed to
 * produce a triangular PDF over `[-1, +1]` LSB before rounding.  This breaks
 * correlated quantisation noise patterns at the cost of a slight noise-floor
 * increase.  The PRNG state is part of the object; reset() re-seeds it.
 *
 * Lifecycle: create -> (step / steps / reset)* -> destroy
 *
 * @code
 * >>> from doppler.cvt import ADC
 * >>> import numpy as np
 * >>> obj = ADC(bits=8, dbfs=0.0, dithering=0)
 * >>> obj.scale
 * 128.0
 * >>> obj.bits
 * 8
 * >>> obj.step(0.0)
 * 0
 * >>> obj.step(-1.0)
 * -128
 * >>> obj.clipped
 * False
 * >>> obj.step(1.0)
 * 127
 * >>> obj.clipped
 * True
 * >>> obj.reset()
 * >>> obj.clipped
 * False
 * >>> x = np.array([-1.0, -0.5, 0.0, 0.5, 1.0], dtype=np.float32)
 * >>> obj.steps(x).tolist()
 * [-128, -64, 0, 64, 127]
 * @endcode
 */
#ifndef ADC_CORE_H
#define ADC_CORE_H

#include "clib_common.h"
#include "dp_state.h"
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
 * Computes @c scale = 2^(bits-1) * 10^(-dbfs/20), sets the clip bounds, and
 * seeds the xorshift32 PRNG to a fixed constant.  Returns NULL if @p bits is
 * outside `[1, 64]` or on allocation failure.
 *
 * @param bits      ADC resolution in bits (1..64).
 * @param dbfs      Full-scale reference level in dBFS (typically negative,
 *                  e.g. -10.0).  A signal with amplitude 10^(dbfs/20) fills
 *                  the converter's integer range exactly.
 * @param dithering 0 = no dither; non-zero = TPDF dither before rounding.
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
 * Clears the sticky @c clipped flag and re-seeds the xorshift32 PRNG to its
 * initial value so dithered runs are reproducible after reset.
 *
 * @param state  Must be non-NULL.
 */
void adc_reset(adc_state_t *state);

/**
 * @brief Process one input sample.
 *
 * Multiplies @p x by the pre-computed double-precision @c scale, optionally
 * adds TPDF dither, rounds with @c llround, and clamps to `[clip_min,
 * clip_max]`.  Sets the sticky @c clipped flag if clamping occurred.
 *
 * @param state  Must be non-NULL.
 * @param x      Normalised float input sample (typically in `[-1, +1]`).
 * @return Quantised signed integer in `[-(2^(bits-1)), 2^(bits-1)-1]`.
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
 * @brief Process a block of float samples to int64.
 *
 * When dithering is disabled the float-to-double multiply can use SIMD
 * widening (jm_simd.h); the int64_t conversion and clamp remain scalar.
 * When dithering is enabled the loop is scalar to preserve sequential PRNG
 * state.  Accepts an optional pre-allocated output array; allocates a fresh
 * one when @p output is NULL.
 *
 * @param state   Must be non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output int64 array; must contain at least @p n elements.
 * @param n       Number of samples to process.
 *
 * @code
 * >>> from doppler.cvt import ADC
 * >>> import numpy as np
 * >>> # ideal 12-bit ADC: full scale spans +-2**11 codes
 * >>> ADC(12, 0.0, 0).steps(np.array([0.0, 0.5, 0.999, -1.0],
 * ...                                dtype=np.float32)).tolist()
 * [0, 1024, 2046, -2048]
 *
 * @endcode
 */
void adc_steps(
    adc_state_t       *state,
    const float       *input,
    int64_t           *output,
    size_t             n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the dither RNG + sticky clip flag resume exactly into an
 * identically-built instance. */
#define ADC_STATE_MAGIC DP_FOURCC ('A','D','C',' ')
#define ADC_STATE_VERSION 1u
size_t adc_state_bytes (const adc_state_t *state);
void adc_get_state (const adc_state_t *state, void *blob);
int adc_set_state (adc_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CORE_H */
