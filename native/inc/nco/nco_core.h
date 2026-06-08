/**
 * @file nco_core.h
 * @brief Pure 32-bit phase-accumulator NCO.
 *
 * Implements a numerically-controlled oscillator whose 32-bit phase
 * register advances by phase_inc every sample and wraps naturally at
 * 2^32, giving exact integer arithmetic with no floating-point drift.
 * Three output mappings expose different views of the accumulator:
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
 * Lifecycle: nco_create → (steps / reset)* → nco_destroy
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
   * Allocates and initialises the phase accumulator to zero, converts
   * norm_freq to the integer phase_inc = floor(frac(norm_freq) × 2^32),
   * and stores nmax for scaled output.  The NCO is immediately ready to
   * call nco_steps_u32 / nco_steps_u32_scaled / nco_steps_u32_ovf.
   *
   * @param norm_freq  Normalised frequency in cycles per sample.
   *                   Any real value; only the fractional part matters.
   *                   Negative values fold correctly (−0.25 → 3×2^30).
   * @param nmax       Wrap target for nco_steps_u32_scaled.
   *                   Pass 0 to return the raw 32-bit accumulator.
   * @return Heap-allocated state, or NULL on allocation failure.
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(norm_freq=0.25, nmax=0)
   * >>> nco.phase_inc
   * 1073741824
   * @endcode
   */
  nco_state_t *nco_create (double norm_freq, uint32_t nmax);

  /** Free all resources.  May be NULL (no-op). */
  void nco_destroy (nco_state_t *state);

  /**
   * @brief Zero the phase accumulator.
   * Sets phase to 0 so the next nco_steps_u32 call starts from the
   * beginning of the cycle.  norm_freq, phase_inc, and nmax are
   * unchanged; the NCO is ready to generate samples again immediately.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> _ = nco.steps_u32(2)
   * >>> nco.phase
   * 2147483648
   * >>> nco.reset()
   * >>> nco.phase
   * 0
   * >>> nco.norm_freq
   * 0.25
   * @endcode
   */
  void nco_reset (nco_state_t *state);

  /* ---- Properties ---- */

  /**
   * @brief Normalised frequency (read/write).
   * Setting norm_freq recomputes phase_inc = floor(frac(v) × 2^32) and
   * takes effect on the next nco_steps_* call; phase is NOT reset.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.norm_freq
   * 0.25
   * >>> nco.norm_freq = 0.5
   * >>> nco.phase_inc
   * 2147483648
   * @endcode
   */
  double nco_get_norm_freq (const nco_state_t *state);
  void nco_set_norm_freq (nco_state_t *state, double norm_freq);

  /**
   * @brief Current phase accumulator value (read/write).
   * Reading returns the current integer phase in [0, 2^32).  Writing
   * overrides the accumulator directly, allowing arbitrary phase offsets
   * without re-creating the NCO.
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.phase
   * 0
   * >>> nco.phase = 2147483648
   * >>> nco.phase
   * 2147483648
   * @endcode
   */
  uint32_t nco_get_phase (const nco_state_t *state);
  void nco_set_phase (nco_state_t *state, uint32_t phase);

  /**
   * @brief Per-sample phase increment (read-only).
   * Derived from norm_freq as floor(frac(norm_freq) × 2^32).  Updated
   * automatically whenever norm_freq is written.  A freq of 0.25 gives
   * phase_inc = 1073741824 (0x40000000).
   *
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> nco.phase_inc
   * 1073741824
   * @endcode
   */
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
   * Each element is the phase value BEFORE the increment fires, so
   * out[0] is the phase at the moment of the call.  The accumulator
   * wraps silently at 2^32, giving the full-resolution integer ramp
   * that the scaled and carry variants derive from.  Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Output buffer; must hold at least n uint32_t values.
   * @return n (always).
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 0)
   * >>> out = nco.steps_u32(4)
   * >>> out.dtype
   * dtype('uint32')
   * >>> out.tolist()
   * [0, 1073741824, 2147483648, 3221225472]
   * @endcode
   */
  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_scaled_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; values scaled to [0, nmax).
   * Uses the branchless fixed-point identity
   *   out[i] = (uint64_t)phase * nmax >> 32
   * to map the full accumulator range uniformly onto [0, nmax) without
   * a modulo operation.  When nmax == 0 falls back to the raw accumulator
   * (identical to nco_steps_u32).  Useful for polyphase filter bank
   * indexing and direct LUT addressing.  Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Output buffer; must hold at least n uint32_t values.
   * @return n (always).
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.25, 4)
   * >>> out = nco.steps_u32_scaled(4)
   * >>> out.dtype
   * dtype('uint32')
   * >>> out.tolist()
   * [0, 1, 2, 3]
   * @endcode
   */
  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_ovf_max_out (nco_state_t *state);

  /**
   * @brief Advance n samples; write raw phase values and per-sample carry.
   * Identical to nco_steps_u32 for the phase array, but simultaneously
   * fills a parallel uint8 carry buffer: out1[i] is 1 if the add that
   * produced out[i]'s post-increment phase wrapped past 2^32, else 0.
   * The carry marks the exact boundary of one input period and is the
   * primitive for polyphase sample-clock and rational resampling engines.
   * Returns n.
   *
   * @param state  NCO state returned by nco_create().
   * @param n      Number of samples to generate.
   * @param out    Phase output buffer; must hold at least n uint32_t values.
   * @param out1   Carry output buffer; must hold at least n uint8_t values.
   * @return n (always).
   * @code
   * >>> from doppler.source import NCO
   * >>> nco = NCO(0.5, 0)
   * >>> ph, carry = nco.steps_u32_ovf(4)
   * >>> ph.tolist()
   * [0, 2147483648, 0, 2147483648]
   * >>> carry.tolist()
   * [0, 1, 0, 1]
   * >>> carry.dtype
   * dtype('uint8')
   * @endcode
   */
  size_t nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
