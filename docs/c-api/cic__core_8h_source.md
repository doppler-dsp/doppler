

# File cic\_core.h

[**File List**](files.md) **>** [**cic**](dir_b18ea702eaae2e8004fd6583c8b1e641.md) **>** [**cic\_core.h**](cic__core_8h.md)

[Go to the documentation of this file](cic__core_8h.md)


```C++

#ifndef DP_CIC_CORE_H
#define DP_CIC_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CIC_N 4

#define CIC_OFFSET ((uint64_t)32768)

#define CIC_R_MAX 2048u

typedef struct {
    uint64_t integ_re[CIC_N]; 
    uint64_t integ_im[CIC_N]; 
    uint64_t comb_re[CIC_N];  
    uint64_t comb_im[CIC_N];  
    uint32_t R;               
    uint32_t phase;           
    uint32_t shift;           
    uint8_t  clipped;         
} dp_cic_state_t;

dp_cic_state_t *dp_cic_create(uint32_t R);

void dp_cic_destroy(dp_cic_state_t *state);

void dp_cic_reset(dp_cic_state_t *state);

/* Serializable state (reusable elastic-resume convention): the decimation
 * rate R, the integrator and comb accumulators, the decimation phase counter
 * and the sticky clip flag.  R travels because dp_cic_reconfigure() changes
 * it (#2022's rule, doppler#2642); shift is derived from it on restore, so
 * the two cannot disagree.  `clipped` is
 * running state, not a diagnostic bolted on the side: a resumed stream that
 * silently forgot it had clipped would answer the question wrongly. */

/* Standard bytes interface (see dp_state.h): [dp_state_hdr_t][u32 R]
 * [integ_re/im, comb_re/im (CIC_N u64 each)][u32 phase][u8 clipped]. */
#define CIC_STATE_MAGIC DP_FOURCC ('C', 'I', 'C', '_')
#define CIC_STATE_VERSION 4u

#define CIC_PAPR_HEADROOM 2.0f

size_t dp_cic_state_bytes(const dp_cic_state_t *state);
void dp_cic_get_state(const dp_cic_state_t *state, void *blob);
int dp_cic_set_state(dp_cic_state_t *state, const void *blob);

size_t dp_cic_decimate_max_out(dp_cic_state_t *state);

double dp_cic_dc_gain(const dp_cic_state_t *state);

double dp_cic_get_group_delay(const dp_cic_state_t *state);

#ifndef __cplusplus
JM_FORCEINLINE JM_HOT size_t
dp_cic_decimate(dp_cic_state_t *state, const float _Complex *in,
             size_t n_in, float _Complex *out, size_t max_out)
{
    const uint32_t R     = state->R;
    const uint32_t shift = state->shift;
    size_t n_out = 0;
    int    clip  = 0;   /* accumulated in a register; folded in once below */

    for (size_t i = 0; i < n_in; i++) {
        /* CF32 → Q15, saturated. The four comparisons run regardless, so
           noting that one fired costs a register OR — which is the whole
           reason `clipped` exists rather than a line of documentation
           asking callers to be careful. The upper one is written as
           "not at or under", so a NaN, which fails every comparison, lands
           on the top of the range and is counted as a clip: the conversion
           to an integer below is never handed a value it cannot hold. */
        float sr = crealf(in[i]) * (32768.0f / CIC_PAPR_HEADROOM);
        float si = cimagf(in[i]) * (32768.0f / CIC_PAPR_HEADROOM);
        if (!(sr <=  32767.0f)) { sr =  32767.0f; clip = 1; }
        if (sr < -32768.0f)     { sr = -32768.0f; clip = 1; }
        if (!(si <=  32767.0f)) { si =  32767.0f; clip = 1; }
        if (si < -32768.0f)     { si = -32768.0f; clip = 1; }
        /* Into the unsigned registers as the value itself, modulo 2^64:
           through offset binary, which is never negative, and the offset
           straight back off in unsigned arithmetic. No register here is
           signed. A zero sample is a zero word, so a filter that has only
           seen silence holds zeros. */
        uint64_t re = (uint64_t)((int32_t)(int16_t)sr + 32768) - CIC_OFFSET;
        uint64_t im = (uint64_t)((int32_t)(int16_t)si + 32768) - CIC_OFFSET;

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

        /* Capacity reached: keep running the filter (the integrator and
           comb state must stay in step with the input stream) but stop
           writing.  Guarding the store, not the loop, is the difference
           between a truncated block and a corrupted filter. */
        if (n_out >= max_out)
            continue;

        /* → CF32. The registers hold the filtered sum modulo 2^64, which
           is negative as often as not. Adding the offset at the pipeline's
           gain makes it the offset-binary sum, in [0, 2^16 * R^N), which
           fits; the right shift then normalises it and floors it, an
           unsigned shift of a non-negative value. The offset comes off in
           floating point, and the encoder's PAPR headroom is undone. The
           offset is NOT scaled by the headroom — it is the midpoint, not
           signal. */
        const uint64_t bias = CIC_OFFSET << shift;
        out[n_out++] = CMPLXF(
            ((float)(uint16_t)((re + bias) >> shift) - 32768.0f)
                * (CIC_PAPR_HEADROOM / 32768.0f),
            ((float)(uint16_t)((im + bias) >> shift) - 32768.0f)
                * (CIC_PAPR_HEADROOM / 32768.0f));
    }
    state->clipped |= (uint8_t)clip;   /* sticky; cleared by reset() */
    return n_out;
}
#else
/* From C++ this is a call into the library's out-of-line copy, which
   cic_core.c emits from the inlined definition above (the `extern` idiom
   of util's saturate.c), so there is one body: that body uses the C99
   complex library, which C++ does not have (dp_complex.h, "From C++"). A
   plain comment, because this block's Doxygen is also the Python
   docstring. */
size_t dp_cic_decimate(dp_cic_state_t *state, const float _Complex *in,
                       size_t n_in, float _Complex *out, size_t max_out);
#endif /* !__cplusplus */

void dp_cic_reconfigure(dp_cic_state_t *state, uint32_t R);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
```


