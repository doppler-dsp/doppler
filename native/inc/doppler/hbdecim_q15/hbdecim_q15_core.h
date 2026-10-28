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
 * instead of computing Sum `h[k]`*`x[n-k]` for all k, the filter computes
 * Sum `h[k]`*(`x[n-k]` + `x[n-(N-1-k)]`) for k = 0..N/2-1, exploiting
 * `h[k]` = `h[N-1-k]`.  The center tap is a single unconditional right-shift
 * (x0.5, baked in as the polyphase rate identity).
 *
 * On AVX2 the inner loop uses _mm256_madd_epi16 to multiply 16 int16_t
 * coefficient values against 16 int16_t delay-line samples in a single
 * instruction, accumulating into 8 int32_t lanes.  The fold is never formed
 * as an int16_t (that would overflow above -6 dBFS): each 16-sample block
 * is multiplied twice, once against the left slice and once against the
 * reversed right slice, and the two products are summed in int32_t.  I and
 * Q run as two independent madd chains on the same coefficient vector.
 * The int32_t lanes are reduced to int64_t before the final round-and-shift
 * to Q15 output, which saturates to the int16_t range.
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
 *   dp_hbdecim_q15_state_t *r = dp_hbdecim_q15_create(num_taps, h_fir);
 *   // in:  interleaved int16_t IQ, 2*n_in elements
 *   // out: interleaved int16_t IQ, 2*n_out elements (n_out <= n_in/2)
 *   size_t n = dp_hbdecim_q15_execute(r, in, n_in, out, max_out);
 *   dp_hbdecim_q15_destroy(r);
 * @endcode
 */
#ifndef DP_HBDECIM_Q15_CORE_H
#define DP_HBDECIM_Q15_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HalfbandDecimatorQ15 state: Q15 FIR-branch coefficients, four
 *        dual-write int16 delay rings and a one-sample carry.
 *
 * Config fields are set by dp_hbdecim_q15_create(); the ring contents,
 * heads and pending pair are running state.  Allocate with
 * dp_hbdecim_q15_create().
 */
typedef struct {
    size_t   num_taps;    /**< FIR branch length from the caller (config). */
    size_t   K;           /**< Symmetric pair count, num_taps / 2 (config). */
    size_t   K_pad;       /**< K rounded up to a multiple of 16 for AVX2
                               loads (config).                             */
    size_t   centre;      /**< num_taps / 2: delay-branch tap index into its
                               ring (config).                              */
    int      fir_on_even; /**< 1 (num_taps even): FIR branch runs on the
                               even-indexed ring; 0: on the odd ring.      */

    /* Q15 FIR branch coefficients, zero-padded to K_pad entries.
     * Scaled by 0.5 at create() to match the halfband polyphase identity.
     * Allocated with 32-byte alignment for AVX2 aligned loads.            */
    int16_t *coeffs;      /**< Owned, 32-byte aligned, K_pad Q15 taps: the
                               first K of h scaled by 0.5, zero-padded.    */

    /* Dual-write delay rings: even- and odd-indexed input samples split
     * into separate I and Q arrays for contiguous SIMD access.            */
    int16_t *even_I;      /**< Owned ring, 2*cap: I of even-index inputs. */
    int16_t *even_Q;      /**< Owned ring, 2*cap: Q of even-index inputs. */
    int16_t *odd_I;       /**< Owned ring, 2*cap: I of odd-index inputs.  */
    int16_t *odd_Q;       /**< Owned ring, 2*cap: Q of odd-index inputs.  */
    size_t   cap;         /**< Ring capacity: next power of 2 >= num_taps
                               (config).                                   */
    size_t   mask;        /**< cap - 1, the ring index mask (config).      */
    size_t   even_head;   /**< Newest-sample index in the even rings,
                               [0, cap) (running state).                   */
    size_t   odd_head;    /**< Newest-sample index in the odd rings,
                               [0, cap) (running state).                   */

    int      has_pending; /**< 1 when a trailing even IQ pair is buffered
                               for the next execute() (running state).    */
    int16_t  pending_I;   /**< I of the buffered pair; valid if has_pending. */
    int16_t  pending_Q;   /**< Q of the buffered pair; valid if has_pending. */
    size_t   consumed;    /**< Complex input samples the last execute took;
                               per-call, not in the blob.                  */
} dp_hbdecim_q15_state_t;

