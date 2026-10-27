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
 * That form is for an even FIR branch length N; for odd N the roles
 * swap (I comes from the delay branch, Q from the FIR on the odd
 * stream).  Both rails are then scaled by 2 so a real tone of
 * amplitude A comes out as a complex tone of amplitude A (the
 * analytic-signal convention), matching the unity DC gain of the
 * complex sibling (dp_hbdecim_dc_gain()).
 *
 * Lifecycle:
 * @code
 *   hbdecim_r2c_state_t *r = dp_hbdecim_r2c_create(num_taps, h);
 *   size_t n = dp_hbdecim_r2c_execute(r, in, num_in, out, max_out);
 *   dp_hbdecim_r2c_destroy(r);
 * @endcode
 */

#ifndef HBDECIM_R2C_CORE_H
#define HBDECIM_R2C_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Opaque real-to-complex halfband decimator state.
   *
   * Allocate with dp_hbdecim_r2c_create(); free with
   * dp_hbdecim_r2c_destroy().  Holds the scaled, sign-modulated FIR
   * coefficients, two dual-write delay rings (even and odd input
   * samples), a held trailing sample and the output parity.
   */
  typedef struct hbdecim_r2c_state hbdecim_r2c_state_t;

  /**
   * @brief Allocate a real-to-complex halfband decimator.
   *
   * The coefficients are copied and stored as `h[k] * (-1)^k * 0.5` (the
   * fs/4 modulation baked into the taps).  As in hbdecim_core.h, only
   * the first num_taps/2 are read, so the branch must be symmetric.
   *
   * @param num_taps  FIR branch length N.  Must be > 0.
   * @param h         FIR coefficients, float32, length num_taps.  Copied;
   *                  the caller keeps ownership.  Must be non-NULL.
   * @return A new state owned by the caller (free with
   *         dp_hbdecim_r2c_destroy()), with zeroed history and parity 0;
   *         NULL if num_taps is 0, h is NULL, or allocation fails.
   */
  hbdecim_r2c_state_t *dp_hbdecim_r2c_create (size_t num_taps, const float *h);

  /**
   * @brief Free the coefficients, both delay lines and the state.
   * @param r  State from dp_hbdecim_r2c_create(); NULL is a no-op.
   */
  void dp_hbdecim_r2c_destroy (hbdecim_r2c_state_t *r);

  /**
   * @brief Zero history and output parity without freeing.
   *
   * Clears both delay lines and their heads, drops any held trailing
   * sample and sets the (-1)^m output parity back to +1.
   *
   * @param r  State.  Must be non-NULL.
   */
  void dp_hbdecim_r2c_reset (hbdecim_r2c_state_t *r);

  /**
   * @brief Output-to-input sample-rate ratio (fixed by design).
   * @param r  State; ignored.
   * @return Always 0.5.
   */
  double dp_hbdecim_r2c_get_rate (const hbdecim_r2c_state_t *r);

  /**
   * @brief FIR branch length N.
   * @param r  State.  Must be non-NULL.
   * @return The num_taps passed to dp_hbdecim_r2c_create().
   */
  size_t dp_hbdecim_r2c_get_num_taps (const hbdecim_r2c_state_t *r);

  /**
   * @brief Decimate real float32 input by 2, producing CF32.
   *
   * One complex output per (even, odd) input pair, counting a sample
   * held from the previous call as the first of the block.  A sample
   * left unpaired at the end is held for the next call, so contiguous
   * blocks give the same output as one large block.  Size @p out for
   * the whole block, (num_in + 1) / 2 samples.  A shorter @p out drops
   * nothing: the call stops at the pair boundary after the last output it
   * has room for, and dp_hbdecim_r2c_consumed() says how much input it
   * took, so the caller offers in + consumed again and the outputs resume
   * exactly as one whole call gives (doppler#2643).  A trailing sample is
   * held only when it is the last input; with @p max_out 0 the call
   * writes nothing and takes at most that one sample.
   *
   * @param r        Must be non-NULL.
   * @param in       Real input samples.
   * @param num_in   Number of input samples.
   * @param[out] out      CF32 output buffer.
   * @param max_out  Capacity of @p out in samples.
   * @return Number of output samples written:
   *         min((num_in + held) / 2, max_out), where held is 1 when a
   *         trailing sample was carried in from the previous call.
   */
  size_t dp_hbdecim_r2c_execute (hbdecim_r2c_state_t *r, const float *in,
                              size_t num_in, float _Complex *out,
                              size_t max_out);

  /**
   * @brief Input samples the last dp_hbdecim_r2c_execute() took.
   *
   * Equal to its @p num_in unless @p out filled first; then the caller
   * resumes at in + consumed.  0 after create, reset and set_state.
   *
   * @param r  Must be non-NULL.
   * @return Input samples taken by the last execute.
   */
  size_t dp_hbdecim_r2c_consumed (const hbdecim_r2c_state_t *r);

  /* ── Serializable state (reusable elastic-resume convention) ──────────────
   * Mutable per-stream state only — the even/odd delay rings, their write
   * heads, the pending even sample, and the output parity.  Coefficients and
   * sizes are config (rebuilt from num_taps on the resumed instance).  Size is
   * derived from even_cap, so a same-num_taps instance round-trips exactly. */

  /* Standard bytes interface; see dp_state.h. */
/** Envelope magic ("HBR2") that tags an hbdecim_r2c state blob. */
#define HBDECIM_R2C_STATE_MAGIC DP_FOURCC ('H', 'B', 'R', '2')
/** Payload layout version of an hbdecim_r2c state blob. */
#define HBDECIM_R2C_STATE_VERSION 1u

  /**
   * @brief Bytes dp_hbdecim_r2c_get_state() writes (envelope + payload).
   * @param r  State.  Must be non-NULL.
   * @return Blob size in bytes; depends on num_taps through the ring
   *         length, so a blob fits only a same-num_taps instance.
   */
  size_t dp_hbdecim_r2c_state_bytes (const hbdecim_r2c_state_t *r);
  /**
   * @brief Serialize @p r's mutable state into @p blob.
   * @param r     State.  Must be non-NULL.  Not modified.
   * @param[out] blob  Caller-owned buffer of at least
   *              dp_hbdecim_r2c_state_bytes(r) bytes.
   */
  void dp_hbdecim_r2c_get_state (const hbdecim_r2c_state_t *r, void *blob);
  /**
   * @brief Restore mutable state from @p blob (same num_taps).
   *
   * The envelope (magic, version, endianness, size) is validated before
   * anything is written; on rejection @p r is unchanged.
   *
   * @param r     State built with the same num_taps as the source.
   * @param blob  Blob from dp_hbdecim_r2c_get_state().
   * @return DP_OK, or DP_ERR_INVALID if the blob's envelope rejects.
   */
  int dp_hbdecim_r2c_set_state (hbdecim_r2c_state_t *r, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* HBDECIM_R2C_CORE_H */
