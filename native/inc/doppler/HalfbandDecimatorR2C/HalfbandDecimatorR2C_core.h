/**
 * @file HalfbandDecimatorR2C_core.h
 * @brief Real-input halfband 2:1 decimator with an fs/4 shift: F32 in, CF32
 *        out (adapter over hbdecim_r2c_core).
 *
 * Thin adapter over hbdecim_r2c_state_t, the real-input sibling of
 * HalfbandDecimator: one step takes a real ADC stream at fs to complex
 * baseband at fs/2, with the band centred at fs/4 landing at DC
 * (Architecture D2, docs/design/ddc.md). The fs/4 mix is folded into the
 * polyphase arithmetic, so it costs no multiplies.
 *
 * Lifecycle:
 * @code
 *   static const float h[] = { 0.25f, 0.5f, 0.25f };  // the FIR branch
 *   static float in[1024];                            // real ADC samples
 *   static float _Complex out[512];
 *   dp_HalfbandDecimatorR2C_state_t *r = dp_HalfbandDecimatorR2C_create (h, 3);
 *   size_t n = dp_HalfbandDecimatorR2C_execute (r, in, 1024, out, 512);
 *   dp_HalfbandDecimatorR2C_destroy (r);
 *   return n == 512 ? 0 : 1;
 * @endcode
 */
#ifndef DP_HALFBANDDECIMATORR2C_CORE_H
#define DP_HALFBANDDECIMATORR2C_CORE_H

