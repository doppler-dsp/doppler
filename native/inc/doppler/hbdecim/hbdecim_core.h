/**
 * @file hbdecim_core.h
 * @brief Halfband 2:1 decimator for CF32 IQ samples.
 *
 * Lifted from dp_hbdecim_cf32_t (c/src/hbdecim.c).  Only the cf32
 * variant lives here; the real-input sibling (Architecture D2) is
 * hbdecim_r2c_core.h, and the jm-bound Python face is
 * HalfbandDecimator_core.h (doppler.resample.HalfbandDecimator).
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
 * Coefficients are scaled by 0.5 inside dp_hbdecim_create — this is the
 * polyphase identity normalisation; do not remove it.
 *
 * Lifecycle:
 * @code
 *   hbdecim_state_t *r = dp_hbdecim_create(num_taps, h_fir);
 *   size_t n = dp_hbdecim_execute(r, in, num_in, out, max_out);
 *   dp_hbdecim_destroy(r);
 * @endcode
 */
#ifndef HBDECIM_CORE_H
#define HBDECIM_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief State of a CF32 halfband 2:1 decimator.
   *
   * Allocate with dp_hbdecim_create() and free with dp_hbdecim_destroy().
   * The layout is public because HalfbandDecimator_core.h aliases it, but
   * callers should treat every field as read-only: the config fields are
   * set once by create, and the running fields are owned by execute,
   * reset and set_state.
   */
  typedef struct
  {
    size_t num_taps; /**< FIR branch length N, as passed to create
                          (config) */
    size_t centre;   /**< num_taps / 2 (integer division): the delay-line
                          index the half-weight delay branch reads
                          (config) */
    int fir_on_even; /**< 1 if N is even (FIR runs on the even-sample
                          line), 0 if N is odd (FIR on the odd line)
                          (config) */
    float *h;        /**< Owned copy of the N FIR coefficients, each
                          scaled by 0.5 (polyphase normalization); only
                          the first N/2 are read, the branch being
                          symmetric (config) */

    /* Even delay line: stores x[2m], x[2m-2], ...
     * Dual-write ring: 2 × even_cap elements.                       */
    float _Complex *even_buf; /**< Owned even-sample ring of 2*even_cap
                                   samples; each push writes index head
                                   and head+even_cap, so even_cap
                                   samples from head are always
                                   contiguous (running state) */
    size_t even_cap;  /**< Ring length: the smallest power of two >=
                           num_taps (config; shared by both rings) */
    size_t even_mask; /**< even_cap - 1, the wrap mask for both rings
                           (config) */
    size_t even_head; /**< Index of the newest sample in even_buf; moves
                           down by one per push (running state) */

    /* Odd delay line: stores x[2m+1], x[2m-1], ...
     * Shares even_cap and even_mask with even_buf.                  */
    float _Complex *odd_buf; /**< Owned odd-sample ring of 2*even_cap
                                  samples, dual-write like even_buf
                                  (running state) */
    size_t odd_head; /**< Index of the newest sample in odd_buf (running
                          state) */

    int has_pending;        /**< 1 when an unpaired sample is held for
                                 the next execute call, else 0 (running
                                 state) */
    float _Complex pending; /**< The held even sample; 0 when none is
                                 held (running state) */
    size_t consumed; /**< Input samples the last execute took; per-call,
                          not in the blob (0 after create, reset and
                          set_state) */
  } hbdecim_state_t;

  /**
   * @brief Allocate and initialise a halfband 2:1 decimator.
   *
   * The FIR branch must be symmetric (`h[k] == h[N-1-k]`): the filter
   * reads only the first N/2 coefficients and applies each to a mirrored
   * pair of delay-line samples.  For odd N the centre coefficient
   * `h[N/2]` is not read; the half-weight delay branch takes its place.
   *
   * @param num_taps  Length N of the FIR branch (the row from
   *                  kaiser_prototype(phases=2) that has more than one
   *                  significant coefficient).  Must be > 0.
   * @param h         FIR coefficients, length num_taps (float32).
   *                  Copied internally and scaled by 0.5, so the caller
   *                  keeps ownership.  Must be non-NULL.
   * @return A new state owned by the caller (free with
   *         dp_hbdecim_destroy()), with zeroed delay lines and no
   *         pending sample; NULL if num_taps is 0, h is NULL, or
   *         allocation fails.
   */
  hbdecim_state_t *dp_hbdecim_create (size_t num_taps, const float *h);

  /**
   * @brief Free the coefficient copy, both delay lines and the state.
   * @param r  State from dp_hbdecim_create(); NULL is a no-op.
   */
  void dp_hbdecim_destroy (hbdecim_state_t *r);

  /**
   * @brief Zero both delay lines and drop any pending sample.
   *
   * num_taps and coefficients are preserved; the next execute behaves
   * as on a freshly created decimator.
   *
   * @param r  State.  Must be non-NULL.
   */
  void dp_hbdecim_reset (hbdecim_state_t *r);

  /* Serializable state (reusable elastic-resume convention): the even/odd
   * dual-write delay rings, their heads, and the pending even sample.  Coeffs
   * and sizes are config (rebuilt from num_taps on the resumed instance). */

  /* Standard bytes interface; see dp_state.h. */
