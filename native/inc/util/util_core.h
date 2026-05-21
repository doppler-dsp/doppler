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
   * @brief Smallest power of two >= n.
   *
   * Returns 1 when n == 0.  Useful for sizing ring buffers and FFT plans
   * that require power-of-two lengths.
   *
   * @param n  Input value.
   * @return   Smallest p = 2^k such that p >= n (p >= 1).
   *
   * Example:
   * @code
   *   assert(dp_next_pow2(0) == 1);
   *   assert(dp_next_pow2(1) == 1);
   *   assert(dp_next_pow2(5) == 8);
   *   assert(dp_next_pow2(8) == 8);
   * @endcode
   */
  JM_FORCEINLINE size_t
  dp_next_pow2 (size_t n)
  {
    if (n == 0)
      return 1;
    size_t p = 1;
    while (p < n)
      p <<= 1;
    return p;
  }

  /**
   * @brief Square-clip a complex sample.
   *
   * Clips the real and imaginary parts independently to [-lin, lin] —
   * a square region in the IQ plane, not a circular magnitude limit.
   *
   * @param y    Input sample.
   * @param lin  Per-component clip threshold (linear amplitude, >= 0).
   * @return Sample with each component limited to [-lin, lin].
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
