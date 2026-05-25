/**
 * @file cic_core.h
 * @brief Cascaded Integrator-Comb (CIC) decimation filter for CF32 IQ.
 *
 * Internally runs two identical uint64_t pipelines — one for the real
 * part and one for the imaginary part.  All arithmetic is unsigned, so
 * C-guaranteed wrap-around handles intermediate overflow automatically.
 * The final output is correct as long as the true (infinite-precision)
 * result fits in 63 bits after applying input_scale.
 *
 * Structure (decimating by R, N stages, differential delay M):
 *
 *   `x[n]` → INT_1 → … → INT_N → ↓R → COMB_1 → … → COMB_N → `y[n]`
 *
 * input_scale is chosen to maximise dynamic range for ±1.0 input:
 *
 *   input_scale = floor((2^63 − 1) / (R × M)^N)
 *
 * output_scale is the reciprocal normalisation so the float output
 * is back in ±1.0 range for a ±1.0 input signal at full CIC passband.
 *
 * Dynamic range (bits) for M=1:
 *
 *        R     N=2  N=3  N=4  N=5  N=6
 *        2      61   60   59   58   57
 *        8      57   54   51   48   45
 *       32      53   48   43   38   33
 *      256      47   39   31   23   15
 *     1024      43   33   23   13    3
 *
 * @code
 * cic_state_t *cic = cic_create(32, 4, 1);
 * // stream processing — block size must stay consistent after first call
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

/**
 * @brief CIC filter state.
 *
 * Allocate with cic_create(); free with cic_destroy().
 *
 * integ_re / integ_im are fixed-size (max N=6); comb_re / comb_im are
 * heap-allocated to N×M elements and freed in cic_destroy().
 */
typedef struct {
    uint64_t  integ_re[6];   /* N integrator accumulators, real path   */
    uint64_t  integ_im[6];   /* N integrator accumulators, imag path   */
    uint64_t *comb_re;       /* N×M comb delay line, real — heap       */
    uint64_t *comb_im;       /* N×M comb delay line, imag — heap       */
    uint32_t  comb_head[6];  /* circular write index per comb stage    */
    uint32_t  R;             /* decimation ratio                        */
    uint32_t  N;             /* number of integrator/comb stages (1–6) */
    uint32_t  M;             /* comb differential delay (1 or 2)       */
    uint32_t  phase;         /* input sample phase counter 0..R-1      */
    double    input_scale;   /* float → int64 scale (auto-computed)    */
    double    output_scale;  /* 1 / (input_scale × (R×M)^N)            */
} cic_state_t;

/**
 * @brief Create a CIC decimation filter.
 *
 * input_scale is computed as floor((2^63−1) / (R×M)^N), which fills
 * every available bit and leaves no headroom — correct for ±1.0 inputs.
 *
 * @param R  Decimation ratio (≥ 1).
 * @param N  Number of stages (1–6).
 * @param M  Differential delay (1 or 2).
 * @return   Heap-allocated state, or NULL on invalid args or OOM.
 */
cic_state_t *cic_create(uint32_t R, uint32_t N, uint32_t M);

/** Free all resources.  NULL is a no-op. */
void cic_destroy(cic_state_t *state);

/**
 * @brief Zero integrators and comb delay lines; preserve R, N, M.
 *
 * Resets the phase counter so the first output sample after reset
 * is produced on input sample R-1 (same as after cic_create).
 */
void cic_reset(cic_state_t *state);

/**
 * @brief Upper bound on decimate output for the lazy-alloc ext path.
 *
 * Returns 0 so the Python extension allocates the output buffer on the
 * first call (sized to n_in, which is always ≥ the actual output count
 * n_in/R).  Block size must stay consistent after the first call.
 */
size_t cic_decimate_max_out(cic_state_t *state);

/**
 * @brief Decimate n_in CF32 samples; write output to out.
 *
 * Each input sample is converted to a uint64_t (two's complement) via
 * input_scale, run through N integrators, and tested against the
 * decimation phase.  Every R-th sample is passed through N comb stages
 * and converted back to CF32 via output_scale.
 *
 * @param state  Must be non-NULL.
 * @param in     CF32 input array, length n_in.
 * @param n_in   Number of input samples.
 * @param out    Output buffer; must hold at least
 *               ceil((state->phase + n_in) / state->R) elements.
 * @return       Number of output samples written.
 */
size_t cic_decimate(cic_state_t *state, const float complex *in,
                    size_t n_in, float complex *out);

/**
 * @brief Reconfigure R, N, M in place; resets all filter state.
 *
 * Reallocates the comb delay lines if N×M changes.  Silently ignores
 * invalid parameters (R=0, N=0 or N>6, M=0 or M>2, or OOM).
 */
void cic_reconfigure(cic_state_t *state, uint32_t R, uint32_t N,
                     uint32_t M);

#ifdef __cplusplus
}
#endif

#endif /* CIC_CORE_H */
