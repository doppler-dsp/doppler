/**
 * @file ddc.h
 * @brief Digital Down-Converter (DDC).
 *
 * Chains an NCO (frequency translation to DC) with an optional DPMFS
 * polyphase resampler (decimation/interpolation), forming the front end
 * of a spectrum analyser or receiver.
 *
 * ### Signal chain
 *
 * ```
 * CF32 in  (fs_in)
 *   → NCO mix   multiply by e^{j·2π·f_n·t}  (shift by f_n·fs_in Hz)
 *   → [DPMFS resample]   optional; rate = fs_out / fs_in
 * CF32 out (fs_out)
 * ```
 *
 * To translate a signal at offset Δf from DC to baseband, set the NCO
 * normalised frequency to −Δf / fs_in.
 *
 * ### Ownership
 *
 * `dp_ddc_create` takes **ownership** of the resampler @p r.  The caller
 * must not use or destroy @p r after passing it to `dp_ddc_create`.
 * `dp_ddc_destroy` frees both the DDC state and the owned resampler.
 *
 * When @p r is NULL the resampler stage is bypassed; output equals the
 * mixed signal at the input sample rate.
 *
 * ### Retuning vs. rebuilding
 *
 * - **Retune** (centre-frequency change): call `dp_ddc_set_freq`.
 *   Cheap — updates the NCO phase increment without touching the
 *   resampler history.
 * - **Zoom** (span / decimation-rate change): destroy and recreate the
 *   DDC with a new resampler designed for the new rate.
 *
 * ### Usage
 *
 * ```c
 * #include <dp/ddc.h>
 *
 * // 8× decimating DDC; coefficients from doppler.polyphase.fit_dpmfs
 * dp_resamp_dpmfs_t *r = dp_resamp_dpmfs_create(3, 19, c0, c1, 0.125);
 *
 * // Translate a signal at +0.1·fs to DC, then decimate
 * dp_ddc_t *ddc = dp_ddc_create(-0.1f, r);  // takes ownership of r
 *
 * dp_cf32_t out[512];
 * size_t n = dp_ddc_execute(ddc, in, 4096, out, 512);
 *
 * dp_ddc_destroy(ddc);  // also destroys r
 * ```
 */

#ifndef DP_DDC_H
#define DP_DDC_H

#include <dp/resamp_dpmfs.h>
#include <dp/stream.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque DDC state. */
  typedef struct dp_ddc dp_ddc_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a DDC.
   *
   * @param norm_freq  NCO normalised frequency f/fs (cycles per sample).
   *                   Negative values shift a positive-offset signal to
   *                   DC.  Range [−0.5, 0.5); values outside are folded
   *                   by unsigned 32-bit wrap-around (same as the NCO).
   * @param r          DPMFS resampler, or NULL to bypass decimation.
   *                   The DDC takes ownership; the caller must not touch
   *                   @p r after this call.
   * @return           Heap-allocated DDC state, or NULL on failure.
   *                   On failure, @p r is destroyed so the caller does
   *                   not leak it.
   */
  dp_ddc_t *dp_ddc_create (float norm_freq, dp_resamp_dpmfs_t *r);

  /**
   * @brief Destroy the DDC and release all resources.
   *
   * Also destroys the resampler that was passed to dp_ddc_create.
   *
   * @param ddc  May be NULL (no-op).
   */
  void dp_ddc_destroy (dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Control
   * ------------------------------------------------------------------ */

  /**
   * @brief Change the NCO tune frequency without resetting phase.
   *
   * Takes effect on the next dp_ddc_execute call.  Does not disturb
   * the resampler history, so retunes are seamless across block
   * boundaries.
   *
   * @param ddc        Must be non-NULL.
   * @param norm_freq  New normalised frequency (same convention as
   *                   dp_ddc_create).
   */
  void dp_ddc_set_freq (dp_ddc_t *ddc, float norm_freq);

  /**
   * @brief Return the current NCO normalised frequency.
   * @param ddc  Must be non-NULL.
   */
  float dp_ddc_get_freq (const dp_ddc_t *ddc);

  /**
   * @brief Reset NCO phase and resampler history to zero.
   *
   * Use after a stream discontinuity to prevent stale state from
   * contaminating the next block.
   *
   * @param ddc  Must be non-NULL.
   */
  void dp_ddc_reset (dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Processing
   * ------------------------------------------------------------------ */

  /**
   * @brief Mix and resample a block of CF32 IQ samples.
   *
   * Each input sample is multiplied by the NCO phasor, then the
   * mixed block is fed through the resampler (if configured) into
   * @p out.  NCO phase and resampler history are preserved across
   * calls.
   *
   * @param ddc      Must be non-NULL.
   * @param in       Input samples, CF32, length @p num_in.
   * @param num_in   Number of input samples.
   * @param out      Output buffer, CF32, capacity @p max_out samples.
   * @param max_out  Maximum output samples to write.
   * @return         Number of output samples written (0 if
   *                 @p num_in == 0 or memory allocation fails).
   */
  size_t dp_ddc_execute (dp_ddc_t *ddc, const dp_cf32_t *in, size_t num_in,
                         dp_cf32_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_DDC_H */
