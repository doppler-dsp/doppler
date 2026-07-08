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
#include "dp_state.h"
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
   * @brief Single-sample direct-form FIR step (inline composition API).
   *
   * Filters one sample and advances the delay line: returns
   * `y = sum_k h[k] * x[n-k]` and shifts @p x into the length-`num_taps-1`
   * delay line (dropping the oldest sample). This is the per-sample counterpart
   * to fir_execute() — a tracking receiver inlines it into its own sample loop
   * (e.g. a matched filter feeding a symbol-timing loop) where fir_execute()'s
   * block interface cannot. It mirrors fir_execute()'s real-tap scalar
   * accumulation term for term, so a fir_step() stream matches fir_execute() to
   * within floating-point rounding: equal in exact arithmetic; a contracted FMA
   * can differ by ~1 ULP across translation units, and fir_execute() on a
   * multi-sample block can differ a little more from SIMD reassociation. Cost is
   * `num_taps` MACs plus an O(num_taps) delay-line shift per sample.
   *
   * @note **Real-tap filters only** (fir_create_real). Pulse-shape matched
   *   filters — RRC, raised-cosine, integrate-and-dump — are real-valued, which
   *   is the streaming use case this serves; a complex-tap variant would add a
   *   complex MAC branch and is left until a consumer needs it.
   *
   * @param s  Real-tap filter state (fir_create_real).  Must be non-NULL.
   * @param x  One input sample.
   * @return The filtered output sample.
   */
  JM_FORCEINLINE JM_HOT float complex
  fir_step (fir_state_t *s, float complex x)
  {
    size_t               M = s->num_taps;
    const float complex *d = s->delay;  /* length M-1 (NULL when M == 1) */
    const float         *h = s->rtaps;  /* real taps (fir_create_real)   */
    float                re = 0.0f, im = 0.0f;
    for (size_t k = 0; k < M; k++)
      {
        float complex cf = (k == 0) ? x : d[M - 1 - k];
        re += h[k] * crealf (cf);
        im += h[k] * cimagf (cf);
      }
    if (M > 1)
      {
        float complex *dl = s->delay; /* shift left, append x as newest */
        for (size_t i = 0; i + 2 < M; i++)
          dl[i] = dl[i + 1];
        dl[M - 2] = x;
      }
    return CMPLXF (re, im);
  }

  /**
   * @brief Create a FIR filter from complex CF32 tap coefficients.
   * Implements a direct-form FIR convolution: `y[n]` = sum_k `h[k]`*`x[n-k]`.
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

  /* Serializable state (standard bytes interface; see dp_state.h): the delay
   * line (num_taps-1 samples) after the envelope; taps/scratch are config. */
#define FIR_STATE_MAGIC DP_FOURCC ('F', 'I', 'R', '_')
#define FIR_STATE_VERSION 1u

  /** @brief Bytes fir_get_state() writes for @p state (envelope + payload). */
  size_t fir_state_bytes (const fir_state_t *state);
  /** @brief Serialize @p state's delay line into @p blob. */
  void fir_get_state (const fir_state_t *state, void *blob);
  /** @brief Restore the delay line from @p blob (same num_taps).
   *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
  int fir_set_state (fir_state_t *state, const void *blob);

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
   * @brief Always 0 -- FIR is a 1:1 transform, not a bounded-capacity one.
   *
   * fir_execute() always writes exactly n_in samples; there is no
   * call-independent upper bound smaller than the input length for this
   * function to report. An `out=` buffer must be sized to exactly
   * `len(x)`, not to this function's return value.
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
