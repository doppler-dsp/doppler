/**
 * @file fir_core.h
 * @brief Direct-form FIR filter — real-tap and complex-tap variants.
 *
 * Two constructors select the tap type at creation time:
 *
 *   fir_create()      — complex CF32 taps (general case)
 *   fir_create_real() — real float taps   (1 FMA/tap; use for real-valued
 * designs)
 *
 * All execute functions accept CF32 input and write CF32 output.
 * The internal scratch buffer (delay + input) is allocated lazily on the
 * first execute call and grown as needed.
 *
 * @code
 * float taps[63] = { ... };
 * fir_state_t *fir = fir_create_real(taps, 63);
 * float complex out[4096];
 * fir_execute(fir, signal, 4096, out);
 * fir_destroy(fir);
 * @endcode
 */
#ifndef FIR_CORE_H
#define FIR_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float complex *taps;    /* complex taps  (NULL for real-tap filter)   */
    float *rtaps;           /* real taps     (NULL for complex-tap filter) */
    float complex *delay;   /* delay line, length num_taps - 1            */
    float complex *scratch; /* [delay | input] workspace, grown on demand  */
    size_t scratch_cap;
    size_t num_taps;
  } fir_state_t;

  /**
   * @brief Create a FIR filter from complex CF32 tap coefficients.
   * Implements a direct-form FIR convolution: y[n] = sum_k h[k]*x[n-k].
   * The tap array is copied at creation; the caller may free it afterward.
   * Use fir_create_real() instead when all imaginary parts are zero —
   * that path costs 1 FMA/tap versus 2 FMA + permute + mul here.
   * @param taps     Array of num_taps CF32 coefficients (I+jQ each), copied.
   * @param num_taps Filter length (>= 1).
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> fir = FIR(taps)
   * >>> fir.num_taps
   * 3
   * >>> fir.is_real
   * False
   * @endcode
   */
  fir_state_t *fir_create (const float complex *taps, size_t num_taps);

  /**
   * @brief Create a FIR filter from real float tap coefficients.
   *
   * Real taps cost 1 FMA/tap instead of 2 FMA + permute + mul.
   * Use for filters designed with e.g. scipy.signal.firwin.
   *
   * @param taps      Pointer to num_taps real tap coefficients (copied).
   * @param num_taps  Filter length (>= 1).
   * @return Heap-allocated state, or NULL on allocation failure.
   */
  fir_state_t *fir_create_real (const float *taps, size_t num_taps);

  /**
   * @brief Zero the delay line; preserve taps and scratch capacity.
   * After a reset the filter behaves identically to a freshly constructed
   * instance of the same length, without paying the allocation cost again.
   * Call this between unrelated signal segments to prevent inter-segment
   * leakage through the delay line.
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> fir = FIR(taps)
   * >>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
   * >>> _ = fir.execute(x)
   * >>> fir.reset()
   * >>> y = fir.execute(x)
   * >>> [round(float(v.real), 4) for v in y]
   * [0.25, 0.5, 0.25]
   * @endcode
   */
  void fir_reset (fir_state_t *state);

  /**
   * @brief Release all heap resources owned by the filter state.
   * Frees the tap array, delay line, and scratch buffer, then the state
   * struct itself.  Passing NULL is a no-op.  The Python wrapper calls
   * this automatically in __del__ and __exit__; call it explicitly only
   * when you want deterministic resource release before GC.
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> with FIR(taps) as fir:
   * ...     y = fir.execute(1.0+0j)
   * ...     y.dtype
   * dtype('complex64')
   * @endcode
   */
  void fir_destroy (fir_state_t *state);

  /**
   * @brief Number of tap coefficients supplied at creation.
   * This equals the filter group delay plus one, and determines the
   * minimum input block length for which no latency is observable.
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> FIR(taps).num_taps
   * 3
   * @endcode
   */
  size_t fir_get_num_taps (const fir_state_t *state);

  /**
   * @brief True when the filter was created with real-valued tap coefficients.
   * Real-tap filters (fir_create_real) use a cheaper inner loop: 1 FMA/tap
   * versus the 2 FMA + lane permute required for complex multiplication.
   * Use this flag to confirm which constructor path was used at runtime.
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> FIR(taps).is_real
   * False
   * @endcode
   */
  int fir_get_is_real (const fir_state_t *state);

  /**
   * @brief Upper bound on execute output samples (always == n_in for FIR).
   *
   * Used by the generated ext.c to size the output buffer.
   * Returns 0 at creation time (n_in unknown); buffer grows on first call.
   */
  size_t fir_execute_max_out (fir_state_t *state);

  /**
   * @brief Filter n_in CF32 samples and write the results to out.
   * Each output sample is the inner product of the tap vector with the
   * current delay line.  The delay line is updated with each input sample
   * so state carries over across successive calls — process frames of any
   * size without gaps or overlap.  The scratch buffer is grown lazily on
   * the first call and reused on subsequent calls of the same size.
   * @param state Filter state (delay line + taps).
   * @param in    Input array of n_in CF32 samples.
   * @param n_in  Number of input samples to process.
   * @param out   Output buffer; caller must provide space for n_in CF32 values.
   * @return      Number of output samples written (always == n_in).
   * @code
   * >>> import numpy as np
   * >>> from doppler.filter import FIR
   * >>> taps = np.array([0.25+0j, 0.5+0j, 0.25+0j], dtype=np.complex64)
   * >>> fir = FIR(taps)
   * >>> x = np.array([1+0j, 0+0j, 0+0j], dtype=np.complex64)
   * >>> y = fir.execute(x)
   * >>> y.dtype
   * dtype('complex64')
   * >>> y.shape
   * (3,)
   * >>> [round(float(v.real), 4) for v in y]
   * [0.25, 0.5, 0.25]
   * @endcode
   */
  size_t fir_execute (fir_state_t *state, const float complex *in, size_t n_in,
                      float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FIR_CORE_H */
