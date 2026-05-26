/**
 * @file cic_core.h
 * @brief CIC decimation filter — 4-stage, M=1, UQ16 integer pipeline.
 *
 * Fixed design parameters:
 *   N = 4 stages  (~77 dB alias rejection at f_p = 0.1 * f_out)
 *   M = 1         (differential delay — one-sample comb)
 *   R = power-of-two decimation ratio (enforced at create time)
 *
 * Input/output boundary: CF32 (`float _Complex`), matching the doppler
 * default signal type.  Internally, each sample is converted to UQ16 —
 * offset-binary: v_q15 + 32768 → `[0, 65535]` in a uint64_t — giving 48
 * bits of headroom for the pipeline gain of N * log2(R) bits.  For R <= 4096
 * (log2 = 12) the gain is 48 bits; max accumulation = 65535 * R^N =
 * (2^16 - 1) * 2^48 = 2^64 - 2^48 < 2^64, so no overflow occurs.
 *
 * All arithmetic is unsigned: inputs are non-negative `[0, 65535]`, wrapping
 * is defined (mod 2^64), and the output decode subtracts the offset in
 * floating-point — no signed integer casts anywhere in the hot path.
 *
 * The unsigned modular-arithmetic CIC property guarantees exact outputs:
 * every intermediate overflow in the integrators cancels in the comb
 * stages, provided the true result fits in 64 bits.  No saturation,
 * no range checks, no floating-point in the inner loop.
 *
 * With M=1 and N fixed, the entire comb state is four uint64_t values
 * per channel — no heap allocation beyond the state struct itself.
 *
 * Alias rejection : ~77 dB at f_p = 0.1 * f_out (independent of R)
 * Passband droop  : ~0.57 dB at f_p = 0.1 * f_out (independent of R)
 * Output precision: 16-bit Q15 (independent of R and N)
 *
 * @code
 * cic_state_t *cic = cic_create(16);   // R=16, N=4, M=1
 * size_t n_out = cic_decimate(cic, in, 1024, out);
 * cic_destroy(cic);
 * @endcode
 */
#ifndef CIC_CORE_H
#define CIC_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fixed stage count.  Alias rejection ~19.2 dB/stage at f_p=0.1. */
#define CIC_N 4

/**
 * @brief CIC filter state.
 *
 * Allocate with cic_create(); free with cic_destroy().
 * All comb state fits in-struct — no heap members.
 */
typedef struct {
    uint64_t integ_re[CIC_N]; /* integrator accumulators, real path    */
    uint64_t integ_im[CIC_N]; /* integrator accumulators, imag path    */
    uint64_t comb_re[CIC_N];  /* previous comb input per stage, real   */
    uint64_t comb_im[CIC_N];  /* previous comb input per stage, imag   */
    uint32_t R;               /* decimation ratio (power of two)       */
    uint32_t phase;           /* input sample counter 0..R-1           */
    uint32_t shift;           /* CIC_N * log2(R) — right-shift to norm */
} cic_state_t;

/**
 * @brief Create a CIC decimation filter.
 *
 * @param R  Decimation ratio.  Must be a power of two in `[2, 4096]`.
 *           Returns NULL for R=0, non-power-of-two, or R > 4096.
 * @return   Heap-allocated state, or NULL on invalid R or OOM.
 */
cic_state_t *cic_create(uint32_t R);

/** Free resources.  NULL is a no-op. */
void cic_destroy(cic_state_t *state);

/**
 * @brief Zero all filter state; preserve R and shift.
 *
 * The first output sample after reset is produced on input sample R-1,
 * matching post-create behaviour.
 */
void cic_reset(cic_state_t *state);

/**
 * @brief Upper bound on decimate output — returns 0 (lazy-alloc signal).
 *
 * The Python extension allocates n_in elements on the first call.
 * Since n_in >= ceil(n_in/R) = n_out for all R >= 1, the buffer is
 * always large enough as long as block size stays consistent.
 */
size_t cic_decimate_max_out(cic_state_t *state);

/**
 * @brief Decimate n_in CF32 samples; write output to out.
 *
 * Each sample is converted to UQ16, run through CIC_N integrators,
 * tested against the decimation phase, then (if a decimation boundary)
 * passed through CIC_N comb stages and converted back to CF32.
 *
 * @param state  Must be non-NULL.
 * @param in     CF32 input array, length n_in.
 * @param n_in   Number of input samples.
 * @param out    Output buffer; must hold at least
 *               ceil((state->phase + n_in) / state->R) elements.
 * @return       Number of output samples written.
 */
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

/**
 * @brief Change the decimation ratio in place; resets all filter state.
 *
 * Silently ignores invalid R (non-power-of-two, out of range).
 *
 * @param state  Filter state to reconfigure.  Must be non-NULL.
 * @param R      New decimation ratio.  Same constraints as cic_create().
 */
void cic_reconfigure(cic_state_t *state, uint32_t R);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
