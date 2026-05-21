/**
 * @file corr_core.h
 * @brief 1-D FFT-based cross-correlator with coherent integrate-and-dump.
 *
 * Implements the correlation theorem: cross-correlation in the lag domain is
 * equivalent to pointwise multiplication of the forward spectrum with the
 * conjugate reference spectrum, followed by an inverse FFT.
 *
 *   R_xh[τ] = IFFT( FFT(x) · conj(FFT(h)) ) / n
 *
 * The reference spectrum ``conj(FFT(h))`` is pre-computed at create time and
 * stored in ``ref_spec``, so each execute call costs two FFTs (forward +
 * inverse) plus n complex multiplies — O(n log n).
 *
 * Integrate-and-dump (int-dump) coherently sums ``dwell`` successive
 * correlation maps into an accumulator.  On the ``dwell``-th call execute()
 * copies the accumulator to the caller's output buffer, zeroes the
 * accumulator, resets the counter, and returns ``n``.  All other calls return
 * 0 (no output produced).  With ``dwell = 1`` the object is a pure, zero-
 * latency correlator.
 *
 * Lifecycle:
 * @code
 * float complex ref[N] = { ... };
 * corr_state_t *c = corr_create(ref, N, 8, 1);   // 8-frame coherent dwell
 * float complex out[N];
 * for (int i = 0; i < 8; i++) {
 *     size_t n_out = corr_execute(c, frame[i], N, out);
 *     if (n_out) process(out, N);   // fires once, on i == 7
 * }
 * corr_destroy(c);
 * @endcode
 *
 * Thread safety: a single state must not be used concurrently from multiple
 * threads; create separate instances per thread.
 */
#ifndef CORR_CORE_H
#define CORR_CORE_H

#include "clib_common.h"
#include "fft/fft_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 1-D FFT correlator state.
 *
 * Allocate with corr_create(); never stack-allocate.  The four heap buffers
 * (ref_spec, work_fft, work_ifft, accum) are each ``n`` complex floats.
 */
typedef struct {
  fft_state_t *fwd;         /**< Forward plan (sign = -1). */
  fft_state_t *inv;         /**< Inverse plan (sign = +1). */
  float complex *ref_spec;  /**< conj(FFT(ref)), pre-computed at create. */
  float complex *work_fft;  /**< Scratch: FFT(in) output.               */
  float complex *work_ifft; /**< Scratch: IFFT output before accumulate. */
  float complex *accum;     /**< Coherent integration accumulator.       */
  size_t n;                 /**< FFT / reference length (samples).       */
  size_t dwell;             /**< Integration depth; dump every dwell calls. */
  size_t count;             /**< Frames accumulated so far (0 … dwell-1). */
} corr_state_t;

/**
 * @brief Create a 1-D FFT correlator.
 *
 * Allocates two FFT plans, computes ``conj(FFT(ref))`` once, and
 * zeroes the accumulator.  The caller may free or reuse ``ref`` after
 * this call returns.
 *
 * @param ref       Reference signal of length @p n (CF32, row-major).
 * @param n         Transform / reference length in samples.
 * @param dwell     Integration depth.  Must be >= 1.  Pass 1 for pure
 *                  correlation (no accumulation).
 * @param nthreads  Accepted for API compatibility; ignored (pocketfft is
 *                  single-threaded).
 * @return Heap-allocated state, or NULL on allocation failure.
 */
corr_state_t *corr_create(const float complex *ref, size_t n, size_t dwell,
                          int nthreads);

/** @brief Destroy and free a corr instance.  @param state May be NULL. */
void corr_destroy(corr_state_t *state);

/**
 * @brief Zero the accumulator and reset the integration counter to 0.
 *
 * Equivalent to starting a fresh dwell cycle.  Does NOT recompute
 * ref_spec; use corr_set_ref() for that.
 *
 * @param state Must be non-NULL.
 */
void corr_reset(corr_state_t *state);

/**
 * @brief Replace the reference signal and recompute conj(FFT(ref)).
 *
 * Also resets the accumulator and counter (as if corr_reset() were
 * called).  Useful when the reference must change between dwells without
 * tearing down the FFT plans.
 *
 * @param state Must be non-NULL.
 * @param ref   New reference signal of length state->n.
 */
void corr_set_ref(corr_state_t *state, const float complex *ref);

/** @brief Maximum output samples per execute call (always == n). */
size_t corr_execute_max_out(corr_state_t *state);

/**
 * @brief Correlate one frame and optionally dump the accumulator.
 *
 * Steps:
 *   1. FFT(in) → work_fft
 *   2. work_fft[k] *= ref_spec[k]  (frequency-domain multiplication)
 *   3. IFFT(work_fft) → work_ifft  (unnormalized; divide by n)
 *   4. accum[k] += work_ifft[k] / n
 *   5. count++
 *   6. If count == dwell: copy accum → out, zero accum, reset count,
 *      return n.
 *   7. Otherwise: return 0 (no output this call).
 *
 * @param state Must be non-NULL.
 * @param in    Input frame of length n_in (must equal state->n).
 * @param n_in  Number of input samples; ignored beyond state->n.
 * @param out   Output buffer of length >= state->n.  Only written when
 *              the function returns n (dump).
 * @return n on a dump, 0 otherwise.
 */
size_t corr_execute(corr_state_t *state, const float complex *in, size_t n_in,
                    float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* CORR_CORE_H */
