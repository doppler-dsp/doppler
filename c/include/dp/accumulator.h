/**
 * @file accumulator.h
 * @brief General-purpose scalar accumulator for f32 and cf64 signals.
 *
 * Provides a stateful accumulator with scalar, 1-D array, and 2-D array
 * variants for both real (f32) and complex (cf64) data.  Designed as a
 * low-level building block for polyphase resamplers, energy detectors,
 * and any algorithm that needs an ongoing sum across time.
 *
 * ### Operations
 *
 * | Function       | Effect                               |
 * |----------------|--------------------------------------|
 * | push           | acc += x           (one sample)      |
 * | add            | acc += Σ x[k]      (1-D array)       |
 * | madd           | acc += Σ x[k]·h[k] (1-D MAC)         |
 * | add2d          | acc += Σᵢⱼ x[i][j]      (2-D array)  |
 * | madd2d         | acc += Σᵢⱼ x[i][j]·h[i][j] (2-D MAC) |
 * | get            | return current accumulated value     |
 * | reset          | acc = 0                              |
 * | dump           | v = acc; acc = 0; return v           |
 *
 * ### Decimation example
 *
 * ```c
 * #include <dp/accumulator.h>
 * #include <dp/nco.h>
 *
 * // Polyphase decimator: NCO runs at input rate, dump on overflow.
 * dp_acc_cf64_t *acc = dp_acc_cf64_create();
 * dp_nco_t      *nco = dp_nco_create(fs_in / fs_out);  // dec ratio
 *
 * for (size_t n = 0; n < num_input; n++) {
 *     uint32_t phase = dp_nco_phase(nco);
 *     size_t   branch = phase >> (32 - 12);  // 4096 phases
 *     const float *h = polyphase_bank[branch];
 *     dp_acc_cf64_madd(acc, &delay_line[n], h, num_taps);
 *     if (dp_nco_tick(nco))  // returns 1 on overflow
 *         output[out++] = dp_acc_cf64_dump(acc);
 * }
 *
 * dp_acc_cf64_destroy(acc);
 * dp_nco_destroy(nco);
 * ```
 */

