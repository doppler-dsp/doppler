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
 * The internal scratch buffer [delay | input] is allocated lazily on the
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
   * @param taps      Pointer to num_taps complex tap coefficients (copied).
   * @param num_taps  Filter length (>= 1).
   * @return Heap-allocated state, or NULL on allocation failure.
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

  /** @brief Zero the delay line; preserve taps and scratch capacity. */
  void fir_reset (fir_state_t *state);

  /** @brief Destroy the filter; safe to pass NULL. */
  void fir_destroy (fir_state_t *state);

  /** @brief Number of tap coefficients. */
  size_t fir_get_num_taps (const fir_state_t *state);

  /** @brief 1 if filter was created with real taps, 0 if complex. */
  int fir_get_is_real (const fir_state_t *state);

  /**
   * @brief Upper bound on execute output samples (always == n_in for FIR).
   *
   * Used by the generated ext.c to size the output buffer.
   * Returns 0 at creation time (n_in unknown); buffer grows on first call.
   */
  size_t fir_execute_max_out (fir_state_t *state);

  /**
   * @brief Filter n_in CF32 samples; write results to out.
   * @param state    Must be non-NULL.
   * @param in       Input array of n_in float complex samples.
   * @param n_in     Number of input samples.
   * @param out      Output array (may alias in for in-place).
   * @return         n_in on success, 0 on allocation failure.
   */
  size_t fir_execute (fir_state_t *state, const float complex *in, size_t n_in,
                      float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* FIR_CORE_H */