/**
 * @brief Allocate and initialise a fixed-point halfband 2:1 decimator.
 * The FIR branch coefficients are supplied as float and converted
 * internally to Q15 with a x0.5 polyphase rate scaling.  The full
 * halfband prototype is sparse (every other tap is zero); supply only
 * the non-zero FIR branch taps, not the full sparse prototype.
 * @param num_taps Number of FIR branch coefficients in h (>= 1).
 * @param h        Float FIR branch coefficients of length num_taps.
 *                 Must be symmetric (`h[k]` == `h[num_taps-1-k]`): only
 *                 the first num_taps/2 are read, scaled by 0.5 and
 *                 saturated to Q15.
 * @return Heap-allocated state, or NULL when @p num_taps is 0, @p h is
 *         NULL, or an allocation fails.
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> dec = HalfbandDecimatorQ15(h)
 * >>> dec.num_taps
 * 3
 * >>> dec.rate
 * 0.5
 * @endcode
 */
dp_hbdecim_q15_state_t *dp_hbdecim_q15_create(size_t num_taps, const float *h);

/**
 * @brief Free all heap resources owned by the decimator state.
 * Releases the Q15 coefficient buffer, all four delay rings, and the
 * state struct itself.  Passing NULL is a no-op.  The Python wrapper
 * calls this in __del__ and __exit__; call it explicitly only for
 * deterministic release before GC reclaims the object.
 * @param r  Decimator state, or NULL.
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> with HalfbandDecimatorQ15(h) as dec:
 * ...     y = dec.execute(
 * ...         np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0],
 * ...                  dtype=np.int16))
 * ...     y.dtype
 * dtype('int16')
 * @endcode
 */
void dp_hbdecim_q15_destroy(dp_hbdecim_q15_state_t *r);

/**
 * @brief Zero all delay rings and clear the pending-sample flag.
 * After a reset the decimator behaves identically to a freshly
 * constructed instance: the four dual-write delay rings are zeroed and
 * has_pending is cleared, so no partial IQ pair carries over.  Call
 * this between unrelated signal segments to prevent inter-segment
 * leakage.
 * @param r  Decimator state (non-NULL).
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> dec = HalfbandDecimatorQ15(h)
 * >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
 * >>> _ = dec.execute(x)
 * >>> dec.reset()
 * >>> y = dec.execute(x)
 * >>> y.tolist()
 * [0, 0, 625, 0]
 * @endcode
 */
void dp_hbdecim_q15_reset(dp_hbdecim_q15_state_t *r);

/**
 * @brief Decimate a block of interleaved IQ int16 samples by 2.
 * Input must be interleaved int16_t IQ pairs (I₀ Q₀ I₁ Q₁ …); pass
 * a 1-D array of 2*n_complex elements.  Each pair of complex input
 * samples produces one complex output sample, counting an IQ pair held
 * from the previous call as the first of the block, so an array of length
 * 2N yields at most (N + held) / 2 output pairs.  An IQ pair left unpaired
 * at the end is held for the next call, so contiguous blocks give the same
 * output as one large block.
 * @param r       Decimator state.
 * @param in      Interleaved int16_t IQ input array of 2*n_in elements
 *                (I₀ Q₀ I₁ Q₁ …).
 * @param n_in    Number of complex input pairs (half the int16 element count).
 * @param out     Output buffer; caller must provide space for 2*max_out
 *                int16_t values (one interleaved I/Q pair per output).
 * @param max_out Capacity of out in COMPLEX samples -- half its int16_t
 *                element count.  The loop guards on the complex index, so
 *                a value in int16 units would let it write 2x the buffer.
 * @return        min(available, max_out) COMPLEX samples -- twice that
 *                many int16_t values; 0 when @p n_in or @p max_out is 0.
 *                When @p max_out stops the loop early, the call stops at
 *                the pair boundary and drops nothing:
 *                dp_hbdecim_q15_consumed() says how much input it took,
 *                and offering the rest again resumes exactly
 *                (doppler#2643).
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> dec = HalfbandDecimatorQ15(h)
 * >>> x = np.array([1000, 0, 1000, 0, 1000, 0, 1000, 0], dtype=np.int16)
 * >>> y = dec.execute(x)
 * >>> y.dtype
 * dtype('int16')
 * >>> y.shape
 * (4,)
 * >>> y.tolist()
 * [0, 0, 625, 0]
 * @endcode
 */
