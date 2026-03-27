/**
 * @file resamp_dpmfs.h
 * @brief DPMFS polyphase resampler for cf32 IQ samples.
 *
 * The Dual Phase Modified Farrow Structure (DPMFS) replaces the large
 * polyphase coefficient table with a compact polynomial bank
 * (M+1)*N*2 float32 values (608 bytes for M=3, N=19) that fits
 * entirely in L1 cache, eliminating table-lookup cache misses.
 *
 * ### Coefficient bank
 *
 * Two sets of (M+1)*N float32 coefficients, supplied row-major [m][k]:
 *   c0: coefficients for j=0 (μ ∈ [0, 0.5))
 *   c1: coefficients for j=1 (μ ∈ [0.5, 1.0))
 *
 * Stored row-major [m*N+k] as supplied.  Internally the hot path
 * runs M vectorised FMA passes over c[j][m*N..] to build h_eff[N],
 * then calls a single dot_cf32_f32, for (M+2)×N real multiplies
 * vs (2(M+1)+2/N)×N with the v[m] form.
 *
 * At phase word p (uint32):
 *   j    = p >> 31                          (0 or 1)
 *   μ_J  = (p & 0x7FFFFFFF) / 2^31         (in [0, 1))
 *   h[k] = Horner(c[j][0..M][k], μ_J)
 *
 * Use @c doppler.polyphase.fit_dpmfs (Python) to generate c0/c1
 * from a Kaiser polyphase bank.
 *
 * ### Rate parameter
 *
 * Same convention as dp_resamp_cf32:
 *   rate > 1 → interpolation, rate < 1 → decimation.
 *
 * ### Usage
 *
 * ```c
 * #include <dp/resamp_dpmfs.h>
 *
 * // c0, c1: (M+1)*N float arrays from doppler.polyphase.fit_dpmfs
 * dp_resamp_dpmfs_t *r = dp_resamp_dpmfs_create(3, 19, c0, c1, 2.0);
 *
 * dp_cf32_t out[2 * IN_LEN + 4];
 * size_t n = dp_resamp_dpmfs_execute(r, in, IN_LEN, out,
 *                                    sizeof(out)/sizeof(out[0]));
 * dp_resamp_dpmfs_destroy(r);
 * ```
 */

#ifndef DP_RESAMP_DPMFS_H
#define DP_RESAMP_DPMFS_H

#include <dp/stream.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque DPMFS resampler state. */
  typedef struct dp_resamp_dpmfs dp_resamp_dpmfs_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a DPMFS polyphase resampler.
   *
   * The coefficient arrays are copied internally; the caller may free
   * them immediately after this call returns.
   *
   * M must be in [1, 3].
   *
   * @param M        Polynomial order (typically 3).
   * @param N        Taps per phase.
   * @param c0       (M+1)*N float32 coefficients for j=0,
   *                 row-major [m][k].
   * @param c1       (M+1)*N float32 coefficients for j=1,
   *                 row-major [m][k].
   * @param rate     fs_out / fs_in.  Must be > 0.
   * @return         Heap-allocated resampler, or NULL on failure.
   */
  dp_resamp_dpmfs_t *dp_resamp_dpmfs_create (size_t M, size_t N,
                                             const float *c0, const float *c1,
                                             double rate);

  /**
   * @brief Free a DPMFS resampler.
   * @param r  May be NULL (no-op).
   */
  void dp_resamp_dpmfs_destroy (dp_resamp_dpmfs_t *r);

  /**
   * @brief Zero the sample history and reset the phase accumulator.
   * @param r  Must be non-NULL.
   */
  void dp_resamp_dpmfs_reset (dp_resamp_dpmfs_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  /** @brief Return the rate (fs_out / fs_in). */
  double dp_resamp_dpmfs_rate (const dp_resamp_dpmfs_t *r);

  /** @brief Return the taps per phase (N). */
  size_t dp_resamp_dpmfs_num_taps (const dp_resamp_dpmfs_t *r);

  /** @brief Return the polynomial order (M). */
  size_t dp_resamp_dpmfs_poly_order (const dp_resamp_dpmfs_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  /**
   * @brief Resample a block of cf32 IQ samples.
   *
   * Processes @p num_in input samples and writes at most @p max_out
   * output samples.  Internal state is preserved across calls.
   *
   * @param r        Must be non-NULL.
   * @param in       Input sample array (may be NULL if num_in == 0).
   * @param num_in   Number of input samples.
   * @param out      Output sample buffer (must hold >= max_out samples).
   * @param max_out  Capacity of @p out in samples.
   * @return         Number of output samples written.
   */
  size_t dp_resamp_dpmfs_execute (dp_resamp_dpmfs_t *r, const dp_cf32_t *in,
                                  size_t num_in, dp_cf32_t *out,
                                  size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_RESAMP_DPMFS_H */
