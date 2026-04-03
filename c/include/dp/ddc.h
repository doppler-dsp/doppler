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
 * ### Fixed block size
 *
 * The DDC is initialised for a fixed input block size @p num_in and
 * pre-allocates all internal buffers at creation time — no heap
 * allocation occurs during processing.  Call `dp_ddc_max_out()` to
 * learn how large the output buffer must be per `dp_ddc_execute` call.
 *
 * ### Default vs. custom filter coefficients
 *
 * `dp_ddc_create` uses a built-in M=3, N=19 Kaiser–DPMFS filter
 * (passband ≤ 0.4·Nyquist, stopband ≥ 0.6·Nyquist, 60 dB rejection)
 * that works well for decimation rates from 2× to 100× and matches the
 * 80%-bandwidth convention used by the doppler spectrum analyser.  No
 * design step required.
 *
 * When you need a specific filter — tighter cutoff, higher order, or a
 * rate-matched design from `doppler.polyphase.optimize_dpmfs` — use
 * `dp_ddc_create_custom` and pass a pre-built `dp_resamp_dpmfs_t`.
 *
 * ### Retuning vs. rebuilding
 *
 * - **Retune** (centre-frequency change): call `dp_ddc_set_freq`.
 *   Cheap — updates the NCO phase increment without touching the
 *   resampler history.
 * - **Zoom** (span / decimation-rate change): destroy and recreate the
 *   DDC for the new rate.
 *
 * ### Usage — default coefficients
 *
 * ```c
 * #include <dp/ddc.h>
 *
 * // 4× decimating DDC; shift a signal at +0.1·fs to DC
 * dp_ddc_t *ddc = dp_ddc_create(-0.1f, 4096, 0.25);
 *
 * dp_cf32_t out[dp_ddc_max_out(ddc)]; // or: malloc(dp_ddc_max_out(ddc))
 * size_t n = dp_ddc_execute(ddc, in, 4096, out, dp_ddc_max_out(ddc));
 *
 * dp_ddc_destroy(ddc);
 * ```
 *
 * ### Usage — custom coefficients
 *
 * ```c
 * #include <dp/ddc.h>
 *
 * // Coefficients designed with doppler.polyphase.optimize_dpmfs
 * dp_resamp_dpmfs_t *r = dp_resamp_dpmfs_create(M, N, c0, c1, rate);
 * dp_ddc_t *ddc = dp_ddc_create_custom(-0.1f, 4096, r);
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
   * Lifecycle — default coefficients
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a DDC using built-in filter coefficients.
   *
   * Uses a built-in M=3, N=19 Kaiser–DPMFS lowpass filter with
   * frequency response normalised to the **output** sample rate:
   *
   *   passband  ≤ 0.4 × (fs_out / 2)    flat response
   *   stopband  ≥ 0.6 × (fs_out / 2)    ≥ 60 dB rejection
   *
   * Because the cutoffs track fs_out, the same coefficient bank works
   * for any decimation rate.  In input-rate units:
   *
   *   passband edge  = 0.4 × rate/2 × fs_in
   *   stopband edge  = 0.6 × rate/2 × fs_in
   *
   * This matches the doppler spectrum analyser's 80 % bandwidth
   * convention (fs_out = span / 0.8).
   *
   * Passing @p rate = 1.0 (or any value within 1×10⁻⁶ of 1.0) bypasses
   * the resampler; output equals the mixed signal at the input rate.
   *
   * @param norm_freq  NCO normalised frequency f/fs (cycles per sample).
   *                   Negative values shift a positive-offset signal to
   *                   DC.  Values outside [−0.5, 0.5) are folded.
   * @param num_in     Fixed input block size in samples.  All subsequent
   *                   calls to dp_ddc_execute must pass this exact count.
   * @param rate       fs_out / fs_in.  Must be > 0.
   * @return           Heap-allocated DDC state, or NULL on failure.
   */
  dp_ddc_t *dp_ddc_create (float norm_freq, size_t num_in, double rate);

  /* ------------------------------------------------------------------
   * Lifecycle — custom resampler
   * ------------------------------------------------------------------ */

  /**
   * @brief Create a DDC with a caller-supplied resampler.
   *
   * Use when the built-in coefficients are not sufficient — for example,
   * when a rate-matched design from `doppler.polyphase.optimize_dpmfs`
   * is required.
   *
   * The DDC takes **ownership** of @p r.  The caller must not use or
   * destroy @p r after this call.  On failure, @p r is destroyed so the
   * caller does not leak it.
   *
   * Passing @p r = NULL creates a DDC without a resampler (passthrough
   * at the input rate, same as dp_ddc_create with rate = 1.0).
   *
   * @param norm_freq  NCO normalised frequency (see dp_ddc_create).
   * @param num_in     Fixed input block size in samples.
   * @param r          Pre-built DPMFS resampler, or NULL.
   * @return           Heap-allocated DDC state, or NULL on failure.
   */
  dp_ddc_t *dp_ddc_create_custom (float norm_freq, size_t num_in,
                                  dp_resamp_dpmfs_t *r);

  /**
   * @brief Destroy the DDC and release all resources.
   *
   * Also destroys the resampler that was passed to dp_ddc_create or
   * dp_ddc_create_custom.
   *
   * @param ddc  May be NULL (no-op).
   */
  void dp_ddc_destroy (dp_ddc_t *ddc);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  /**
   * @brief Return the maximum output samples per dp_ddc_execute call.
   *
   * Allocate at least this many CF32 samples for the @p out buffer
   * passed to dp_ddc_execute.  The value is fixed at creation time.
   *
   * Without a resampler: equals @p num_in.
   * With a resampler at rate @p r: equals ⌈num_in × r⌉ + 4.
   *
   * @param ddc  Must be non-NULL.
   */
  size_t dp_ddc_max_out (const dp_ddc_t *ddc);

  /**
   * @brief Return the actual output sample count from the last
   *        dp_ddc_execute call.
   *
   * Equals the value returned by dp_ddc_execute.  With a resampler this
   * can vary by ±1 from call to call (phase accumulator rounding); it is
   * always ≤ dp_ddc_max_out().
   *
   * Zero before the first dp_ddc_execute call.
   *
   * @param ddc  Must be non-NULL.
   */
  size_t dp_ddc_nout (const dp_ddc_t *ddc);

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
   * Each input sample is multiplied by the NCO phasor, then the mixed
   * block is fed through the resampler (if configured) into @p out.
   * NCO phase and resampler history are preserved across calls.
   *
   * @p num_in should equal the value passed to dp_ddc_create /
   * dp_ddc_create_custom.  Passing a smaller value is safe but wastes
   * the pre-allocated buffer capacity.
   *
   * Pass `dp_ddc_max_out(ddc)` as @p max_out to guarantee all output
   * samples are captured.
   *
   * @param ddc      Must be non-NULL.
   * @param in       Input samples, CF32, length ≥ @p num_in.
   * @param num_in   Number of input samples to process.
   * @param out      Output buffer, CF32, capacity ≥ @p max_out.
   * @param max_out  Maximum output samples to write.
   * @return         Number of output samples written (0 if num_in == 0).
   */
  size_t dp_ddc_execute (dp_ddc_t *ddc, const dp_cf32_t *in, size_t num_in,
                         dp_cf32_t *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DP_DDC_H */
