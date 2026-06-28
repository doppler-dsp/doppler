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
 *   HalfbandDecimator_state_t *r =
 *       HalfbandDecimator_create(num_taps, h);
 *   float complex out[512];
 *   size_t n = HalfbandDecimator_execute(r, in, 1024, out);
 *   HalfbandDecimator_destroy(r);
 * @endcode
 */
#ifndef HALFBANDDECIMATOR_CORE_H
#define HALFBANDDECIMATOR_CORE_H

#include "hbdecim/hbdecim_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef hbdecim_state_t HalfbandDecimator_state_t;

/* Maximum output samples per call (pre-allocated by ext.c at init). */
#define HBDECIM_MAX_OUT 32768

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
   * @param num_taps  Number of FIR branch coefficients in h.
   * @param h         Float32 FIR branch coefficients, length num_taps.
   *                  Must be a symmetric halfband prototype (antisymmetric
   *                  even-indexed taps zeroed).
   * @return Non-NULL on success, NULL on invalid args or OOM.
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
  HalfbandDecimator_state_t *HalfbandDecimator_create (size_t num_taps,
                                                       const float *h);

  /** Free all resources.  NULL is a no-op. */
  void HalfbandDecimator_destroy (HalfbandDecimator_state_t *state);

  /**
   * @brief Zero all delay lines.  Coefficients and num_taps preserved.
   * Call between signal bursts to suppress transient ringing from prior
   * filter state. The next execute() after reset produces the same
   * output as a freshly created decimator fed the same input.
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
  void HalfbandDecimator_reset (HalfbandDecimator_state_t *state);

  /** @brief Serialized-state byte size (forwarded to the hbdecim leaf). */
  size_t HalfbandDecimator_state_bytes (const HalfbandDecimator_state_t *state);
  /** @brief Serialize the decimator's delay-line state into @p blob. */
  void HalfbandDecimator_get_state (const HalfbandDecimator_state_t *state,
                                    void *blob);
  /** @brief Restore state from @p blob; DP_OK, or DP_ERR_INVALID if rejected. */
  int HalfbandDecimator_set_state (HalfbandDecimator_state_t *state,
                                   const void *blob);

  /* ------------------------------------------------------------------ */
  /* Execute                                                             */
  /* ------------------------------------------------------------------ */

  /** Always returns HBDECIM_MAX_OUT. */
  size_t HalfbandDecimator_execute_max_out (HalfbandDecimator_state_t *state);

  /**
   * @brief Decimate x by 2 using the polyphase halfband FIR filter.
   * Processes every second input sample through the FIR branch and
   * passes the other branch through the all-pass (zero-delay) path.
   * State persists between calls — contiguous blocks give identical
   * output to one large block. Output length is floor(x_len / 2).
   *
   * @param state  Pointer to a valid HalfbandDecimator_state_t.
   * @param x      CF32 input array.  Length must be even for exact
   *               half-rate output; odd lengths write floor(x_len/2).
   * @param x_len  Number of input samples.
   * @param out    Output buffer; must hold at least floor(x_len/2) samples.
   * @return       CF32 decimated output; length == floor(x_len / 2).
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
  size_t HalfbandDecimator_execute (HalfbandDecimator_state_t *state,
                                    const float complex *x, size_t x_len,
                                    float complex *out);

  /* ------------------------------------------------------------------ */
  /* Properties                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Fixed decimation rate — always 0.5.
   * The halfband decimator is structurally 2:1; this property exists
   * for API parity with Resampler and RateConverter.
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
  double HalfbandDecimator_get_rate (const HalfbandDecimator_state_t *state);

  /**
   * @brief Number of FIR branch taps as passed to create.
   * The all-pass (even-phase) branch has no taps; only the odd-phase
   * FIR branch has length num_taps. The total prototype length is
   * 2 * num_taps - 1.
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
  HalfbandDecimator_get_num_taps (const HalfbandDecimator_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* HALFBANDDECIMATOR_CORE_H */
