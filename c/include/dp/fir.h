/**
 * @file fir.h
 * @brief FIR filter with SIMD-accelerated hot loops (real and complex taps).
 *
 * Implements direct-form FIR filtering for IQ signal streams.
 * Two filter variants share the same lifecycle (reset/destroy) API:
 *
 *   dp_fir_create()      — complex CF32 taps (general case)
 *   dp_fir_create_real() — real float taps   (DDC/DUC common case)
 *
 * Real-tap filters are the norm in DDC/DUC applications: CIC compensation,
 * root-raised-cosine, channel-select LPF.  Using real taps costs 1 FMA per
 * tap instead of 2 FMA + permute + mul, cutting the multiply count in half.
 *
 * Computation precision is CF32 throughout.  Integer inputs (CI8, CI16,
 * CI32) are upcasted to CF32 in the hot loop via AVX-512 integer converts.
 *
 * SIMD dispatch (compile-time):
 *   - Complex taps: AVX-512F+DQ — 8 complex outputs/iteration
 *   - Real taps:    AVX-512F    — 8 complex outputs/iteration
 *   - Scalar fallback always available
 *
 * ```c
 * #include <dp/fir.h>
 *
 * // DDC low-pass: real taps, CI16 input (LimeSDR/USRP)
 * float taps[63] = { ... };   // designed with scipy.signal.firwin
 * dp_fir_t *fir = dp_fir_create_real(taps, 63);
 *
 * dp_ci16_t raw[4096];
 * dp_cf32_t out[4096];
 * dp_fir_execute_real_ci16(fir, raw, out, 4096);
 *
 * // Complex taps (e.g. frequency-shifted filter, Hilbert transformer)
 * dp_cf32_t ctaps[63] = { ... };
 * dp_fir_t *cfir = dp_fir_create(ctaps, 63);
 * dp_fir_execute_cf32(cfir, out, out, 4096);
 *
 * dp_fir_destroy(fir);
 * dp_fir_destroy(cfir);
 * ```
 */

