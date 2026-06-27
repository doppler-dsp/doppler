/**
 * @file hbdecim_core.h
 * @brief Halfband 2:1 decimator for CF32 IQ samples.
 *
 * Lifted from dp_hbdecim_cf32_t (c/src/hbdecim.c).  Only the cf32
 * variant is ported; dp_hbdecim_r2cf32 (real-input D2) is a separate
 * object and a separate session.
 *
 * Algorithm: two dual-write circular delay lines hold even-indexed and
 * odd-indexed input samples separately.  Per output sample:
 *   1. Push x(2m)   → even delay line.
 *   2. Push x(2m+1) → odd delay line.
 *   3. Compute symmetric FIR (N/2 paired multiplies) + scaled delay tap.
 *
 * Which delay line carries the FIR is determined by N:
 *   N even (fir_on_even=1): FIR on even_dl;  delay from odd_dl(centre).
 *   N odd  (fir_on_even=0): FIR on odd_dl at offset +1; delay from
 *                           even_dl(centre).
 *
 * Coefficients are scaled by 0.5 inside hbdecim_create — this is the
 * polyphase identity normalisation; do not remove it.
 *
 * Lifecycle:
 * @code
 *   hbdecim_state_t *r = hbdecim_create(num_taps, h_fir);
 *   size_t n = hbdecim_execute(r, in, num_in, out, max_out);
 *   hbdecim_destroy(r);
 * @endcode
 */
#ifndef HBDECIM_CORE_H
#define HBDECIM_CORE_H

#include "clib_common.h"
#include "dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    size_t num_taps; /* FIR branch length                         */
    size_t centre;   /* = num_taps / 2                            */
    int fir_on_even; /* 1 if N even, 0 if N odd                   */
    float *h;        /* FIR coeffs × 0.5 (polyphase normalisation) */

    /* Even delay line: stores x[2m], x[2m-2], ...
     * Dual-write ring: 2 × even_cap elements.                       */
    float _Complex *even_buf;
    size_t even_cap;
    size_t even_mask;
    size_t even_head;

    /* Odd delay line: stores x[2m+1], x[2m-1], ...
     * Shares even_cap and even_mask with even_buf.                  */
    float _Complex *odd_buf;
    size_t odd_head;

    int has_pending;        /* 1 when a trailing even sample is held */
    float _Complex pending; /* the held even sample                  */
  } hbdecim_state_t;

  /**
   * @brief Allocate and initialise a halfband 2:1 decimator.
   *
   * @param num_taps  Length of the FIR branch (the row from
   *                  kaiser_prototype(phases=2) that has more than one
   *                  significant coefficient).
   * @param h         FIR coefficients, length num_taps (float32).
   *                  Copied internally and scaled by 0.5.
   * @return Non-NULL on success, NULL on invalid args or OOM.
   */
  hbdecim_state_t *hbdecim_create (size_t num_taps, const float *h);

  /** Free all resources.  NULL is a no-op. */
  void hbdecim_destroy (hbdecim_state_t *r);

  /** Zero both delay lines and clear the pending-sample flag.
   *  num_taps and coefficients are preserved. */
  void hbdecim_reset (hbdecim_state_t *r);

  /* Serializable state (reusable elastic-resume convention): the even/odd
   * dual-write delay rings, their heads, and the pending even sample.  Coeffs
   * and sizes are config (rebuilt from num_taps on the resumed instance). */

  /* Standard bytes interface; see dp_state.h. */
#define HBDECIM_STATE_MAGIC DP_FOURCC ('H', 'B', 'D', 'C')
#define HBDECIM_STATE_VERSION 1u

  /** @brief Bytes hbdecim_get_state() writes for @p r (envelope + payload). */
  size_t hbdecim_state_bytes (const hbdecim_state_t *r);
  /** @brief Serialize @p r's mutable state into @p blob. */
  void hbdecim_get_state (const hbdecim_state_t *r, void *blob);
  /** @brief Restore mutable state from @p blob (same num_taps).
   *  @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects. */
  int hbdecim_set_state (hbdecim_state_t *r, const void *blob);

  /**
   * @brief Decimate a block of CF32 samples by 2.
   *
   * Processes input pairs (even, odd); one output per pair.  If num_in
   * is odd, the trailing even sample is buffered and consumed on the
   * next call.
   *
   * Output-buffer sizing: allocate at least (num_in + 1) / 2 samples.
   *
   * @param r        Must be non-NULL.
   * @param in       Input CF32 samples.
   * @param num_in   Number of input samples.
   * @param out      Output buffer.
   * @param max_out  Capacity of out in samples.
   * @return Number of output samples written.
   */
  size_t hbdecim_execute (hbdecim_state_t *r, const float _Complex *in,
                          size_t num_in, float _Complex *out, size_t max_out);

  /** Always returns 0.5 (rate is fixed by design). */
  double hbdecim_get_rate (const hbdecim_state_t *r);

  /** Returns the FIR branch length passed to hbdecim_create. */
  size_t hbdecim_get_num_taps (const hbdecim_state_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_CORE_H */
