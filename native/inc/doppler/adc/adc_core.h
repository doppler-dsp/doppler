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
 * step() works in double precision, so converters wider than 23 bits
 * (float32 mantissa limit) are modelled without rounding artefacts.  The
 * undithered steps() fast path multiplies in float32 instead (see
 * dp_adc_steps()).
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
#ifndef DP_ADC_CORE_H
#define DP_ADC_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ADC state.
 *
 * Allocate with dp_adc_create().
 *
 * @c clipped is sticky — set on any sample that saturates; cleared only
 * by reset().
 */
typedef struct {
    double   scale;     /**< Input-to-code gain, 2^(bits-1) * 10^(-dbfs/20)
                             (codes per unit input).  Config, set by
                             create(). */
    int64_t  clip_max;  /**< Largest output code, 2^(bits-1) - 1
                             (INT64_MAX when bits = 64).  Config. */
    int64_t  clip_min;  /**< Smallest output code, -2^(bits-1)
                             (INT64_MIN when bits = 64).  Config. */
    int      bits;      /**< ADC resolution in bits, 1..64.  Config. */
    float    dbfs;      /**< Full-scale reference level in dBFS, as passed
                             to create().  Config; informational once
                             @c scale is derived. */
    int      dithering; /**< 0 = no dither; positive = TPDF dither before
                             rounding.  Config. */
    uint8_t  clipped;   /**< Sticky saturation flag: 1 once any sample
                             saturated, 0 otherwise.  Running state, set by
                             step()/steps(), cleared by reset(). */
    uint32_t rng;       /**< xorshift32 dither PRNG state.  Running state;
                             seeded to a fixed constant by create() and
                             reset(), advanced only when dithering. */
} dp_adc_state_t;

/**
 * @brief Create an ADC instance.
 *
 * Computes @c scale = 2^(bits-1) * 10^(-dbfs/20), sets the clip bounds, and
 * seeds the xorshift32 PRNG to a fixed constant.  Returns NULL if @p bits is
 * outside `[1, 64]`, if @p dithering is negative, or on allocation failure.
 *
 * @param bits      ADC resolution in bits (1..64; default 16).
 * @param dbfs      Full-scale reference level in dBFS (typically negative,
 *                  e.g. -10.0).  A signal with amplitude 10^(dbfs/20) fills
 *                  the converter's integer range exactly (default -10.0).
 * @param dithering 0 = no dither (default); positive = TPDF dither before
 *                  rounding.  A negative value is rejected (NULL).
 * @return Heap-allocated state, or NULL on invalid args or allocation failure.
 * @note Caller must call dp_adc_destroy() when done.
 */
dp_adc_state_t *dp_adc_create(int bits, float dbfs, int dithering);

/**
 * @brief Destroy an ADC instance and release all memory.
 * @param state  Instance from dp_adc_create(); may be NULL (no-op).
 */
void dp_adc_destroy(dp_adc_state_t *state);

/**
 * @brief Clear the clip flag and re-seed the dither PRNG for a reproducible run.
 *
 * Zeroes the sticky @c clipped flag and re-seeds the xorshift32 dither PRNG to
 * its fixed initial value, so a dithered capture restarted after reset() is
 * bit-for-bit reproducible. The immutable configuration (bits, scale, clip
 * bounds) is preserved.
 *
 * @param state  Instance to reset; must be non-NULL.
 *
 * @code
 * >>> from doppler.cvt import ADC
 * >>> adc = ADC(bits=8, dbfs=0.0, dithering=0)
 * >>> adc.step(9.0)          # beyond full scale -> saturates, clips
 * 127
 * >>> adc.reset()           # clear clips, re-seed the dither PRNG
 * >>> adc.clipped
 * False
 *
 * @endcode
 */
void dp_adc_reset(dp_adc_state_t *state);

