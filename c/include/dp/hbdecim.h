/**
 * @file hbdecim.h
 * @brief Halfband 2:1 decimator for cf32 IQ samples.
 *
 * Exploits the halfband filter structure to halve the multiply count
 * compared to a general polyphase decimator:
 *
 * - **FIR branch** — symmetric FIR with N taps; pre-summing symmetric
 *   pairs reduces the inner loop to N/2 multiply-adds plus one multiply
 *   for the centre tap.
 *
 * - **Delay branch** — a single tap of unit gain at the centre position,
 *   contributing one multiply per output sample.
 *
 * Total cost per output: N/2 + 2 real multiply-adds (vs N for a
 * general 2-phase decimator using the same prototype filter).
 *
 * ### Coefficient source
 *
 * Design with `doppler.polyphase.kaiser_prototype(phases=2)`.  That
 * call returns a (2, N) polyphase bank; one row is the symmetric FIR
 * branch and the other is the pure delay.  Pass the FIR row as @p h
 * (the Python wrapper detects and selects it automatically).
 *
 * The delay-branch gain is always 1.0 for banks produced by
 * `kaiser_prototype`; this value is baked into the implementation.
 *
 * ### Usage
 *
 * ```c
 * #include <dp/hbdecim.h>
 *
 * // h[NUM_TAPS] = FIR branch from kaiser_prototype(phases=2)
 * dp_hbdecim_cf32_t *r = dp_hbdecim_cf32_create(NUM_TAPS, h);
 *
 * dp_cf32_t out[IN_LEN / 2 + 2];
 * size_t n_out = dp_hbdecim_cf32_execute(
 *     r, in, IN_LEN, out, sizeof(out)/sizeof(out[0]));
 *
 * dp_hbdecim_cf32_destroy(r);
 * ```
 *
 * ### Output buffer sizing
 *
 * Allocate at least `ceil(num_in / 2) + 1` samples.
 *
 * ### Streaming
 *
 * Odd-length blocks are handled transparently: the dangling even
 * sample is saved internally and consumed on the next call.
 */

#ifndef DP_HBDECIM_H
#define DP_HBDECIM_H

#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque halfband decimator state. */
  typedef struct dp_hbdecim_cf32 dp_hbdecim_cf32_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a halfband 2:1 decimator.
   *
   * @p h is the FIR branch of the polyphase bank produced by
   * `kaiser_prototype(phases=2)`.  The pure-delay branch is implicit
   * (unit gain at the centre position).  The array is copied
   * internally; the caller may free it immediately after this call.
   *
   * @p num_taps may be even or odd depending on the prototype filter
   * length.  Banks from @c kaiser_prototype(phases=2) may produce
   * either an 18-tap (even) or 19-tap (odd) FIR branch.
   *
   * @param num_taps  Length of the FIR branch (odd).
   * @param h         FIR branch coefficients, float32, length num_taps.
   * @return          Heap-allocated decimator, or NULL on failure.
   */
  dp_hbdecim_cf32_t *dp_hbdecim_cf32_create (size_t num_taps, const float *h);

  /**
   * @brief Free a halfband decimator.
   * @param r  May be NULL (no-op).
   */
  void dp_hbdecim_cf32_destroy (dp_hbdecim_cf32_t *r);

  /**
   * @brief Zero the sample history and clear any pending sample.
   *
   * Use after a stream discontinuity.
   *
   * @param r  Must be non-NULL.
   */
  void dp_hbdecim_cf32_reset (dp_hbdecim_cf32_t *r);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  /** @brief Return the decimation rate (always 0.5). */
  double dp_hbdecim_cf32_rate (const dp_hbdecim_cf32_t *r);

  /** @brief Return the FIR branch length. */
  size_t dp_hbdecim_cf32_num_taps (const dp_hbdecim_cf32_t *r);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  /**
   * @brief Decimate a block of cf32 IQ samples by 2.
   *
   * Processes @p num_in input samples and writes at most @p max_out
   * output samples.  Produces one output per two inputs; odd-length
   * blocks are buffered transparently.
   *
   * Internal state is preserved across calls — blocks may be any
   * size.
   *
   * @param r        Must be non-NULL.
   * @param in       Input sample array (may be NULL if num_in == 0).
   * @param num_in   Number of input samples.
   * @param out      Output buffer (must hold ≥ max_out samples).
   * @param max_out  Capacity of @p out in samples.
   * @return         Number of output samples written.
   */
  size_t dp_hbdecim_cf32_execute (dp_hbdecim_cf32_t *r, const dp_cf32_t *in,
                                  size_t num_in, dp_cf32_t *out,
                                  size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_HBDECIM_H */
