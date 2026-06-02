/**
 * @file hbdecim_q15_core.h
 * @brief Fixed-point halfband 2:1 decimator for interleaved IQ int16 samples.
 *
 * Input: interleaved int16_t pairs (I₀ Q₀ I₁ Q₁ …) as produced by ADCIQ.
 * Output: decimated 2:1 interleaved int16_t IQ pairs.
 *
 * Algorithm
 * ---------
 * The halfband filter has the polyphase property: one branch is a pure delay
 * (every other sample passes through unchanged) and the other branch carries
 * the FIR computation.  Caller supplies the FIR branch coefficients as float;
 * they are converted to Q15 internally with the standard x0.5 rate scaling.
 *
 * The symmetric-fold optimisation halves the number of multiplications:
 * instead of computing Sum h[k]*x[n-k] for all k, the filter computes
 * Sum h[k]*(x[n-k] + x[n-(N-1-k)]) for k = 0..N/2-1, exploiting
 * h[k] = h[N-1-k].  The center tap is a single unconditional right-shift
 * (x0.5, baked in as the polyphase rate identity).
 *
 * On AVX2 the inner loop uses _mm256_madd_epi16 to multiply 16 int16_t
 * coefficient values against 16 int16_t folded delay-line samples in a
 * single instruction, accumulating into 8 int32_t lanes.  I and Q run as
 * two independent madd chains on the same coefficient vector — free ILP on
 * any superscalar core.  The fold uses saturating add (_mm256_adds_epi16)
 * which clips at +-32767; for signals at or below -1 dBFS the saturation
 * never fires.  The int32_t accumulator is reduced to int64_t before the
 * final round-and-shift to Q15 output.
 *
 * Delay-line layout
 * -----------------
 * Even- and odd-indexed input samples are demultiplexed into separate I and Q
 * rings (four int16_t dual-write rings total).  The dual-write trick stores
 * each value at position p and p+cap so the FIR inner loop reads a contiguous
 * slice — no modulo arithmetic in the hot path.
 *
 * Lifecycle
 * ---------
 * @code
 *   hbdecim_q15_state_t *r = hbdecim_q15_create(num_taps, h_fir);
 *   // in:  interleaved int16_t IQ, 2*n_in elements
 *   // out: interleaved int16_t IQ, 2*n_out elements (n_out <= n_in/2)
 *   size_t n = hbdecim_q15_execute(r, in, n_in, out, max_out);
 *   hbdecim_q15_destroy(r);
 * @endcode
 */
#ifndef HBDECIM_Q15_CORE_H
#define HBDECIM_Q15_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t   num_taps;    /* FIR branch length supplied by caller          */
    size_t   K;           /* symmetric pair count = num_taps / 2           */
    size_t   K_pad;       /* K rounded up to multiple of 16 (SIMD align)   */
    size_t   centre;      /* = num_taps / 2 (index of delay-branch tap)    */
    int      fir_on_even; /* 1: FIR on even_dl; 0: FIR on odd_dl          */

    /* Q15 FIR branch coefficients, zero-padded to K_pad entries.
     * Scaled by 0.5 at create() to match the halfband polyphase identity.
     * Allocated with 32-byte alignment for AVX2 aligned loads.            */
    int16_t *coeffs;

    /* Dual-write delay rings: even- and odd-indexed input samples split
     * into separate I and Q arrays for contiguous SIMD access.            */
    int16_t *even_I;      /* [2 * cap]  */
    int16_t *even_Q;      /* [2 * cap]  */
    int16_t *odd_I;       /* [2 * cap]  */
    int16_t *odd_Q;       /* [2 * cap]  */
    size_t   cap;         /* next power-of-2 >= num_taps                   */
    size_t   mask;        /* = cap - 1                                     */
    size_t   even_head;
    size_t   odd_head;

    int      has_pending; /* 1 when a trailing even IQ pair is buffered    */
    int16_t  pending_I;
    int16_t  pending_Q;
} hbdecim_q15_state_t;

/**
 * @brief Allocate and initialise a fixed-point halfband 2:1 decimator.
 *
 * @param num_taps  FIR branch length (number of non-zero symmetric taps,
 *                  NOT the full sparse prototype length).  Must be >= 1.
 * @param h         Float FIR branch coefficients, length num_taps.
 *                  Must be symmetric (h[k] = h[num_taps-1-k]).
 *                  Copied and converted to Q15 with x0.5 scaling.
 * @return Non-NULL on success, NULL on invalid args or allocation failure.
 */
hbdecim_q15_state_t *hbdecim_q15_create(size_t num_taps, const float *h);

/** Free all resources.  NULL is a no-op. */
void hbdecim_q15_destroy(hbdecim_q15_state_t *r);

/** Zero all delay rings and clear the pending-sample flag. */
void hbdecim_q15_reset(hbdecim_q15_state_t *r);

/**
 * @brief Decimate a block of interleaved IQ int16 samples by 2.
 *
 * Input layout:  I0 Q0 I1 Q1 ... (2*n_in  int16_t values).
 * Output layout: I0 Q0 I1 Q1 ... (2*n_out int16_t values), n_out <= n_in/2.
 *
 * If n_in is odd the trailing even IQ pair is buffered and consumed on
 * the next call.  Output buffer must be allocated for at least
 * 2 * ((n_in + 1) / 2) int16_t elements to be safe.
 *
 * @param r       State handle, must be non-NULL.
 * @param in      Interleaved IQ input, 2*n_in elements.
 * @param n_in    Number of complex input samples (IQ pairs).
 * @param out     Output buffer, capacity >= 2*max_out elements.
 * @param max_out Maximum complex output samples to write.
 * @return Number of complex output samples written.
 */
size_t hbdecim_q15_execute(hbdecim_q15_state_t *r,
                           const int16_t *in, size_t n_in,
                           int16_t *out, size_t max_out);

/**
 * @brief Maximum output samples for a given input length.
 *
 * Returns 0 to trigger the lazy-alloc path in the Python glue: the
 * output buffer is sized to n_in on first call (always sufficient for 2:1).
 */
size_t hbdecim_q15_execute_max_out(hbdecim_q15_state_t *r);

/** Always returns 0.5. */
double hbdecim_q15_get_rate(const hbdecim_q15_state_t *r);

/** Returns num_taps as supplied to hbdecim_q15_create. */
size_t hbdecim_q15_get_num_taps(const hbdecim_q15_state_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_Q15_CORE_H */
