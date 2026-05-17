/**
 * @file nco_core.h
 * @brief Pure 32-bit phase-accumulator NCO.
 *
 * Phase increments by phase_inc each sample and wraps naturally at 2^32.
 * Three output mappings:
 *
 *   nco_steps_u32        raw accumulator value  [0, 2^32)
 *   nco_steps_u32_scaled (uint64)phase * nmax >> 32  →  [0, nmax)
 *   nco_steps_u32_ovf    raw phase + per-sample carry flag
 *
 * nmax=0 in nco_steps_u32_scaled is treated identically to
 * nco_steps_u32 (returns raw accumulator unchanged).
 *
 * Normalised-frequency → phase_inc conversion:
 *   phase_inc = floor((norm_freq mod 1.0) × 2^32)
 *
 * Negative frequencies fold correctly: −0.25 → phase_inc = 3×2^30.
 *
 * reset() zeroes phase only; norm_freq and nmax are unchanged.
 *
 * Lifecycle: nco_create → [steps / reset]* → nco_destroy
 *
 * @code
 * nco_state_t *nco = nco_create(0.25, 0);
 * uint32_t out[4];
 * nco_steps_u32(nco, 4, out);
 * // out[0]=0x00000000, out[1]=0x40000000,
 * // out[2]=0x80000000, out[3]=0xC0000000
 * nco_destroy(nco);
 * @endcode
 */
#ifndef NCO_CORE_H
#define NCO_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Wrapping add with carry detection.
 *
 * NCO_ADD_OVF(a, b, res) computes *res = a + b and returns 1 if the
 * addition wrapped (carry out), 0 otherwise.  Branchless on x86/AArch64.
 */
#if defined(__GNUC__) || defined(__clang__)
#define NCO_ADD_OVF(a, b, res)                                                \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),             \
                                    (uint32_t *)(res)))
#else
static inline uint8_t
nco_add_ovf_ (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define NCO_ADD_OVF(a, b, res) nco_add_ovf_ ((a), (b), (res))
#endif

  /**
   * @brief NCO state.
   *
   * Allocate with nco_create().  All fields are managed by the library;
   * read phase and phase_inc via the property accessors.
   */
  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)         */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)          */
    uint32_t nmax;      /* wrap target for steps_u32_scaled; 0 = raw   */
  } nco_state_t;

  /**
   * @brief Create an NCO instance.
   *
   * @param norm_freq  Normalised frequency (cycles per sample).
   *                   Any value; fractional part used internally.
   * @param nmax       Wrap target for nco_steps_u32_scaled.
   *                   Pass 0 to return the raw 32-bit accumulator.
   * @return Heap-allocated state, or NULL on allocation failure.
   */
  nco_state_t *nco_create (double norm_freq, uint32_t nmax);

  /** Free all resources.  May be NULL (no-op). */
  void nco_destroy (nco_state_t *state);

  /** Zero the phase accumulator.  norm_freq and nmax are unchanged. */
  void nco_reset (nco_state_t *state);

  /* ---- Properties ---- */

  double nco_get_norm_freq (const nco_state_t *state);
  void nco_set_norm_freq (nco_state_t *state, double norm_freq);
  uint32_t nco_get_phase (const nco_state_t *state);
  void nco_set_phase (nco_state_t *state, uint32_t phase);
  uint32_t nco_get_phase_inc (const nco_state_t *state);

  /* ---- Block generators ---- */

  /**
   * @brief Maximum samples per call (determines pre-allocated buffer size).
   *
   * The Python extension pre-allocates output buffers of this size at
   * create time.  Requesting more samples per call is undefined behaviour.
   */
  size_t nco_steps_u32_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; write raw uint32 accumulator values.
   *
   * Output is emitted before increment: out[0] = current phase,
   * out[1] = phase + phase_inc, etc.  Returns n.
   */
  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_scaled_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; values scaled to [0, nmax).
   *
   * Uses the branchless fixed-point identity:
   *   out[i] = (uint64_t)phase * nmax >> 32
   * When nmax == 0 falls back to the raw accumulator.
   */
  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_ovf_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; write raw phase values and per-sample carry.
   *
   * out[i]  — raw 32-bit phase value (same as nco_steps_u32).
   * out1[i] — 1 when the accumulator wrapped on sample i, 0 otherwise.
   * The carry marks the boundary of one input period; useful for
   * polyphase sample-clock generation.
   */
  size_t nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
