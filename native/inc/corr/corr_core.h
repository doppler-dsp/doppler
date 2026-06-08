/**
 * @file corr_core.h
 * @brief 1-D FFT-based cross-correlator with coherent integrate-and-dump.
 *
 * Implements the correlation theorem: cross-correlation in the lag domain is
 * equivalent to pointwise multiplication of the forward spectrum with the
 * conjugate reference spectrum, followed by an inverse FFT.
 *
 *   `R_xh[τ] = IFFT( FFT(x) · conj(FFT(h)) ) / n`
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
 * @brief Allocate a 1-D FFT correlator with coherent integrate-and-dump.
 * Pre-computes conj(FFT(ref)) once at construction so each execute()
 * call costs only two FFTs and n complex multiplies.  @p ref may be freed
 * after this returns.  With @p dwell == 1 every call produces output; with
 * larger values the accumulator absorbs @p dwell frames before dumping.
 *
 * @param ref       Reference signal, CF32, length @p n.
 * @param n         Reference / FFT length in samples.
 * @param dwell     Integration depth; must be >= 1.  Pass 1 for immediate
 *                  output on every call.
 * @param nthreads  Accepted for API compatibility; ignored.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @code
 * >>> from doppler.spectral import Corr
 * >>> import numpy as np
 * >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
 * >>> corr = Corr(ref=ref, dwell=1, nthreads=1)
 * >>> corr.n, corr.dwell, corr.count
 * (4, 1, 0)
 * @endcode
 */
corr_state_t *corr_create(const float complex *ref, size_t n, size_t dwell,
                          int nthreads);

/** @brief Destroy and free a corr instance.  @param state May be NULL. */
void corr_destroy(corr_state_t *state);

/**
 * @brief Zero the accumulator and reset the integration counter to 0.
 * Equivalent to starting a fresh dwell cycle without tearing down the FFT
 * plans.  Does NOT recompute ref_spec; use corr_set_ref() to replace the
 * reference.
 *
 * @code
 * >>> from doppler.spectral import Corr
 * >>> import numpy as np
 * >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
 * >>> corr = Corr(ref=ref, dwell=3)
 * >>> _ = corr.execute(np.ones(4, dtype=np.complex64))
 * >>> corr.count
 * 1
 * >>> corr.reset()
 * >>> corr.count
 * 0
 * @endcode
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
 * @brief Correlate one frame and optionally dump the coherent accumulator.
 * Runs the six-step pipeline: forward FFT → pointwise multiply with
 * ref_spec → inverse FFT → normalise (÷ n) → accumulate → conditional dump.
 * On the @p dwell-th call the accumulator is copied to @p out, zeroed, and
 * the counter resets; the function returns n.  All other calls return 0 and
 * leave @p out unmodified.  In Python, a dump returns an ndarray and a
 * no-dump returns None.
 *
 * @param state  Allocated correlator (non-NULL).
 * @param in     Input frame, CF32, length state->n.
 * @param n_in   Number of input samples; must equal state->n.
 * @param out    Output buffer for the correlation map (CF32, length state->n);
 *               written only on a dump call.
 * @return n on a dump call, 0 otherwise (None in Python).
 * @code
 * >>> from doppler.spectral import Corr
 * >>> import numpy as np
 * >>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
 * >>> corr = Corr(ref=ref, dwell=2)
 * >>> x = np.ones(4, dtype=np.complex64)
 * >>> corr.execute(x) is None   # frame 1 — no dump yet
 * True
 * >>> corr.execute(x).tolist()  # frame 2 — dump
 * [(2+0j), (2+0j), (2+0j), (2+0j)]
 * @endcode
 */
size_t corr_execute(corr_state_t *state, const float complex *in, size_t n_in,
                    float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* CORR_CORE_H */
