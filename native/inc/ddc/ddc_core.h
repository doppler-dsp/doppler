/**
 * @file ddc_core.h
 * @brief Digital Down-Converter — composes LO + polyphase resampler.
 *
 * Two types:
 *
 *   Ddc   — complex (CF32) input.  Chain: LO mix → resample.
 *   DdcR  — real (float32) input.  Chain: halfband R2C → LO mix → resample.
 *
 * Both are streaming (variable block size per execute call).
 *
 * ### Ddc signal chain
 *
 * ```
 * CF32 in (fs_in)  →  LO mix  →  resample  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  NCO normalised frequency (cycles/sample at fs_in).
 *             Set to -f_carrier to shift a carrier at f_carrier to DC.
 *
 * ### DdcR signal chain
 *
 * ```
 * float in (fs_in)  →  halfband R2C (2:1, embedded fs/4 shift)
 *                   →  LO mix at intermediate rate (fs_in/2)
 *                   →  resample  →  CF32 out (fs_out)
 * ```
 *
 * norm_freq:  Fine NCO frequency at the INTERMEDIATE rate (fs_in/2).
 *             To tune a real tone at f_carrier (input normalised) to DC:
 *             set norm_freq = -(2*f_carrier + 0.5).
 *             Total output rate: fs_out = rate * fs_in  (rate < 0.5).
 */
#ifndef DDC_CORE_H
#define DDC_CORE_H

#include <complex.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ================================================================== */
  /* Ddc — complex-input DDC                                            */
  /* ================================================================== */

  typedef struct ddc_state ddc_state_t;

  /**
   * @brief Create a complex-input DDC.
   *
   * @param norm_freq  LO frequency in cycles/sample at the input rate.
   * @param rate       Output rate / input rate.  Must be > 0.
   * @return Non-NULL on success, NULL on OOM or invalid args.
   */
  ddc_state_t *ddc_create (double norm_freq, double rate);

  /** Free all resources.  NULL is a no-op. */
  void ddc_destroy (ddc_state_t *s);

  /** Zero LO phase and resampler history. */
  void ddc_reset (ddc_state_t *s);

  /** Return the current LO normalised frequency. */
  double ddc_get_norm_freq (const ddc_state_t *s);

  /**
   * @brief Retune the LO without resetting phase or resampler history.
   * @param norm_freq  New normalised frequency.
   */
  void ddc_set_norm_freq (ddc_state_t *s, double norm_freq);

  /** Return the configured output/input rate ratio. */
  double ddc_get_rate (const ddc_state_t *s);

  /**
   * @brief Mix and resample a block of CF32 samples.
   *
   * @param s        Must be non-NULL.
   * @param in       Input samples, complex64, length n_in.
   * @param n_in     Number of input samples.
   * @param out      Output buffer, complex64, capacity max_out.
   * @param max_out  Maximum output samples to write.
   * @return Number of output samples written.
   */
  size_t ddc_execute (ddc_state_t *s, const float _Complex *in, size_t n_in,
                      float _Complex *out, size_t max_out);

  /* ================================================================== */
  /* DdcR — real-input DDC (Architecture D2)                           */
  /* ================================================================== */

  typedef struct ddcr_state ddcr_state_t;

  /**
   * @brief Create a real-input DDC.
   *
   * @param norm_freq  Fine NCO frequency at the intermediate rate (fs_in/2).
   * @param rate       Total output/input rate.  Must be in (0, 0.5).
   * @return Non-NULL on success, NULL on OOM or invalid args.
   */
  ddcr_state_t *ddcr_create (double norm_freq, double rate);

  /** Free all resources.  NULL is a no-op. */
  void ddcr_destroy (ddcr_state_t *s);

  /** Zero halfband, LO phase, and resampler history. */
  void ddcr_reset (ddcr_state_t *s);

  /** Return the current fine NCO normalised frequency (at intermediate rate).
   */
  double ddcr_get_norm_freq (const ddcr_state_t *s);

  /** Retune the fine NCO without resetting state. */
  void ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq);

  /** Return the total configured rate (fs_out / fs_in). */
  double ddcr_get_rate (const ddcr_state_t *s);

  /**
   * @brief Process a block of real float32 samples.
   *
   * @param s        Must be non-NULL.
   * @param in       Real input samples, float32, length n_in.
   * @param n_in     Number of input samples.
   * @param out      CF32 output buffer, capacity max_out.
   * @param max_out  Maximum output samples to write.
   * @return Number of output samples written.
   */
  size_t ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
                       float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* DDC_CORE_H */
