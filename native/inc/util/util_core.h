/**
 * @file util_core.h
 * @brief Util module — public C API.
 *
 * The util functions are header-only and JM_FORCEINLINE: any caller
 * that includes this header inlines them with zero call overhead, and
 * the util Python extension module exposes the very same definitions.
 * There is one source of truth per function, here.
 */
#ifndef UTIL_CORE_H
#define UTIL_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Square-clip a complex sample: clip the real and imaginary
   * parts independently to `[-lin, lin]` (a square region in the IQ
   * plane, not a circular magnitude limit).  Each component is passed
   * through unchanged when its magnitude is within the threshold and
   * clamped to the nearest boundary otherwise.
   *
   * @param y    Complex CF32 input sample.
   * @param lin  Per-component clip threshold (linear amplitude, >= 0).
   *             Values outside `[-lin, lin]` are clamped; values on the
   *             boundary are preserved exactly.
   * @return Sample with each component limited to `[-lin, lin]`.
   * @code
   * >>> from doppler.util import square_clip
   * >>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through
   * (0.5+0.25j)
   * >>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged
   * (1+0.5j)
   * >>> square_clip(3.0-4.0j, 1.0)    # both components clipped
   * (1-1j)
   * >>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both
   * (0.25+0.25j)
   * >>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped
   * (-1+0j)
   * @endcode
   */
  JM_FORCEINLINE float complex
  square_clip (float complex y, float lin)
  {
    float r = fminf (fmaxf (crealf (y), -lin), lin);
    float i = fminf (fmaxf (cimagf (y), -lin), lin);
    return r + i * I;
  }
#ifdef __cplusplus
}
#endif

#endif /* UTIL_CORE_H */