#ifndef DP_ACCUMULATOR_H
#define DP_ACCUMULATOR_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------
   * Opaque types
   * ------------------------------------------------------------------ */

  /** @brief Stateful real f32 accumulator. */
  typedef struct dp_acc_f32  dp_acc_f32_t;

  /** @brief Stateful complex cf64 accumulator. */
  typedef struct dp_acc_cf64 dp_acc_cf64_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Allocate and zero a real f32 accumulator.
   * @return Heap-allocated accumulator, or NULL on failure.
   */
  dp_acc_f32_t *dp_acc_f32_create (void);

  /**
   * @brief Allocate and zero a complex cf64 accumulator.
   * @return Heap-allocated accumulator, or NULL on failure.
   */
  dp_acc_cf64_t *dp_acc_cf64_create (void);

  /**
   * @brief Free a real f32 accumulator.
   * @param acc  May be NULL (no-op).
   */
  void dp_acc_f32_destroy (dp_acc_f32_t *acc);

  /**
   * @brief Free a complex cf64 accumulator.
   * @param acc  May be NULL (no-op).
   */
  void dp_acc_cf64_destroy (dp_acc_cf64_t *acc);

  /* ------------------------------------------------------------------
   * Reset / dump
   * ------------------------------------------------------------------ */

  /**
   * @brief Zero the f32 accumulator without freeing it.
   * @param acc  Must be non-NULL.
   */
  void dp_acc_f32_reset (dp_acc_f32_t *acc);

  /**
   * @brief Zero the cf64 accumulator without freeing it.
   * @param acc  Must be non-NULL.
   */
  void dp_acc_cf64_reset (dp_acc_cf64_t *acc);

  /**
   * @brief Return the current f32 value then reset to zero.
   *
   * Equivalent to get() followed by reset().  The canonical "dump"
   * operation for the polyphase decimation overflow event.
   *
   * @param acc  Must be non-NULL.
   * @return     Accumulated value before the reset.
   */
  float dp_acc_f32_dump (dp_acc_f32_t *acc);

  /**
   * @brief Return the current cf64 value then reset to zero.
   *
   * @param acc  Must be non-NULL.
   * @return     Accumulated value before the reset.
   */
  dp_cf64_t dp_acc_cf64_dump (dp_acc_cf64_t *acc);

  /* ------------------------------------------------------------------
   * Read
   * ------------------------------------------------------------------ */

  /**
   * @brief Return the current f32 accumulated value (non-destructive).
   * @param acc  Must be non-NULL.
   */
  float dp_acc_f32_get (const dp_acc_f32_t *acc);

  /**
   * @brief Return the current cf64 accumulated value (non-destructive).
   * @param acc  Must be non-NULL.
   */
  dp_cf64_t dp_acc_cf64_get (const dp_acc_cf64_t *acc);

  /* ------------------------------------------------------------------
   * Scalar push
   * ------------------------------------------------------------------ */

  /**
   * @brief Add a single f32 sample: acc += x.
   * @param acc  Must be non-NULL.
   * @param x    Value to add.
   */
  void dp_acc_f32_push (dp_acc_f32_t *acc, float x);

  /**
   * @brief Add a single cf64 sample: acc += x.
   * @param acc  Must be non-NULL.
   * @param x    Complex value to add.
   */
  void dp_acc_cf64_push (dp_acc_cf64_t *acc, dp_cf64_t x);

  /* ------------------------------------------------------------------
   * 1-D array operations
   * ------------------------------------------------------------------ */

  /**
   * @brief Accumulate a 1-D f32 array: acc += Σ x[k], k=0..n-1.
   *
   * Inner loop is written to allow auto-vectorisation.
   *
   * @param acc  Must be non-NULL.
   * @param x    Input array of @p n floats.
   * @param n    Number of elements.
   */
  void dp_acc_f32_add (dp_acc_f32_t *acc,
                       const float *x, size_t n);

  /**
   * @brief Accumulate a 1-D cf64 array: acc += Σ x[k], k=0..n-1.
   *
   * @param acc  Must be non-NULL.
   * @param x    Input array of @p n dp_cf64_t samples.
   * @param n    Number of complex samples.
   */
  void dp_acc_cf64_add (dp_acc_cf64_t *acc,
                        const dp_cf64_t *x, size_t n);

  /**
   * @brief 1-D multiply-accumulate (MAC) for f32:
   *        acc += Σ x[k]·h[k], k=0..n-1.
   *
   * @param acc  Must be non-NULL.
   * @param x    Signal array  (@p n floats).
   * @param h    Coefficient array (@p n floats).
   * @param n    Number of elements.
   */
  void dp_acc_f32_madd (dp_acc_f32_t *acc,
                        const float * restrict x,
                        const float * restrict h,
                        size_t n);

  /**
   * @brief 1-D multiply-accumulate (MAC) for cf64 × real h:
   *        acc += Σ x[k]·h[k], k=0..n-1.
   *
   * Complex samples are multiplied by real coefficients, matching the
   * typical polyphase FIR structure where taps are real-valued.
   *
   * @param acc  Must be non-NULL.
   * @param x    Complex signal array  (@p n dp_cf64_t samples).
   * @param h    Real coefficient array (@p n floats).
   * @param n    Number of samples / taps.
   */
  void dp_acc_cf64_madd (dp_acc_cf64_t *acc,
                         const dp_cf64_t * restrict x,
                         const float    * restrict h,
                         size_t n);

  /* ------------------------------------------------------------------
   * 2-D array operations
   * ------------------------------------------------------------------ */

  /**
   * @brief Accumulate a row-major 2-D f32 array:
   *        acc += Σᵢ Σⱼ x[i][j].
   *
   * Equivalent to dp_acc_f32_add(acc, x, rows*cols) for contiguous
   * arrays; provided for clarity at call sites using 2-D data.
   *
   * @param acc   Must be non-NULL.
   * @param x     Row-major array of @p rows × @p cols floats.
   * @param rows  Number of rows.
   * @param cols  Number of columns.
   */
  void dp_acc_f32_add2d (dp_acc_f32_t *acc,
                         const float *x,
                         size_t rows, size_t cols);

  /**
   * @brief Accumulate a row-major 2-D cf64 array:
   *        acc += Σᵢ Σⱼ x[i][j].
   *
   * @param acc   Must be non-NULL.
   * @param x     Row-major array of @p rows × @p cols dp_cf64_t samples.
   * @param rows  Number of rows.
   * @param cols  Number of columns.
   */
  void dp_acc_cf64_add2d (dp_acc_cf64_t *acc,
                          const dp_cf64_t *x,
                          size_t rows, size_t cols);

  /**
   * @brief 2-D multiply-accumulate for f32:
   *        acc += Σᵢ Σⱼ x[i][j]·h[i][j].
   *
   * Both arrays must be row-major with identical dimensions.
   *
   * @param acc   Must be non-NULL.
   * @param x     Signal matrix     (@p rows × @p cols floats).
   * @param h     Coefficient matrix (@p rows × @p cols floats).
   * @param rows  Number of rows.
   * @param cols  Number of columns.
   */
  void dp_acc_f32_madd2d (dp_acc_f32_t *acc,
                          const float * restrict x,
                          const float * restrict h,
                          size_t rows, size_t cols);

  /**
   * @brief 2-D multiply-accumulate for cf64 × real h:
   *        acc += Σᵢ Σⱼ x[i][j]·h[i][j].
   *
   * @param acc   Must be non-NULL.
   * @param x     Complex signal matrix  (@p rows × @p cols dp_cf64_t).
   * @param h     Real coefficient matrix (@p rows × @p cols floats).
   * @param rows  Number of rows.
   * @param cols  Number of columns.
   */
  void dp_acc_cf64_madd2d (dp_acc_cf64_t *acc,
                           const dp_cf64_t * restrict x,
                           const float     * restrict h,
                           size_t rows, size_t cols);

#ifdef __cplusplus
}
#endif

#endif /* DP_ACCUMULATOR_H */
