/**
 * @file nco.h
 * @brief Numerically Controlled Oscillator (NCO).
 *
 * Implements a phase-accurate NCO using a 32-bit unsigned overflowing
 * phase accumulator and a 2^16-entry single-precision sine LUT.
 *
 * The normalised frequency @p f_n maps to the accumulator as:
 * @code
 *   phase_inc = (uint32_t)(f_n * 2^32)
 * @endcode
 * where @p f_n = f / fs (cycles per sample).  @p f_n = 0.5 produces
 * the Nyquist tone; negative values produce conjugate-sense rotation.
 * Values outside [−0.5, 0.5) are folded by unsigned wrap-around so
 * the full ±Nyquist range is always reachable.
 *
 * The control port (dp_nco_execute_cf32_ctrl) accepts a per-sample
 * normalised-frequency deviation that is added to the base phase
 * increment before each sample — standard FM-modulator topology:
 * @code
 *   inc_i     = phase_inc + (uint32_t)(ctrl[i] * 2^32)
 *   phase    += inc_i
 *   out[i]    = { cos(2π·phase/2^32), sin(2π·phase/2^32) }
 * @endcode
 *
 * Phase precision: 16 bits (top 16 bits of the 32-bit accumulator
 * index the LUT; the lower 16 bits are truncated, giving a worst-case
 * spurious level of ~−96 dBc).  Amplitude accuracy is determined
 * solely by float32 precision of sinf() at LUT construction time.
 *
 * **Example — free-running quarter-rate tone:**
 * ```c
 * #include <dp/nco.h>
 *
 * dp_nco_t   *nco = dp_nco_create(0.25f);  // f = fs/4
 * dp_cf32_t   out[256];
 * dp_nco_execute_cf32(nco, out, 256);
 * dp_nco_destroy(nco);
 * ```
 *
 * **Example — FM modulation:**
 * ```c
 * float mod[256] = { ... };           // normalised freq deviations
 * dp_nco_t  *nco = dp_nco_create(0.1f);
 * dp_cf32_t  out[256];
 * dp_nco_execute_cf32_ctrl(nco, mod, out, 256);
 * dp_nco_destroy(nco);
 * ```
 */

#ifndef DP_NCO_H
#define DP_NCO_H

