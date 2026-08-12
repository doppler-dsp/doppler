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

  /**
   * @brief Saturate a value into `[lo, hi]`, **total over every double** —
   * including NaN and both infinities.
   *
   * `fmin`/`fmax` are not enough for this job.  A plain
   * `fmin(fmax(v, lo), hi)` propagates NaN on some platforms and silently
   * returns a bound on others, and a hand-written `v > hi ? hi : v` leaves
   * NaN untouched, because every comparison against NaN is false.  This
   * function has no fall-through: a value that is neither inside the
   * interval, nor below it, nor above it can only be NaN.
   *
   * @par Why the NaN destination is the caller's
   * Which end is *safe* is domain knowledge, not arithmetic.  A gain
   * control guarding a measured power wants NaN at the **ceiling** — an
   * unknown level must drive the gain down, because too little gain loses a
   * signal while too much rails everything downstream.  A lock statistic
   * wants NaN at the **floor** — an unknown lock is not a lock.  Baking
   * either choice in would hand the wrong default to half its callers, so
   * `nan_to` is a parameter and each call site states its own safe
   * direction.
   *
   * @par Where to use it
   * At the boundary where an untrusted value first becomes **persistent
   * state** — the input of an EMA, an accumulator, or an integrator.  Ahead
   * of that boundary a bad value corrupts one output and is gone; past it,
   * it is remembered and every quantity derived from it inherits the
   * damage.  One guard there makes the whole downstream chain total, where
   * a clamp at each stage is several chances to miss one.
   *
   * @param v       Value to saturate.  Any double.
   * @param lo      Lower bound, returned for any `v < lo`.
   * @param hi      Upper bound, returned for any `v > hi`.
   * @param nan_to  Returned when `v` is NaN.  Pick the end that is safe in
   *                the caller's own terms; it is usually `lo` or `hi`.
   * @return `v` when `lo <= v <= hi`, otherwise `lo`, `hi` or `nan_to`.
   * @code
   * >>> from doppler.util import saturate
   * >>> saturate(0.5, 0.0, 1.0, 1.0)     # inside the interval
   * 0.5
   * >>> saturate(2.0, 0.0, 1.0, 1.0)     # above the ceiling
   * 1.0
   * >>> saturate(-3.0, 0.0, 1.0, 1.0)    # below the floor
   * 0.0
   * >>> saturate(float("inf"), 0.0, 1.0, 1.0)   # infinity is just above
   * 1.0
   * >>> saturate(float("nan"), 0.0, 1.0, 1.0)   # NaN takes the caller's end
   * 1.0
   * >>> saturate(float("nan"), 0.0, 1.0, 0.0)   # ... which may be the other
   * 0.0
   * @endcode
   */
  JM_FORCEINLINE double
  saturate (double v, double lo, double hi, double nan_to)
  {
    if (v >= lo && v <= hi)
      return v; /* the common case; false for NaN, which falls through */
    if (v < lo)
      return lo;
    if (v > hi)
      return hi;
    return nan_to; /* nothing else can reach here */
  }
#ifdef __cplusplus
}
#endif

#endif /* UTIL_CORE_H */
