

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

#define CIC_REG_BITS 64u

#define CIC_PRECISION_16 16u
#define CIC_PRECISION_32 32u
#define CIC_PRECISION_64 64u
#define CIC_PRECISION_DEFAULT CIC_PRECISION_64
#define CIC_HEADROOM_BITS_16 1u
#define CIC_HEADROOM_BITS_WIDE 2u
#define CIC_PAPR_16 ((float)(1u << CIC_HEADROOM_BITS_16))
#define CIC_PAPR_WIDE ((float)(1u << CIC_HEADROOM_BITS_WIDE))
#define CIC_WORD_TOP(b)                                                    \
    ((b) <= 25 ? (float)((1ull << ((b) - 1)) - 1u)                         \
               : (float)(1ull << ((b) - 1)) * (1.0f - 0x1p-24f))
#define CIC_WORD_BOTTOM(b) (-(float)(1ull << ((b) - 1)))
#define CIC_WORD_WIDTHS(X)                                                 \
    X(16) X(20) X(24) X(28) X(32) X(36) X(40) X(44) X(48) X(52) X(56) X(60)
#define CIC_FRACTION_BITS_MIN 1u
#define CIC_HEADROOM_BITS_MAX (CIC_PRECISION_16 - 1u - CIC_FRACTION_BITS_MIN)
#define CIC_R_MAX 2048u

typedef struct {
    uint64_t integ_re[CIC_N]; 
    uint64_t integ_im[CIC_N]; 
    uint64_t comb_re[CIC_N];  
    uint64_t comb_im[CIC_N];  
    uint32_t R;               
    uint32_t phase;           
    uint32_t shift;           
    uint32_t precision;       
    uint32_t headroom_bits;   
    uint32_t bits;            
    uint64_t offset;          
    float    in_scale;        
    float    out_scale;       
    uint8_t  clipped;         
} dp_cic_state_t;

dp_cic_state_t *dp_cic_create(uint32_t R);

void dp_cic_destroy(dp_cic_state_t *state);

void dp_cic_reset(dp_cic_state_t *state);

/* Serializable state (reusable elastic-resume convention): the decimation
 * rate R, the precision and the headroom, the integrator and comb
 * accumulators, the decimation phase counter and the sticky clip flag.  R,
 * the precision and the headroom travel because dp_cic_reconfigure(),
 * dp_cic_set_precision() and dp_cic_set_headroom_bits() change
 * them (#2022's rule, doppler#2642); the shift, the word and its scales are
 * derived from the three on restore, so they cannot disagree.  `clipped` is
 * running state, not a diagnostic bolted on the side: a resumed stream that
 * silently forgot it had clipped would answer the question wrongly. */

/* Standard bytes interface (see dp_state.h): [dp_state_hdr_t][u32 R]
 * [u32 precision][u32 headroom_bits][integ_re/im, comb_re/im (CIC_N u64
 * each)][u32 phase][u8 clipped]. */
#define CIC_STATE_MAGIC DP_FOURCC ('C', 'I', 'C', '_')
#define CIC_STATE_VERSION 5u

int dp_cic_precision_ok(uint32_t precision);

uint32_t dp_cic_precision_headroom(uint32_t precision);

int dp_cic_headroom_bits_ok(uint32_t bits);

int dp_cic_set_precision(dp_cic_state_t *state, uint32_t precision);

uint32_t dp_cic_get_precision(const dp_cic_state_t *state);

uint32_t dp_cic_get_bits(const dp_cic_state_t *state);

int dp_cic_set_headroom_bits(dp_cic_state_t *state, uint32_t bits);

uint32_t dp_cic_get_headroom_bits(const dp_cic_state_t *state);

double dp_cic_get_full_scale(const dp_cic_state_t *state);

double dp_cic_get_step(const dp_cic_state_t *state);

size_t dp_cic_state_bytes(const dp_cic_state_t *state);
void dp_cic_get_state(const dp_cic_state_t *state, void *blob);
int dp_cic_set_state(dp_cic_state_t *state, const void *blob);

size_t dp_cic_decimate_max_out(dp_cic_state_t *state);

double dp_cic_dc_gain(const dp_cic_state_t *state);

double dp_cic_get_group_delay(const dp_cic_state_t *state);