#include <dp/stream.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Opaque NCO state. */
  typedef struct dp_nco dp_nco_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  /**
   * @brief Create an NCO at the given normalised frequency.
   *
   * Initialises the global 2^16-entry sine LUT on the first call
   * (one-time cost; the table is never freed).  The phase accumulator
   * starts at zero.
   *
   * @param norm_freq  Normalised frequency f/fs (cycles per sample).
   *                   Typical range [−0.5, 0.5); values outside this
   *                   range are folded via unsigned 32-bit arithmetic.
   * @return           Heap-allocated NCO state, or NULL on failure.
   */
  dp_nco_t *dp_nco_create (float norm_freq);

  /**
   * @brief Change the centre frequency without resetting the phase.
   *
   * Takes effect on the next call to an execute function.
   *
   * @param nco        NCO state (must be non-NULL).
   * @param norm_freq  New normalised frequency (same convention as
   *                   dp_nco_create).
   */
  void dp_nco_set_freq (dp_nco_t *nco, float norm_freq);

  /**
   * @brief Return the current normalised frequency.
   *
   * Returns the value last passed to dp_nco_create() or
   * dp_nco_set_freq() — no conversion from the phase accumulator.
   *
   * @param nco  Must be non-NULL.
   */
  float dp_nco_get_freq (const dp_nco_t *nco);

  /**
   * @brief Return the current raw 32-bit phase accumulator value.
   *
   * The phase advances by @c phase_inc on every execute tick.
   * Convert to normalised units: @c phase / 2^32.
   *
   * @param nco  Must be non-NULL.
   */
  uint32_t dp_nco_get_phase (const dp_nco_t *nco);

  /**
   * @brief Return the phase increment (fixed-point frequency).
   *
   * This is the value added to the phase accumulator on every execute
   * tick: @c phase_inc = round(norm_freq × 2^32).  Directly usable as
   * the NCO step in a polyphase branch selector:
   * @code
   *   branch = (phase + phase_inc) >> (32 - log2(num_phases));
   * @endcode
   *
   * @param nco  Must be non-NULL.
   */
  uint32_t dp_nco_get_phase_inc (const dp_nco_t *nco);

  /**
   * @brief Reset the phase accumulator to zero.
   *
   * Does not change the centre frequency.  Use after a stream
   * discontinuity to restart from a known phase reference.
   *
   * @param nco  NCO state (must be non-NULL).
   */
  void dp_nco_reset (dp_nco_t *nco);

  /**
   * @brief Destroy the NCO and release all memory.
   * @param nco  NCO state (may be NULL).
   */
  void dp_nco_destroy (dp_nco_t *nco);

  /* ------------------------------------------------------------------
   * Execute — free-running
   * ------------------------------------------------------------------ */

  /**
   * @brief Generate @p n complex samples from the free-running NCO.
   *
   * For each sample @p i the phase accumulator advances by
   * @p phase_inc (set from the normalised frequency), then the output
   * is read from the sine LUT:
   * @code
   *   out[i].i = cos(2π × phase / 2^32)   (in-phase)
   *   out[i].q = sin(2π × phase / 2^32)   (quadrature)
   * @endcode
   * The accumulator wraps silently via uint32 overflow.
   *
   * @param nco  NCO state.
   * @param out  Output array of @p n CF32 samples.
   * @param n    Number of samples to generate.
   */
  void dp_nco_execute_cf32 (dp_nco_t *nco, dp_cf32_t *out, size_t n);

  /* ------------------------------------------------------------------
   * Execute — with per-sample phase-increment control port
   * ------------------------------------------------------------------ */

  /**
   * @brief Generate @p n complex samples with a per-sample frequency
   *        deviation input (FM-modulator / phase-increment port).
   *
   * For each sample @p i the effective phase increment is:
   * @code
   *   inc_i = phase_inc + (uint32_t)(ctrl[i] × 2^32)
   * @endcode
   * where @p ctrl[i] is a normalised frequency deviation (same units
   * as the @p norm_freq argument to dp_nco_create).  The base phase
   * increment stored in the NCO state is not modified; only the
   * instantaneous frequency is perturbed.
   *
   * @param nco   NCO state.
   * @param ctrl  Per-sample normalised-frequency deviations (length
   *              ≥ @p n).
   * @param out   Output array of @p n CF32 samples.
   * @param n     Number of samples to generate.
   */
  void dp_nco_execute_cf32_ctrl (dp_nco_t *nco, const float *ctrl,
                                 dp_cf32_t *out, size_t n);

  /* ------------------------------------------------------------------
   * Execute — raw uint32 phase output
   *
   * These variants skip the sine LUT entirely and deliver the raw
   * 32-bit phase accumulator value for each sample.  The output is
   * the phase used to produce that sample (i.e. the value before the
   * increment is applied).
   *
   *   out[i] = phase_after_i_increments
   *
   * Useful when a downstream block supplies its own LUT or when the
   * phase value itself is the signal of interest (e.g. FM
   * demodulation, phase ramp generation, cross-correlator input).
   * ------------------------------------------------------------------ */

  /**
   * @brief Output raw uint32 phase values, free-running.
   *
   * @param nco  NCO state.
   * @param out  Output array of @p n uint32_t phase values.
   * @param n    Number of samples.
   */
  void dp_nco_execute_u32 (dp_nco_t *nco, uint32_t *out, size_t n);

  /**
   * @brief Output raw uint32 phase values with per-sample ctrl port.
   *
   * @param nco   NCO state.
   * @param ctrl  Per-sample normalised-frequency deviations.
   * @param out   Output array of @p n uint32_t phase values.
   * @param n     Number of samples.
   */
  void dp_nco_execute_u32_ctrl (dp_nco_t *nco, const float *ctrl,
                                uint32_t *out, size_t n);

  /* ------------------------------------------------------------------
   * Execute — raw uint32 phase + per-sample overflow / carry bit
   *
   * Identical to the u32 variants above, but also writes a carry flag
   * for each sample:
   *
   *   carry[i] = 1  if  phase + phase_inc wrapped past 2^32
   *            = 0  otherwise
   *
   * A rising carry edge marks the completion of one full phase
   * rotation (i.e. one period of the NCO frequency).  Useful for
   * cycle counting, PLL feedback, and zero-crossing detection.
   *
   * Implementation notes
   * --------------------
   * On GCC / Clang the carry is detected with __builtin_add_overflow,
   * which maps directly to the CPU carry flag (ADD + SETB on x86,
   * ADDS + CSET on AArch64) with no branch.  On other compilers the
   * equivalent comparison idiom is used:
   *
   *   uint32_t new_ph = ph + inc;
   *   carry = (new_ph < ph);         // well-defined; same codegen
   *   ph = new_ph;
   *
   * Both forms compile to identical object code on all major targets;
   * the builtin is preferred because it expresses intent directly and
   * avoids a redundant comparison on compilers that miss the pattern.
   * ------------------------------------------------------------------ */

  /**
   * @brief Output raw uint32 phase + carry bit, free-running.
   *
   * @param nco    NCO state.
   * @param out    Output array of @p n uint32_t phase values.
   * @param carry  Output array of @p n uint8_t carry flags.
   * @param n      Number of samples.
   */
  void dp_nco_execute_u32_ovf (dp_nco_t *nco, uint32_t *out,
                               uint8_t *carry, size_t n);

  /**
   * @brief Output raw uint32 phase + carry bit with ctrl port.
   *
   * @param nco    NCO state.
   * @param ctrl   Per-sample normalised-frequency deviations.
   * @param out    Output array of @p n uint32_t phase values.
   * @param carry  Output array of @p n uint8_t carry flags.
   * @param n      Number of samples.
   */
  void dp_nco_execute_u32_ovf_ctrl (dp_nco_t *nco, const float *ctrl,
                                    uint32_t *out, uint8_t *carry,
                                    size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_NCO_H */
