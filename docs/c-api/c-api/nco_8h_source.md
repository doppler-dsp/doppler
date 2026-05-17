

# File nco.h

[**File List**](files.md) **>** [**ddc**](dir_b33dc116452ac5c7d7799725e78b6bdc.md) **>** [**nco.h**](nco_8h.md)

[Go to the documentation of this file](nco_8h.md)


```C++


#ifndef NATIVE_DDC_NCO_H
#define NATIVE_DDC_NCO_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct dp_nco dp_nco_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  dp_nco_t *dp_nco_create (float norm_freq);

  void dp_nco_set_freq (dp_nco_t *nco, float norm_freq);

  float dp_nco_get_freq (const dp_nco_t *nco);

  uint32_t dp_nco_get_phase (const dp_nco_t *nco);

  uint32_t dp_nco_get_phase_inc (const dp_nco_t *nco);

  void dp_nco_reset (dp_nco_t *nco);

  void dp_nco_destroy (dp_nco_t *nco);

  /* ------------------------------------------------------------------
   * Execute — free-running
   * ------------------------------------------------------------------ */

  void dp_nco_execute_cf32 (dp_nco_t *nco, float _Complex *out, size_t n);

  /* ------------------------------------------------------------------
   * Execute — with per-sample phase-increment control port
   * ------------------------------------------------------------------ */

  void dp_nco_execute_cf32_ctrl (dp_nco_t *nco, const float *ctrl,
                                 float _Complex *out, size_t n);

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

  void dp_nco_execute_u32 (dp_nco_t *nco, uint32_t *out, size_t n);

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

  void dp_nco_execute_u32_ovf (dp_nco_t *nco, uint32_t *out, uint8_t *carry,
                               size_t n);

  void dp_nco_execute_u32_ovf_ctrl (dp_nco_t *nco, const float *ctrl,
                                    uint32_t *out, uint8_t *carry, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DP_NCO_H */
```