size_t dp_hbdecim_q15_execute(dp_hbdecim_q15_state_t *r,
                           const int16_t *in, size_t n_in,
                           int16_t *out, size_t max_out);

/**
 * @brief Complex input samples the last dp_hbdecim_q15_execute() took.
 *
 * Equal to its @p n_in unless @p out filled first; then the caller
 * resumes at in + 2 * consumed int16 elements.  0 after create, reset and
 * set_state.
 *
 * @param r  Must be non-NULL.
 * @return Complex input samples taken by the last execute.
 */
size_t dp_hbdecim_q15_consumed(const dp_hbdecim_q15_state_t *r);

/**
 * @brief Maximum output samples for a given input length.
 *
 * Returns 0 to trigger the lazy-alloc path in the Python glue: the
 * output buffer is sized to n_in on first call (always sufficient for 2:1).
 *
 * @param r  Decimator state (unused).
 * @return Always 0 (no fixed bound; size the output from n_in).
 */
size_t dp_hbdecim_q15_execute_max_out(dp_hbdecim_q15_state_t *r);

/**
 * @brief The sample-rate reduction factor; always 0.5 for 2:1 decimation.
 * Exposed as a read-only property so pipelines can query the rate of
 * each stage programmatically without hard-coding the 2:1 assumption.
 * @param r  Decimator state (unused).
 * @return 0.5 (output rate / input rate).
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> HalfbandDecimatorQ15(h).rate
 * 0.5
 * @endcode
 */
double dp_hbdecim_q15_get_rate(const dp_hbdecim_q15_state_t *r);

/**
 * @brief FIR branch length as supplied to the constructor.
 * This is the count of non-zero symmetric taps in the FIR branch,
 * not the full sparse halfband prototype length.  Useful for introspection
 * when chaining multiple stages with programmatically computed filter banks.
 * @param r  Decimator state (non-NULL).
 * @return num_taps as passed to dp_hbdecim_q15_create().
 * @code
 * >>> import numpy as np
 * >>> from doppler.resample import HalfbandDecimatorQ15
 * >>> h = np.array([0.25, 0.5, 0.25], dtype=np.float32)
 * >>> HalfbandDecimatorQ15(h).num_taps
 * 3
 * @endcode
 */
size_t dp_hbdecim_q15_get_num_taps(const dp_hbdecim_q15_state_t *r);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack four dual-write rings + heads + pending; coeffs restored by create. */
/** @brief State-blob magic ("HB15"); set_state() rejects any other. */
#define HBDECIM_Q15_STATE_MAGIC DP_FOURCC ('H','B','1','5')
/** @brief State-blob layout version; bumped when the payload changes. */
#define HBDECIM_Q15_STATE_VERSION 1u
/**
 * @brief Size of this instance's serialized state blob.
 * @param state  Instance to size (non-NULL).
 * @return Bytes: envelope + two heads + pending flag and pair + the four
 *         2*cap rings.
 */
size_t dp_hbdecim_q15_state_bytes (const dp_hbdecim_q15_state_t *state);
/**
 * @brief Serialize rings, heads and the pending pair into a blob.
 * @param state  Instance to snapshot (non-NULL).
 * @param blob   Destination of at least dp_hbdecim_q15_state_bytes()
 *               bytes.
 */
void dp_hbdecim_q15_get_state (const dp_hbdecim_q15_state_t *state, void *blob);
/**
 * @brief Restore rings, heads and the pending pair from a blob.
 *
 * The instance must have been created with the same num_taps (the size
 * check enforces the ring capacity, not the coefficients).  The restored
 * heads are not range-checked against cap.
 *
 * @param state  Instance to overwrite (non-NULL).
 * @param blob   Blob of dp_hbdecim_q15_state_bytes() bytes.
 * @return DP_OK on success; DP_ERR_INVALID (state untouched) when the
 *         magic, version, endianness or size does not match.
 */
int dp_hbdecim_q15_set_state (dp_hbdecim_q15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_Q15_CORE_H */
