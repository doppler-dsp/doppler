

# File cic\_core.h

[**File List**](files.md) **>** [**cic**](dir_cf560077cc62991e7289ea57a3d930a1.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the documentation of this file](cic__core_8h.md)


```C++

#ifndef CIC_CORE_H
#define CIC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CIC_N 4

typedef struct {
    uint64_t integ_re[CIC_N]; /* integrator accumulators, real path    */
    uint64_t integ_im[CIC_N]; /* integrator accumulators, imag path    */
    uint64_t comb_re[CIC_N];  /* previous comb input per stage, real   */
    uint64_t comb_im[CIC_N];  /* previous comb input per stage, imag   */
    uint32_t R;               /* decimation ratio (power of two)       */
    uint32_t phase;           /* input sample counter 0..R-1           */
    uint32_t shift;           /* CIC_N * log2(R) — right-shift to norm */
} cic_state_t;

cic_state_t *cic_create(uint32_t R);

void cic_destroy(cic_state_t *state);

void cic_reset(cic_state_t *state);

size_t cic_decimate_max_out(cic_state_t *state);

JM_FORCEINLINE JM_HOT size_t
cic_decimate(cic_state_t *state, const float complex *in,
             size_t n_in, float complex *out)
{
    const uint32_t R     = state->R;
    const uint32_t shift = state->shift;
    size_t n_out = 0;

    for (size_t i = 0; i < n_in; i++) {
        /* CF32 → UQ16: saturate to Q15, shift to offset-binary [0, 65535]. */
        float sr = crealf(in[i]) * 32768.0f;
        float si = cimagf(in[i]) * 32768.0f;
        if (sr >  32767.0f) sr =  32767.0f;
        if (sr < -32768.0f) sr = -32768.0f;
        if (si >  32767.0f) si =  32767.0f;
        if (si < -32768.0f) si = -32768.0f;
        uint64_t re = (uint64_t)((int32_t)(int16_t)sr + 32768);
        uint64_t im = (uint64_t)((int32_t)(int16_t)si + 32768);

        /* 4 integrators — unsigned wrap-around is intentional. */
        re = state->integ_re[0] += re;
        re = state->integ_re[1] += re;
        re = state->integ_re[2] += re;
        re = state->integ_re[3] += re;
        im = state->integ_im[0] += im;
        im = state->integ_im[1] += im;
        im = state->integ_im[2] += im;
        im = state->integ_im[3] += im;

        if (++state->phase < R)
            continue;
        state->phase = 0;

        /* 4 comb stages — M=1: y = x - prev; prev = x. */
        uint64_t t;
        t = state->comb_re[0]; state->comb_re[0] = re; re -= t;
        t = state->comb_re[1]; state->comb_re[1] = re; re -= t;
        t = state->comb_re[2]; state->comb_re[2] = re; re -= t;
        t = state->comb_re[3]; state->comb_re[3] = re; re -= t;
        t = state->comb_im[0]; state->comb_im[0] = im; im -= t;
        t = state->comb_im[1]; state->comb_im[1] = im; im -= t;
        t = state->comb_im[2]; state->comb_im[2] = im; im -= t;
        t = state->comb_im[3]; state->comb_im[3] = im; im -= t;

        /* UQ16 → CF32: right-shift to normalise, remove offset-binary bias. */
        out[n_out++] = CMPLXF(
            ((float)(uint16_t)(re >> shift) - 32768.0f) * (1.0f / 32768.0f),
            ((float)(uint16_t)(im >> shift) - 32768.0f) * (1.0f / 32768.0f));
    }
    return n_out;
}

void cic_reconfigure(cic_state_t *state, uint32_t R);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
```


