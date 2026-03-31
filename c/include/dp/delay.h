/**
 * @file delay.h
 * @brief Dual-buffer circular delay line for cf64 IQ samples.
 *
 * Implements the sample history required by a polyphase FIR resampler.
 * The dual-buffer trick keeps the most-recent @p num_taps samples in a
 * contiguous memory window at all times, eliminating wrap-around handling
 * in the MAC hot loop.
 *
 * ### Dual-buffer layout
 *
 * Internal buffer length = 2 × @e capacity, where @e capacity is the
 * smallest power of two ≥ @p num_taps.
 *
 * ```
 * buf:  [ lower half (0..cap-1) | upper half (cap..2cap-1) ]
 * ```
 *
 * Every push writes the new sample to @c buf[head] and to
 * @c buf[head+capacity], maintaining the invariant that
 * @c buf[head..head+num_taps-1] is always a valid, contiguous read
 * window — no modulo required in the reader.
 *
 * ### Interpolation usage
 *
 * ```c
 * #include <dp/delay.h>
 * #include <dp/accumulator.h>
 * #include <dp/nco.h>
 *
 * dp_delay_cf64_t *dl  = dp_delay_cf64_create(num_taps);
 * dp_acc_cf64_t   *acc = dp_acc_cf64_create();
 * dp_nco_t        *nco = dp_nco_create(fs_in / fs_out);
 *
 * for (size_t n = 0; n < num_in; n++) {
 *     // Load new input sample on NCO overflow (input rate)
 *     if (dp_nco_tick(nco))
 *         dp_delay_cf64_push(dl, input[n]);
 *
 *     // Select polyphase branch, MAC, emit output sample
 *     uint32_t phase  = dp_nco_phase(nco) >> (32 - 12);
 *     const float *h  = polyphase_bank[phase];
 *     dp_acc_cf64_madd(acc, dp_delay_cf64_ptr(dl), h, num_taps);
 *     output[out++]   = dp_acc_cf64_dump(acc);
 * }
 *
 * dp_delay_cf64_destroy(dl);
 * dp_acc_cf64_destroy(acc);
 * dp_nco_destroy(nco);
 * ```
 */

#ifndef DP_DELAY_H
#define DP_DELAY_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque cf64 delay line. */
  typedef struct dp_delay_cf64 dp_delay_cf64_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Allocate a cf64 delay line for @p num_taps samples.
   *
   * Internal capacity is rounded up to the next power of two so that
   * the head pointer can be advanced with a bitmask instead of a
   * modulo operation.  The dual buffer is initialised to all zeros.
   *
   * @param num_taps  Number of taps (filter length ≥ 1).
   * @return          Heap-allocated delay line, or NULL on failure.
   */
  dp_delay_cf64_t *dp_delay_cf64_create (size_t num_taps);

  /**
   * @brief Free a cf64 delay line.
   * @param dl  May be NULL (no-op).
   */
  void dp_delay_cf64_destroy (dp_delay_cf64_t *dl);

  /**
   * @brief Zero the sample history without freeing the delay line.
   *
   * Use after a stream discontinuity to prevent history contamination.
   *
   * @param dl  Must be non-NULL.
   */
  void dp_delay_cf64_reset (dp_delay_cf64_t *dl);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  /**
   * @brief Return the number of taps the delay line was created for.
   * @param dl  Must be non-NULL.
   */
  size_t dp_delay_cf64_num_taps (const dp_delay_cf64_t *dl);

  /**
   * @brief Return the internal capacity (power of two ≥ num_taps).
   * @param dl  Must be non-NULL.
   */
  size_t dp_delay_cf64_capacity (const dp_delay_cf64_t *dl);

  /* ------------------------------------------------------------------
   * Hot path
   * ------------------------------------------------------------------ */

  /**
   * @brief Push one cf64 sample into the delay line.
   *
   * Advances the internal head pointer by one (bitmask wrap) and writes
   * @p x to both halves of the dual buffer.  After the call,
   * dp_delay_cf64_ptr() returns a window with @p x at index 0.
   *
   * @param dl  Must be non-NULL.
   * @param x   New sample (most recent).
   */
  void dp_delay_cf64_push (dp_delay_cf64_t *dl, dp_cf64_t x);

  /**
   * @brief Return a pointer to the contiguous @p num_taps-sample window.
   *
   * @code
   * ptr[0]          — most recent sample
   * ptr[1]          — one sample ago
   * ...
   * ptr[num_taps-1] — oldest sample in the window
   * @endcode
   *
   * The pointer is valid until the next call to dp_delay_cf64_push().
   * Pass directly to dp_acc_cf64_madd() — no copy required.
   *
   * @param dl  Must be non-NULL.
   * @return    Pointer into the dual buffer; valid for @p num_taps reads.
   */
  const dp_cf64_t *dp_delay_cf64_ptr (const dp_delay_cf64_t *dl);

  /**
   * @brief Push a sample and return the updated read pointer in one call.
   *
   * Convenience wrapper for the interpolation hot loop:
   * @code
   * const dp_cf64_t *win = dp_delay_cf64_push_ptr(dl, x);
   * dp_acc_cf64_madd(acc, win, h, num_taps);
   * @endcode
   *
   * @param dl  Must be non-NULL.
   * @param x   New sample.
   * @return    Updated contiguous read pointer.
   */
  const dp_cf64_t *dp_delay_cf64_push_ptr (dp_delay_cf64_t *dl, dp_cf64_t x);

  /**
   * @brief Push @p n samples from an array into the delay line.
   *
   * Equivalent to calling dp_delay_cf64_push() @p n times with
   * @p in[0], @p in[1], ..., @p in[n-1] in order.  After the call,
   * dp_delay_cf64_ptr()[0] == @p in[n-1] (the last sample pushed).
   *
   * Useful for loading a block of input samples before running the
   * polyphase MAC:
   * @code
   * dp_delay_cf64_write(dl, block, block_len);
   * const dp_cf64_t *win = dp_delay_cf64_ptr(dl);
   * dp_acc_cf64_madd(acc, win, h, num_taps);
   * @endcode
   *
   * @param dl   Must be non-NULL.
   * @param in   Input array of @p n cf64 samples (oldest first).
   * @param n    Number of samples to push.
   */
  void dp_delay_cf64_write (dp_delay_cf64_t *dl, const dp_cf64_t *in,
                            size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_DELAY_H */
