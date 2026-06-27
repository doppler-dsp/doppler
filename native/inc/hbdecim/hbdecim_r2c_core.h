/**
 * @file hbdecim_r2c_core.h
 * @brief Real-to-complex halfband 2:1 decimator (Architecture D2).
 *
 * Lifted from dp_hbdecim_r2cf32_t (c/src/hbdecim.c).  Accepts real
 * float32 input and produces CF32 IQ at half the input rate, with an
 * embedded fs/4 frequency shift (same effect as mixing with e^{jπn/2}
 * before decimating, at zero extra multiplications).
 *
 * The output at sample m is:
 *   y(m) = (FIR(even) + j·delay(odd)) · (-1)^m
 *
 * where the sign pattern (-1)^m provides the fs/4 shift correction.
 *
 * Lifecycle:
 * @code
 *   hbdecim_r2c_state_t *r = hbdecim_r2c_create(num_taps, h);
 *   size_t n = hbdecim_r2c_execute(r, in, num_in, out, max_out);
 *   hbdecim_r2c_destroy(r);
 * @endcode
 */

#ifndef HBDECIM_R2C_CORE_H
#define HBDECIM_R2C_CORE_H

#include "clib_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct hbdecim_r2c_state hbdecim_r2c_state_t;

  /**
   * @brief Allocate a real-to-complex halfband decimator.
   *
   * @param num_taps  FIR branch length.
   * @param h         FIR coefficients, float32, length num_taps.
   * @return Non-NULL on success, NULL on invalid args or OOM.
   */
  hbdecim_r2c_state_t *hbdecim_r2c_create (size_t num_taps, const float *h);

  /** Free all resources.  NULL is a no-op. */
  void hbdecim_r2c_destroy (hbdecim_r2c_state_t *r);

  /** Zero history and output parity without freeing. */
  void hbdecim_r2c_reset (hbdecim_r2c_state_t *r);

  /** Always returns 0.5. */
  double hbdecim_r2c_get_rate (const hbdecim_r2c_state_t *r);

  /** Returns the FIR branch length passed to hbdecim_r2c_create. */
  size_t hbdecim_r2c_get_num_taps (const hbdecim_r2c_state_t *r);

  /**
   * @brief Decimate real float32 input by 2, producing CF32.
   *
   * @param r        Must be non-NULL.
   * @param in       Real input samples.
   * @param num_in   Number of input samples.
   * @param out      CF32 output buffer.
   * @param max_out  Capacity in samples.
   * @return Number of output samples written.
   */
  size_t hbdecim_r2c_execute (hbdecim_r2c_state_t *r, const float *in,
                              size_t num_in, float _Complex *out,
                              size_t max_out);

  /* ── Serializable state (reusable elastic-resume convention) ──────────────
   * Mutable per-stream state only — the even/odd delay rings, their write
   * heads, the pending odd sample, and the output parity.  Coefficients and
   * sizes are config (rebuilt from num_taps on the resumed instance).  Size is
   * derived from even_cap, so a same-num_taps instance round-trips exactly. */

  /** @brief Bytes hbdecim_r2c_get_state() writes for @p r. */
  size_t hbdecim_r2c_state_bytes (const hbdecim_r2c_state_t *r);
  /** @brief Serialize @p r's mutable state into @p blob. */
  void hbdecim_r2c_get_state (const hbdecim_r2c_state_t *r, void *blob);
  /** @brief Restore mutable state from @p blob (same num_taps).  @return 0. */
  int hbdecim_r2c_set_state (hbdecim_r2c_state_t *r, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_R2C_CORE_H */