#include "doppler/hbdecim/hbdecim_r2c_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /** HalfbandDecimatorR2C state: an alias of hbdecim_r2c_state_t, so every
   *  function here forwards to the dp_hbdecim_r2c_* leaf unchanged. */
  typedef hbdecim_r2c_state_t dp_HalfbandDecimatorR2C_state_t;

  /* ------------------------------------------------------------------ */
  /* Lifecycle                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Create a real-input halfband decimator with caller-supplied FIR
   *        taps.
   * Implements a 2:1 polyphase halfband decimator from real F32 at fs to
   * CF32 at fs/2, with an embedded fs/4 shift: a tone at f_in leaves at
   * f_in - fs/4 (input-normalised 0.25 lands at DC). The FIR branch is the
   * same one HalfbandDecimator takes; size it with
   * ``doppler.resample.kaiser_num_taps(2, atten, pb, sb)``.
   *
   * @param h      Float32 FIR branch coefficients (the branch of the
   *               halfband prototype with more than one nonzero tap, not
   *               the whole prototype). Copied; must be non-NULL.
   * @param h_len  Number of coefficients in h (the FIR branch length
   *               reported by num_taps). Must be > 0.
   * @return A new state owned by the caller (free with
   *         dp_HalfbandDecimatorR2C_destroy()); NULL if h is NULL, h_len
   *         is 0, or allocation fails.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimatorR2C
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimatorR2C(h=h)
   * >>> hb.num_taps, hb.rate
   * (5, 0.5)
   * @endcode
   */
  dp_HalfbandDecimatorR2C_state_t *
  dp_HalfbandDecimatorR2C_create (const float *h, size_t h_len);

  /**
   * @brief Free the coefficients, delay lines and the state.
   * @param state  State from dp_HalfbandDecimatorR2C_create(); NULL is a
   *               no-op.
   */
  void dp_HalfbandDecimatorR2C_destroy (dp_HalfbandDecimatorR2C_state_t *state);

  /**
   * @brief Zero the delay lines and the fs/4 phase.  Coefficients and
   *        num_taps preserved.
   * The next execute() after reset produces the same output as a freshly
   * created decimator fed the same input. Any sample held for the next
   * call is dropped too.
   *
   * @param state  State. Must be non-NULL.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimatorR2C
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimatorR2C(h=h)
   * >>> x = np.ones(64, dtype=np.float32)
   * >>> a = hb.execute(x)
   * >>> hb.reset()
   * >>> bool(np.array_equal(hb.execute(x), a))
   * True
   * @endcode
   */
  void dp_HalfbandDecimatorR2C_reset (dp_HalfbandDecimatorR2C_state_t *state);

  /**
   * @brief Serialized-state byte size (forwarded to the hbdecim_r2c leaf).
   * @param state  State. Must be non-NULL.
   * @return dp_hbdecim_r2c_state_bytes(state); depends on num_taps.
   */
  size_t
  dp_HalfbandDecimatorR2C_state_bytes (const dp_HalfbandDecimatorR2C_state_t *state);

  /**
   * @brief Serialize the decimator's running state into @p blob.
   *
   * The blob is the hbdecim_r2c leaf's envelope: the delay lines, the
   * fs/4 phase and any held trailing sample.
   *
   * @param state  State. Must be non-NULL. Not modified.
   * @param[out] blob   Caller-owned buffer of at least
   *               dp_HalfbandDecimatorR2C_state_bytes(state) bytes.
   */
  void
  dp_HalfbandDecimatorR2C_get_state (const dp_HalfbandDecimatorR2C_state_t *state,
                                     void *blob);

  /**
   * @brief Restore state from @p blob; DP_OK, or DP_ERR_INVALID if rejected.
   * @param state  State built with the same num_taps as the source;
   *               unchanged on rejection.
   * @param blob   Blob from dp_HalfbandDecimatorR2C_get_state() (or
   *               dp_hbdecim_r2c_get_state()).
   * @return DP_OK, or DP_ERR_INVALID if the envelope (magic, version,
   *         endianness, size) rejects.
   */
  int dp_HalfbandDecimatorR2C_set_state (dp_HalfbandDecimatorR2C_state_t *state,
                                         const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /**
   * @brief The most outputs one dp_HalfbandDecimatorR2C_execute() call over
   *        @p x_len inputs can write: `(x_len + 1) / 2`.
   *
   * One output per input pair, and at most one sample held over from the
   * previous call, so `(x_len + held) / 2` never exceeds this.
   *
   * @param state  State; ignored.
   * @param x_len  Number of inputs the call will be given.
   * @return `(x_len + 1) / 2`; 0 for an empty call.
   */
  size_t
  dp_HalfbandDecimatorR2C_execute_max_out (dp_HalfbandDecimatorR2C_state_t *state,
                                           size_t x_len);

  /**
   * @brief Decimate real x by 2 to CF32, shifting fs/4 to DC.
   * Each (even, odd) input pair yields one complex output. State persists
   * between calls: a sample left unpaired at the end of a block (counting
   * one held from the previous call) is held for the next, so contiguous
   * blocks give identical output to one large block.
   *
   * @param state  Pointer to a valid dp_HalfbandDecimatorR2C_state_t.
   * @param x      Real F32 input array.
   * @param x_len  Number of input samples.
   * @param[out] out    CF32 output buffer; must hold at least
   *               (x_len + 1) / 2 samples, which is
   *               dp_HalfbandDecimatorR2C_execute_max_out(). Python refuses
   *               a shorter `out=`. In C a shorter one drops nothing: the
   *               call stops at a pair boundary and
   *               dp_hbdecim_r2c_consumed() says how much input it took.
   * @param max_out Capacity of @p out in elements.
   * @return       Number of CF32 samples written to @p out:
   *               min((x_len + held) / 2, max_out). 0 if x_len or max_out
   *               is 0.
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimatorR2C
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> hb = HalfbandDecimatorR2C(h=h)
   * >>> y = hb.execute(np.zeros(100, dtype=np.float32))
   * >>> y.shape, y.dtype
   * ((50,), dtype('complex64'))
   * @endcode
   */
  size_t dp_HalfbandDecimatorR2C_execute (dp_HalfbandDecimatorR2C_state_t *state,
                                          const float *x, size_t x_len,
                                          float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Fixed decimation rate — always 0.5.
   *
   * @param state  State; ignored.
   * @return Always 0.5 (output rate / input rate).
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimatorR2C
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> HalfbandDecimatorR2C(h=h).rate
   * 0.5
   * @endcode
   */
  double
  dp_HalfbandDecimatorR2C_get_rate (const dp_HalfbandDecimatorR2C_state_t *state);

  /**
   * @brief Number of FIR branch taps as passed to create.
   *
   * @param state  State. Must be non-NULL.
   * @return The h_len passed to dp_HalfbandDecimatorR2C_create().
   *
   * @code
   * >>> from doppler.resample import HalfbandDecimatorR2C
   * >>> import numpy as np
   * >>> h = np.array([0.0625, 0.25, 0.375, 0.25, 0.0625],
   * ...              dtype=np.float32)
   * >>> HalfbandDecimatorR2C(h=h).num_taps
   * 5
   * @endcode
   */
  size_t
  dp_HalfbandDecimatorR2C_get_num_taps (const dp_HalfbandDecimatorR2C_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* DP_HALFBANDDECIMATORR2C_CORE_H */