/**
 * @brief Quantise one float sample to a signed N-bit ADC code.
 *
 * Multiplies @p x by the pre-computed double-precision @c scale, optionally
 * adds TPDF dither (when the object was built with dithering enabled), rounds
 * with @c llround, and clamps to the signed integer range `[clip_min,
 * clip_max]`. Latches the sticky @c clipped flag when the scaled (and
 * dithered) value lies outside `[clip_min, clip_max]` BEFORE rounding, so a
 * value such as clip_max + 0.3 latches it even though it rounds to clip_max.
 * A sample at amplitude 10^(dbfs/20) reaches full scale.
 *
 * @param state  Instance to update (clip flag, dither PRNG); non-NULL.
 * @param x      Input sample, normally a normalised float in `[-1, +1]`.
 * @return Signed ADC code in `[-(2^(bits-1)), 2^(bits-1)-1]`.
 *
 * @code
 * >>> from doppler.cvt import ADC
 * >>> adc = ADC(bits=8, dbfs=0.0, dithering=0)  # 8-bit, FS at 0 dBFS
 * >>> adc.step(0.5)            # 0.5 * 128 codes
 * 64
 * >>> adc.step(2.0)            # beyond full scale -> clamps to +127
 * 127
 * >>> adc.clipped              # sticky flag latched by the clamp
 * True
 *
 * @endcode
 */
JM_FORCEINLINE JM_HOT int64_t
dp_adc_step(dp_adc_state_t *state, float x)
{
    /* jm: body sourced from [adc] impl/impl_file in objects/adc.toml — edit
     * there, not here; `jm apply` overwrites this. */
        double s = (double)x * state->scale;
        if (state->dithering) {
            /* TPDF dither: two SUCCESSIVE xorshift32 draws summed →
             * triangular PDF over [-1, +1] LSB.  Each uniform is in
             * [-0.5, +0.5] LSB.  The second is a draw of its own: a multiply
             * of the first is one variable, not two (#2651). */
            uint32_t r = state->rng;
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            uint32_t r1 = r;
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            state->rng = r;
            s += ((double)(int32_t)r1 + (double)(int32_t)r)
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
 * When dithering is disabled and a SIMD tier is available (jm_simd.h), whole
 * SIMD-width groups are scaled by @c scale rounded to float32, multiplied in
 * float32, rounded with @c llroundf and clamped; the clip flag latches on
 * the same double-precision pre-rounding value step() uses, so it does not
 * depend on which path a sample took.  The remaining tail samples, and
 * every sample when dithering is enabled, go through dp_adc_step() (double
 * precision, sequential PRNG).  For converters wider than ~24 bits the
 * float32 path can therefore differ from step() in the last codes.
 *
 * The C function writes into @p output and never allocates; the Python
 * binding allocates the output array when its optional @c out is omitted.
 *
 * @param state   Instance to update (clip flag, dither PRNG); non-NULL.
 * @param input   Input float32 array; must contain at least @p n elements.
 * @param output  Output int64 array; must hold at least @p n elements and
 *                must be non-NULL.
 * @param n       Number of samples to process (0 is a no-op).
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
void dp_adc_steps(
    dp_adc_state_t       *state,
    const float       *input,
    int64_t           *output,
    size_t             n);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the dither RNG + sticky clip flag resume exactly into an
 * identically-built instance. */
/** FourCC type tag ("ADC ") stamped in every ADC state blob's envelope. */
#define ADC_STATE_MAGIC DP_FOURCC ('A','D','C',' ')
/** ADC state blob format version; a blob of another version is rejected. */
#define ADC_STATE_VERSION 1u
/**
 * @brief Size in bytes of this instance's serialized state blob.
 *
 * A fixed size: the 16-byte dp_state_hdr_t envelope plus
 * sizeof(dp_adc_state_t) (the whole struct is snapshotted).
 *
 * @param state  Instance to size; not dereferenced.
 * @return Blob size in bytes; allocate this much for dp_adc_get_state().
 */
size_t dp_adc_state_bytes (const dp_adc_state_t *state);
/**
 * @brief Serialize the full state (config, clip flag, dither PRNG).
 *
 * Writes the envelope then a byte copy of the struct, so an identically
 * built instance restored from it continues bit-for-bit, dither included.
 *
 * @param state  Instance to snapshot; non-NULL.
 * @param[out] blob  Destination of at least dp_adc_state_bytes() bytes.
 */
void dp_adc_get_state (const dp_adc_state_t *state, void *blob);
/**
 * @brief Restore a state blob written by dp_adc_get_state().
 *
 * Validates the envelope (magic, version, endianness, size) first and
 * leaves @p state untouched on rejection.  On success the WHOLE struct is
 * overwritten, configuration (bits, dbfs, scale, clip bounds, dithering)
 * included, not just the running fields.
 *
 * @param state  Instance to overwrite; non-NULL.
 * @param blob   Blob of dp_adc_state_bytes() bytes.
 * @return DP_OK, or DP_ERR_INVALID if the envelope does not match.
 */
int dp_adc_set_state (dp_adc_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ADC_CORE_H */
