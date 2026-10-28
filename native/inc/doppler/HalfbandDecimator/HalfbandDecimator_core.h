/**
 * @file HalfbandDecimator_core.h
 * @brief Halfband 2:1 decimator for CF32 IQ (adapter over hbdecim_core).
 *
 * Thin adapter over hbdecim_state_t.  The caller supplies a FIR coefficient
 * array at construction; use resample.build_bank(2, ...) from Python to
 * generate a suitable halfband prototype.
 *
 * Lifecycle:
 * @code
 *   float h[] = { ... };  // num_taps FIR branch coefficients
 *   dp_HalfbandDecimator_state_t *r =
 *       dp_HalfbandDecimator_create(h, num_taps);
 *   float _Complex out[512];
 *   size_t n = dp_HalfbandDecimator_execute(r, in, 1024, out, 1024);
 *   dp_HalfbandDecimator_destroy(r);
 * @endcode
 */
#ifndef DP_HALFBANDDECIMATOR_CORE_H
#define DP_HALFBANDDECIMATOR_CORE_H

#include "doppler/hbdecim/hbdecim_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** HalfbandDecimator state: an alias of hbdecim_state_t, so every
   *  function here forwards to the dp_hbdecim_* leaf unchanged. */
  typedef hbdecim_state_t dp_HalfbandDecimator_state_t;

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Create a HalfbandDecimator with caller-supplied FIR taps.
   * Implements a 2:1 polyphase halfband decimator over CF32 IQ. The
   * caller provides the FIR branch coefficient array h; use
   * ``doppler.resample.kaiser_num_taps(2, atten, pb, sb)`` to size it
   * and scipy or the built-in bank helper to design the prototype.
   * Output length is approximately x_len / 2 per execute() call.
   *
   * @param h      Float32 FIR branch coefficients (the branch of the
   *               halfband prototype with more than one nonzero tap,
   *               not the whole prototype). Must be symmetric,
   *               `h[k] == h[h_len-1-k]`: only the first h_len/2 are read,
   *               and for odd h_len the centre tap is ignored in favor
   *               of the fixed 0.5 delay branch. Copied and scaled by
   *               0.5; must be non-NULL.
   * @param h_len  Number of coefficients in h (the FIR branch length
   *               reported by num_taps). Must be > 0.
   * @return A new state owned by the caller (free with
   *         dp_HalfbandDecimator_destroy()); NULL if h is NULL, h_len
   *         is 0, or allocation fails.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimator
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimator(h=h)
   * >>> hb.num_taps, hb.rate
   * (5, 0.5)
   * @endcode
   */
  dp_HalfbandDecimator_state_t *dp_HalfbandDecimator_create (const float *h,
                                                       size_t h_len);

  /**
   * @brief Free the coefficients, delay lines and the state.
   * @param state  State from dp_HalfbandDecimator_create(); NULL is a
   *               no-op.
   */
  void dp_HalfbandDecimator_destroy (dp_HalfbandDecimator_state_t *state);

  /**
   * @brief Zero all delay lines.  Coefficients and num_taps preserved.
   * Call between signal bursts to suppress transient ringing from prior
   * filter state. The next execute() after reset produces the same
   * output as a freshly created decimator fed the same input. Any
   * sample held for the next call is dropped too.
   *
   * @param state  State. Must be non-NULL.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimator
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimator(h=h)
   * >>> _ = hb.execute(np.ones(64, dtype=np.complex64))
   * >>> hb.reset()
   * >>> hb.num_taps
   * 5
   * @endcode
   */
  void dp_HalfbandDecimator_reset (dp_HalfbandDecimator_state_t *state);

  /**
   * @brief Serialized-state byte size (forwarded to the hbdecim leaf).
   * @param state  State. Must be non-NULL.
   * @return dp_hbdecim_state_bytes(state); depends on num_taps.
   */
  size_t dp_HalfbandDecimator_state_bytes (const dp_HalfbandDecimator_state_t *state);
  /**
   * @brief Serialize the decimator's delay-line state into @p blob.
   *
   * The blob is the hbdecim leaf's (HBDECIM_STATE_MAGIC envelope): both
   * delay rings, their heads and any held trailing sample.
   *
   * @param state  State. Must be non-NULL. Not modified.
   * @param[out] blob   Caller-owned buffer of at least
   *               dp_HalfbandDecimator_state_bytes(state) bytes.
   */
  void dp_HalfbandDecimator_get_state (const dp_HalfbandDecimator_state_t *state,
                                    void *blob);
  /**
   * @brief Restore state from @p blob; DP_OK, or DP_ERR_INVALID if rejected.
   * @param state  State built with the same num_taps as the source;
   *               unchanged on rejection.
   * @param blob   Blob from dp_HalfbandDecimator_get_state() (or
   *               dp_hbdecim_get_state()).
   * @return DP_OK, or DP_ERR_INVALID if the envelope (magic, version,
   *         endianness, size) rejects.
   */
  int dp_HalfbandDecimator_set_state (dp_HalfbandDecimator_state_t *state,
                                   const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief The most outputs one dp_HalfbandDecimator_execute() call over
   *        @p x_len inputs can write: `(x_len + 1) / 2`.
   *
   * A 2:1 decimator emits one output per input pair, and at most one sample
   * is held over from the previous call, so `(x_len + held) / 2` never
   * exceeds this. It is a true per-call bound, which is what lets the
   * binding accept an `out=` of `len(x) // 2 + 1` and allocate no more than
   * that when it sizes the output itself (doppler#2644; this used to return
   * a fixed 32768, which the binding then demanded of every `out=`).
   *
   * @param state  State; ignored.
   * @param x_len  Number of inputs the call will be given.
   * @return `(x_len + 1) / 2`; 0 for an empty call.
   */
  size_t dp_HalfbandDecimator_execute_max_out (dp_HalfbandDecimator_state_t *state,
                                            size_t x_len);

  /**
   * @brief Decimate x by 2 using the polyphase halfband FIR filter.
   * Each (even, odd) input pair yields one output: one phase runs
   * through the symmetric FIR branch and the other through a pure
   * delay weighted 0.5. State persists between calls: a sample left
   * unpaired at the end of a block (counting one held from the previous
   * call) is held for the next, so contiguous blocks give identical
   * output to one large block.
   *
   * @param state  Pointer to a valid dp_HalfbandDecimator_state_t.
   * @param x      CF32 input array.  A sample left unpaired at the end
   *               is held for the next call.
   * @param x_len  Number of input samples.
   * @param[out] out    Output buffer; must hold at least
   *               (x_len + 1) / 2 samples, which is
   *               dp_HalfbandDecimator_execute_max_out(). Python refuses a
   *               shorter `out=`. In C a shorter one drops nothing: the
   *               call stops at a pair boundary and dp_hbdecim_consumed()
   *               says how much input it took.
   * @param max_out Capacity of @p out in elements. Emission stops there, so
   *                the return value is the number actually written.
   * @return       Number of CF32 samples written to @p out:
   *               min((x_len + held) / 2, max_out), where held is 1 if
   *               a trailing sample was carried in from the previous
   *               call. 0 if x_len or max_out is 0.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimator
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimator(h=h)
   * >>> y = hb.execute(np.zeros(100, dtype=np.complex64))
   * >>> y.shape, y.dtype
   * ((50,), dtype('complex64'))
   * @endcode
   */
  size_t dp_HalfbandDecimator_execute (dp_HalfbandDecimator_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Fixed decimation rate — always 0.5.
   * The halfband decimator is structurally 2:1; this property exists
   * for API parity with Resampler and RateConverter.
   *
   * @param state  State; ignored.
   * @return Always 0.5 (output rate / input rate).
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimator
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> HalfbandDecimator(h=h).rate
   * 0.5
   * @endcode
   */
  double dp_HalfbandDecimator_get_rate (const dp_HalfbandDecimator_state_t *state);

  /**
   * @brief Number of FIR branch taps as passed to create.
   * The pure-delay branch has no taps; only the FIR branch has length
   * num_taps (it runs on the even-indexed inputs when num_taps is even,
   * the odd-indexed ones when it is odd). The total prototype length is
   * 2 * num_taps - 1.
   *
   * @param state  State. Must be non-NULL.
   * @return The h_len passed to dp_HalfbandDecimator_create().
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimator
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> HalfbandDecimator(h=h).num_taps
   * 5
   * @endcode
   */
  size_t
  dp_HalfbandDecimator_get_num_taps (const dp_HalfbandDecimator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* HALFBANDDECIMATOR_CORE_H */