/** Envelope magic ("HBDC") that tags an hbdecim state blob. */
#define HBDECIM_STATE_MAGIC DP_FOURCC ('H', 'B', 'D', 'C')
/** Payload layout version of an hbdecim state blob. */
#define HBDECIM_STATE_VERSION 1u

  /**
   * @brief Bytes dp_hbdecim_get_state() writes for @p r (envelope + payload).
   *
   * The size depends on num_taps (through the ring length even_cap), so
   * a blob only fits an instance built with the same num_taps.
   *
   * @param r  State.  Must be non-NULL.
   * @return Blob size in bytes: header, both ring heads, the pending flag
   *         and sample, and both dual-write rings.
   */
  size_t dp_hbdecim_state_bytes (const hbdecim_state_t *r);
  /**
   * @brief Serialize @p r's mutable state into @p blob.
   * @param r     State.  Must be non-NULL.  Not modified.
   * @param[out] blob  Caller-owned buffer of at least
   *              dp_hbdecim_state_bytes(r) bytes.
   */
  void dp_hbdecim_get_state (const hbdecim_state_t *r, void *blob);
  /**
   * @brief Restore mutable state from @p blob (same num_taps).
   *
   * The envelope (magic, version, endianness, size) is validated before
   * anything is written; on rejection @p r is unchanged.  Coefficients
   * are not in the blob and are kept from create.
   *
   * @param r     State built with the same num_taps as the source.
   * @param blob  Blob from dp_hbdecim_get_state().
   * @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects.
   */
  int dp_hbdecim_set_state (hbdecim_state_t *r, const void *blob);

  /**
   * @brief Decimate a block of CF32 samples by 2.
   *
   * Processes input pairs (even, odd); one output per pair, counting a
   * sample held from the previous call as the first of the block.  A
   * sample left unpaired at the end is buffered and consumed on the
   * next call.
   *
   * Output-buffer sizing: allocate at least (num_in + 1) / 2 samples.
   * A shorter @p out drops nothing: the call stops at the pair boundary
   * after the last output it has room for, and dp_hbdecim_consumed()
   * says how much input it took, so the caller offers in + consumed
   * again and the outputs resume exactly as one whole call gives
   * (doppler#2643).  A trailing sample is held only when it is the last
   * input; with @p max_out 0 the call writes nothing and takes at most
   * that one sample.
   *
   * @param r        Must be non-NULL.
   * @param in       Input CF32 samples.
   * @param num_in   Number of input samples.
   * @param[out] out      Output buffer.
   * @param max_out  Capacity of out in samples.
   * @return Number of output samples written:
   *         min((num_in + has_pending) / 2, max_out).
   */
  size_t dp_hbdecim_execute (hbdecim_state_t *r, const float _Complex *in,
                          size_t num_in, float _Complex *out, size_t max_out);

  /**
   * @brief Input samples the last dp_hbdecim_execute() took.
   *
   * Equal to its @p num_in unless @p out filled first; then the caller
   * resumes at in + consumed.  0 after create, reset and set_state.
   *
   * @param r  Must be non-NULL.
   * @return Input samples taken by the last execute.
   */
  size_t dp_hbdecim_consumed (const hbdecim_state_t *r);

  /**
   * @brief Output-to-input sample-rate ratio (fixed by design).
   * @param r  State; ignored.
   * @return Always 0.5.
   */
  double dp_hbdecim_get_rate (const hbdecim_state_t *r);

  /**
   * @brief FIR branch length N.
   * @param r  State.  Must be non-NULL.
   * @return The num_taps passed to dp_hbdecim_create().
   */
  size_t dp_hbdecim_get_num_taps (const hbdecim_state_t *r);

  /**
   * @brief The filter's response to a constant input, from its own taps.
   *
   * A halfband decimator sums a symmetric FIR branch (each stored tap
   * multiplies TWO delay-line samples) and a half-weight delay branch, so at
   * DC the answer is `2*sum(h) + 0.5`. Computed, not measured: a caller — or
   * a gate — can ask what gain the object contributes without running a
   * signal through it.
   *
   * @param r State. Must be non-NULL.
   * @return The DC gain. 1.0 for a correctly normalised halfband.
   *
   * @code
   * hbdecim_state_t *d = dp_hbdecim_create (ntaps, h);
   * printf ("%.4f\n", dp_hbdecim_dc_gain (d));  // 1.0000
   * dp_hbdecim_destroy (d);
   * @endcode
   */
  double dp_hbdecim_dc_gain (const hbdecim_state_t *r);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_CORE_H */