#ifndef __cplusplus
JM_FORCEINLINE JM_HOT size_t
cic_decimate_word(dp_cic_state_t *state, const float _Complex *in,
                  size_t n_in, float _Complex *out, size_t max_out,
                  const unsigned b)
{
    const uint32_t R      = state->R;
    const uint32_t shift  = state->shift;
    const float    scale  = state->in_scale;
    const float    hi     = CIC_WORD_TOP(b);
    const float    lo     = CIC_WORD_BOTTOM(b);
    const uint64_t offset = (uint64_t)1 << (b - 1u);
    /* Read once: `out` is a float array and the compiler must otherwise
       assume each output it stores could be this field, and reload it. */
    const float    step   = state->out_scale;
    const uint64_t bias   = offset << shift;
    size_t n_out = 0;
    int    clip  = 0;   /* accumulated in a register; folded in once below */
    /* The filter's registers, in locals for the whole call and written back
       once at the end: the loop then touches no memory but the input and
       the output, so where the state sits beside the buffers cannot slow
       it (measured: storing them every sample, one benchmark program ran
       the converter's CIC plan a quarter slower than another with the
       same objects). */
    uint64_t ir[CIC_N], ii[CIC_N], cr[CIC_N], ci[CIC_N];
    for (int k = 0; k < CIC_N; k++) {
        ir[k] = state->integ_re[k];
        ii[k] = state->integ_im[k];
        cr[k] = state->comb_re[k];
        ci[k] = state->comb_im[k];
    }
    uint32_t phase = state->phase;

    for (size_t i = 0; i < n_in; i++) {
        /* CF32 → the word, saturated. The four comparisons run regardless,
           so noting that one fired costs a register OR — which is the whole
           reason `clipped` exists rather than a line of documentation
           asking callers to be careful. The upper one is written as "not
           at or under", so a NaN, which fails every comparison, lands on
           the top of the range and is counted as a clip: the conversion to
           an integer below is never handed a value it cannot hold. */
        float sr = crealf(in[i]) * scale;
        float si = cimagf(in[i]) * scale;
        if (!(sr <= hi)) { sr = hi; clip = 1; }
        if (sr < lo)     { sr = lo; clip = 1; }
        if (!(si <= hi)) { si = hi; clip = 1; }
        if (si < lo)     { si = lo; clip = 1; }
        /* Into the unsigned registers as the value itself, modulo 2^64:
           through offset binary, which is never negative (the word is
           under CIC_REG_BITS - 1 bits, so the sum cannot leave an int64),
           and the offset straight back off in unsigned arithmetic. No
           register here is signed. A zero sample is a zero word, so a
           filter that has only seen silence holds zeros. */
        uint64_t re = (uint64_t)((int64_t)sr + (int64_t)offset) - offset;
        uint64_t im = (uint64_t)((int64_t)si + (int64_t)offset) - offset;

        /* CIC_N integrators — unsigned wrap-around is intentional. The
           count is a constant, so the loops below unroll. */
        for (int k = 0; k < CIC_N; k++) {
            re = ir[k] += re;
            im = ii[k] += im;
        }

        if (++phase < R)
            continue;
        phase = 0;

        /* CIC_N one-sample combs: y = x - prev; prev = x. */
        for (int k = 0; k < CIC_N; k++) {
            uint64_t t;
            t = cr[k]; cr[k] = re; re -= t;
            t = ci[k]; ci[k] = im; im -= t;
        }

        /* Capacity reached: keep running the filter (the integrator and
           comb state must stay in step with the input stream) but stop
           writing.  Guarding the store, not the loop, is the difference
           between a truncated block and a corrupted filter. */
        if (n_out >= max_out)
            continue;

        /* → CF32. The registers hold the filtered sum modulo 2^64, which
           is negative as often as not. Adding the offset at the pipeline's
           gain makes it the offset-binary sum, in [0, 2^b * R^CIC_N),
           which fits; the right shift then normalises it and floors it,
           an unsigned shift of a non-negative value. What is left is a
           word, so it and the offset are both in an int64 and their
           difference is the sample. */
        out[n_out++] = CMPLXF(
            (float)((int64_t)((re + bias) >> shift) - (int64_t)offset)
                * step,
            (float)((int64_t)((im + bias) >> shift) - (int64_t)offset)
                * step);
    }
    for (int k = 0; k < CIC_N; k++) {
        state->integ_re[k] = ir[k];
        state->integ_im[k] = ii[k];
        state->comb_re[k]  = cr[k];
        state->comb_im[k]  = ci[k];
    }
    state->phase = phase;
    state->clipped |= (uint8_t)clip;   /* sticky; cleared by reset() */
    return n_out;
}

#define CIC_WORD_CASE(b)                                                   \
    case b:                                                                \
        return cic_decimate_word(state, in, n_in, out, max_out, b);

JM_FORCEINLINE JM_HOT size_t
dp_cic_decimate(dp_cic_state_t *state, const float _Complex *in,
             size_t n_in, float _Complex *out, size_t max_out)
{
    /* One call of the kernel for each width, each with its width as a
       literal: a width the switch does not name is one no filter has
       (CIC_WORD_WIDTHS() is every one derive() can give). */
    switch (state->bits) {
        CIC_WORD_WIDTHS(CIC_WORD_CASE)
    default:
        return 0;
    }
}
#undef CIC_WORD_CASE
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


