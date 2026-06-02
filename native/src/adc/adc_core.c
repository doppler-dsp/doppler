#include "adc/adc_core.h"
#include "jm_simd.h"

#define ADC_RNG_SEED 0x12345678u

adc_state_t *
adc_create(int bits, float dbfs, int dithering)
{
    if (bits < 1 || bits > 64 || dithering < 0)
        return NULL;
    adc_state_t *obj = calloc(1, sizeof(*obj));
    if (!obj)
        return NULL;
    obj->bits      = bits;
    obj->dbfs      = dbfs;
    obj->dithering = dithering;
    obj->rng       = ADC_RNG_SEED;
    /* ldexp(1.0, bits-1) == 2^(bits-1) exactly in double for bits<=64. */
    obj->scale    = ldexp(1.0, bits - 1) * pow(10.0, -(double)dbfs / 20.0);
    /* For bits==64, 1u<<63 is INT64_MIN; handle via bit pattern directly. */
    if (bits < 64) {
        obj->clip_max = (int64_t)((UINT64_C(1) << (bits - 1)) - 1);
        obj->clip_min = -(int64_t)(UINT64_C(1) << (bits - 1));
    } else {
        obj->clip_max = INT64_MAX;
        obj->clip_min = INT64_MIN;
    }
    return obj;
}

void
adc_destroy(adc_state_t *state)
{
    free(state);
}

void
adc_reset(adc_state_t *state)
{
    state->clipped = 0;
    state->rng     = ADC_RNG_SEED;
}

JM_HOT void
adc_steps(adc_state_t *state, const float *input,
          int64_t *output, size_t n)
{
    if (!state->dithering) {
        /* Hot path: SIMD-width float multiply, scalar int64 convert.
         * The multiply is the dominant cost; the per-lane scalar round
         * and clamp are cheap relative to the float load + scale. */
        size_t i = 0;
#if JM_SIMD_WIDTH_F32 > 1
        const float scale_f = (float)state->scale;
        const size_t stride = JM_SIMD_WIDTH_F32;
        JM_VEC_F32 sv = JM_SPLAT_F32(scale_f);
        float tmp[JM_SIMD_WIDTH_F32];
        for (; i + stride <= n; i += stride) {
            JM_VEC_F32 v = JM_MUL_F32(JM_LOAD_F32(input + i), sv);
            JM_STORE_F32(tmp, v);
            for (size_t k = 0; k < stride; k++) {
                int64_t q = llroundf(tmp[k]);
                if (q > state->clip_max) {
                    q = state->clip_max;
                    state->clipped = 1;
                } else if (q < state->clip_min) {
                    q = state->clip_min;
                    state->clipped = 1;
                }
                output[i + k] = q;
            }
        }
#endif
        for (; i < n; i++)
            output[i] = adc_step(state, input[i]);
        return;
    }
    /* Dither path: sequential PRNG — scalar loop preserves state ordering. */
    for (size_t i = 0; i < n; i++)
        output[i] = adc_step(state, input[i]);
}