#ifndef DP_FIR_H
#define DP_FIR_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque FIR filter state. */
  typedef struct dp_fir dp_fir_t;

  /* ------------------------------------------------------------------
   * Lifecycle — shared by both real-tap and complex-tap filters
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a complex FIR filter from CF32 tap coefficients.
   *
   * The filter allocates an internal delay line of @p num_taps − 1
   * complex samples, initialised to zero.  A scratch buffer is
   * allocated lazily on the first call to an execute function and
   * reused on subsequent calls.
   *
   * @param taps      Pointer to @p num_taps complex tap coefficients.
   * @param num_taps  Number of taps (filter length ≥ 1).
   * @return          Heap-allocated filter state, or NULL on failure.
   */
  dp_fir_t *dp_fir_create (const dp_cf32_t *taps, size_t num_taps);

  /**
   * @brief Create a real-coefficient FIR filter for complex (IQ) signals.
   *
   * Real taps are the common case in DDC/DUC: the filter is designed in
   * the real domain (e.g. scipy.signal.firwin) and applied to complex IQ
   * samples.  Using real taps costs 1 FMA per tap instead of 2 FMA +
   * permute + mul, halving the multiply count vs dp_fir_create() with
   * zero-imaginary coefficients.
   *
   * Use dp_fir_execute_real_*() to run the filter.  dp_fir_reset() and
   * dp_fir_destroy() work identically for both real and complex filters.
   *
   * @param taps      Pointer to @p num_taps real-valued tap coefficients.
   * @param num_taps  Number of taps (filter length ≥ 1).
   * @return          Heap-allocated filter state, or NULL on failure.
   */
  dp_fir_t *dp_fir_create_real (const float *taps, size_t num_taps);

  /**
   * @brief Zero the delay line without freeing the filter.
   *
   * Use after a stream discontinuity to prevent history contamination.
   *
   * @param f  Filter state (must be non-NULL).
   */
  void dp_fir_reset (dp_fir_t *f);

  /**
   * @brief Destroy the filter and release all associated memory.
   * @param f  Filter state (may be NULL).
   */
  void dp_fir_destroy (dp_fir_t *f);

  /* ------------------------------------------------------------------
   * Execute — CF32 input (native compute type)
   * ------------------------------------------------------------------ */

  /**
   * @brief Filter an array of CF32 complex samples.
   *
   * Dispatches to the widest available SIMD path at compile time
   * (AVX-512 → scalar).  Processes 8 output samples per inner
   * iteration on AVX-512-capable hardware.
   *
   * @param f           Filter state.
   * @param in          Input array of @p num_samples CF32 samples.
   * @param out         Output array (may alias @p in for in-place use).
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_cf32 (dp_fir_t *f, const dp_cf32_t *in, dp_cf32_t *out,
                           size_t num_samples);

  /* ------------------------------------------------------------------
   * Execute — compact IQ inputs (upcasting hot paths)
   * ------------------------------------------------------------------ */

  /**
   * @brief Filter CI8 IQ samples, writing CF32 output.
   *
   * Upcasts int8_t I/Q to float on the fly before filtering.
   * Designed for RTL-SDR and HackRF (2 bytes/sample → 32 AVX-512
   * lanes per register before conversion).
   *
   * @param f           Filter state.
   * @param in          Input array of @p num_samples CI8 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_ci8 (dp_fir_t *f, const dp_ci8_t *in, dp_cf32_t *out,
                          size_t num_samples);

  /**
   * @brief Filter CI16 IQ samples, writing CF32 output.
   *
   * Upcasts int16_t I/Q to float on the fly before filtering.
   * Designed for LimeSDR, USRP, and PlutoSDR (4 bytes/sample →
   * 16 AVX-512 lanes per register before conversion).
   *
   * @param f           Filter state.
   * @param in          Input array of @p num_samples CI16 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_ci16 (dp_fir_t *f, const dp_ci16_t *in, dp_cf32_t *out,
                           size_t num_samples);

  /**
   * @brief Filter CI32 IQ samples, writing CF32 output.
   *
   * Upcasts int32_t I/Q to float on the fly before filtering.
   *
   * @param f           Filter state.
   * @param in          Input array of @p num_samples CI32 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_ci32 (dp_fir_t *f, const dp_ci32_t *in, dp_cf32_t *out,
                           size_t num_samples);

  /* ------------------------------------------------------------------
   * Execute — real-tap paths (DDC/DUC common case)
   * ------------------------------------------------------------------ */

  /**
   * @brief Filter CF32 IQ samples through a real-tap filter.
   *
   * @param f           Real-tap filter (from dp_fir_create_real).
   * @param in          Input array of @p num_samples CF32 samples.
   * @param out         Output array (may alias @p in for in-place use).
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_real_cf32 (dp_fir_t *f, const dp_cf32_t *in,
                                dp_cf32_t *out, size_t num_samples);

  /**
   * @brief Filter CI8 IQ samples through a real-tap filter, output CF32.
   *
   * @param f           Real-tap filter (from dp_fir_create_real).
   * @param in          Input array of @p num_samples CI8 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_real_ci8 (dp_fir_t *f, const dp_ci8_t *in, dp_cf32_t *out,
                               size_t num_samples);

  /**
   * @brief Filter CI16 IQ samples through a real-tap filter, output CF32.
   *
   * @param f           Real-tap filter (from dp_fir_create_real).
   * @param in          Input array of @p num_samples CI16 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_real_ci16 (dp_fir_t *f, const dp_ci16_t *in,
                                dp_cf32_t *out, size_t num_samples);

  /**
   * @brief Filter CI32 IQ samples through a real-tap filter, output CF32.
   *
   * @param f           Real-tap filter (from dp_fir_create_real).
   * @param in          Input array of @p num_samples CI32 samples.
   * @param out         Output array of CF32 samples.
   * @param num_samples Number of complex samples to process.
   * @return            DP_OK on success, DP_ERR_MEMORY on alloc fail.
   */
  int dp_fir_execute_real_ci32 (dp_fir_t *f, const dp_ci32_t *in,
                                dp_cf32_t *out, size_t num_samples);

#ifdef __cplusplus
}
#endif

#endif /* DP_FIR_H */
