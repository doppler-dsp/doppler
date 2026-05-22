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
   * @brief Square-clip a complex sample.
   *
   * Clips the real and imaginary parts independently to &#91;-lin, lin&#93; —
   * a square region in the IQ plane, not a circular magnitude limit.
   *
   * @param y    Input sample.
   * @param lin  Per-component clip threshold (linear amplitude, >= 0).
   * @return Sample with each component limited to &#91;-lin, lin&#93;.
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
