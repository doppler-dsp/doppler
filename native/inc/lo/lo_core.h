/**
 * @file lo_core.h
 * @brief Local oscillator: NCO + 2^16 sin/cos LUT → CF32 phasors.
 *
 * The 32-bit phase accumulator drives a static 65536-entry float sine LUT.
 * The top 16 bits of the phase select the LUT index; a quarter-cycle offset
 * (LUT_QTR = 16384) converts sin to cos without extra storage:
 *
 *   idx      = phase >> 16
 *   out(i)   = cos(θ) + j·sin(θ)
 *            = lut((idx + LUT_QTR) & 0xFFFF) + j·lut(idx)
 *
 * Output is emitted BEFORE the phase is incremented (same convention as NCO).
 *
 * The 16-bit phase truncation gives ~96 dBc SFDR.
 *
 * Lifecycle: lo_create → (steps / reset)* → lo_destroy
 *
 * @code
 * lo_state_t *lo = lo_create(0.25);
 * float complex out[4];
 * lo_steps(lo, 4, out);
 * // out ≈ { 1+0j, 0+1j, -1+0j, 0-1j }
 * lo_destroy(lo);
 * @endcode
 */
#ifndef LO_CORE_H
#define LO_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief LO state.
   *
   * Allocate with lo_create().  The shared 65536-entry LUT is initialised
   * lazily on the first lo_create() call and never freed.
   */
  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } lo_state_t;

  /**
   * @brief Create an LO instance.
   *
   * Initialises the shared LUT on first call (thread-safe concern: use
   * from a single thread or call lo_create() before spawning threads).
   *
   * @param norm_freq  Normalised frequency (cycles per sample).
   *                   Any value; fractional part used internally.
   * @return Heap-allocated state, or NULL on allocation failure.
   */
  lo_state_t *lo_create (double norm_freq);

  /** Free all resources.  May be NULL (no-op). */
  void lo_destroy (lo_state_t *state);

  /** Zero the phase accumulator.  norm_freq is unchanged. */
  void lo_reset (lo_state_t *state);

  /* ---- Properties ---- */

  double lo_get_norm_freq (const lo_state_t *state);
  void lo_set_norm_freq (lo_state_t *state, double norm_freq);
  uint32_t lo_get_phase (const lo_state_t *state);
  void lo_set_phase (lo_state_t *state, uint32_t phase);
  uint32_t lo_get_phase_inc (const lo_state_t *state);

  /* ---- Block generators ---- */

  /**
   * @brief Maximum samples per call (determines pre-allocated buffer size).
   */
  size_t lo_steps_max_out (lo_state_t *state);

  /**
   * @brief Generate n CF32 phasors at the current norm_freq.
   *
   * Output is emitted before increment: out(0) corresponds to the phase
   * at entry, out(1) to phase + phase_inc, etc.  Returns n.
   */
  size_t lo_steps (lo_state_t *state, size_t n, float complex *out);

  size_t lo_steps_ctrl_max_out (lo_state_t *state);

  /**
   * @brief Generate CF32 phasors with per-sample FM deviation.
   *
   * ctrl(i) (real float, fractional part used) is converted to a per-sample
   * phase-increment delta added on top of the base phase_inc.  The base
   * norm_freq is not modified.
   *
   * Output length equals ctrl_len.  Returns ctrl_len.
   */
  size_t lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
                        float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
