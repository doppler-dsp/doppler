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

  /**
   * @brief One step of a first-order exponential moving average:
   * `state <- state + alpha * (x - state)`.
   *
   * The canonical EMA for the whole library.  It was written out four
   * times before this existed — `agc` (power detector),
   * `async_dsss_receiver` (the lock_num/lock_den pair), `acc_trace`
   * (ACC_TRACE_EXP) and the recursion `det_ema_alpha` sizes — in **two
   * different algebraic forms**, which are identical on paper and not in
   * floating point.  Duplicated implementations drift; this is the one.
   *
   * ### Why this form, and not `alpha*x + (1-alpha)*state`
   *
   * Both were measured against a 60-digit reference over 5000 steps.  The
   * incremental form written here is the more accurate one everywhere the
   * library actually operates, by a margin that grows as the average gets
   * longer — which is the direction a narrow-band estimator moves:
   *
   * | `alpha` | this form | `alpha*x + (1-alpha)*state` |
   * |---------|-----------|------------------------------|
   * | 0.05    | 9.0e-17   | 6.5e-16                      |
   * | 1e-3    | 3.1e-16   | 1.6e-15                      |
   * | 1e-5    | 2.7e-17   | 5.4e-15                      |
   *
   * The other form wins exactly one case, and it is a boundary rather
   * than a regime: at `alpha == 1` it returns `x` bit-exactly while the
   * incremental form does not (measured inexact for 9.6% of random
   * `(state, x)` pairs, because `state + 1*(x - state)` rounds twice).
   * That case is real — `det_ema_alpha` returns exactly 1.0 for "no gain
   * requested, so no averaging" — so it is handled explicitly below
   * rather than paid for at every alpha.
   *
   * @param state  Current EMA state.
   * @param x      New observation.
   * @param alpha  Coefficient in `[0, 1]`.  `1` is pass-through (no
   *               averaging) and is exact; `0` freezes the state and is
   *               exact.  A value above 1 saturates to pass-through
   *               rather than overshooting.
   * @return The updated state.
   *
   * @note NOT total in `x`: a non-finite observation poisons the state
   *       permanently, because an EMA remembers.  That is deliberate —
   *       the guard belongs at the boundary where an untrusted value
   *       first becomes persistent state, which is this function's input.
   *       Use ::saturate there, as `agc_steps` does.  See `agc_core.h`
   *       for what one unguarded non-finite sample cost.
   * @code
   * >>> from doppler.util import ema_step
   * >>> ema_step(0.0, 1.0, 0.5)          # halfway to the observation
   * 0.5
   * >>> ema_step(2.0, 2.0, 0.25)         # at its fixed point, no motion
   * 2.0
   * >>> ema_step(1.0, 7.0, 1.0)          # alpha 1 is exact pass-through
   * 7.0
   * >>> ema_step(1.0, 7.0, 0.0)          # alpha 0 freezes the state
   * 1.0
   * @endcode
   */
  JM_FORCEINLINE double
  ema_step (double state, double x, double alpha)
  {
    /* Loop-invariant, and folded away entirely when alpha is a
       compile-time constant, so the common path pays nothing. */
    if (alpha >= 1.0)
      return x;
    return state + alpha * (x - state);
  }

  /**
   * @brief The EMA coefficient that advances `d` samples in one step:
   * `1 - (1 - alpha)^d`.
   *
   * A decimated loop updates its average once per chunk of `d` samples
   * and must not thereby change its own time constant.  Compounding the
   * pole exactly is what makes `decim` a performance knob instead of a
   * retune.
   *
   * ### Why `expm1`/`log1p` rather than the direct expression
   *
   * `1.0 - pow(1.0 - alpha, d)` cancels catastrophically for small
   * `alpha`, and the damage is worst exactly where a narrow-band
   * estimator lives.  Measured at `d == 1`, where the answer must be
   * `alpha` itself:
   *
   * | `alpha` | direct `1-(1-alpha)^1` | this function |
   * |---------|------------------------|---------------|
   * | 0.05    | 6 ulps off             | exact         |
   * | 1e-5    | 26865 ulps off         | exact         |
   *
   * `agc_steps` used the repeated-multiply form and had this defect; it
   * now forms BOTH its per-chunk coefficients with this function.  Being
   * exact at `d == 1` is the property that lets a caller set `decim = 1`
   * and get bit-for-bit the undecimated recursion, so the decimated and
   * per-sample paths can be compared at all.
   *
   * @param alpha  Per-sample coefficient in `[0, 1]`.
   * @param d      Chunk length in samples, `>= 1`.
   * @return The per-chunk coefficient, in `[0, 1]`.
   * @code
   * >>> from doppler.util import ema_alpha_decim
   * >>> ema_alpha_decim(0.05, 1)         # d == 1 returns alpha exactly
   * 0.05
   * >>> round(ema_alpha_decim(0.05, 8), 12)
   * 0.336579568711
   * >>> ema_alpha_decim(1.0, 4)          # pass-through stays pass-through
   * 1.0
   * >>> ema_alpha_decim(0.0, 8)          # frozen stays frozen
   * 0.0
   * @endcode
   */
  JM_FORCEINLINE double
  ema_alpha_decim (double alpha, size_t d)
  {
    if (d <= 1)
      return alpha; /* exact by construction, not by luck */
    if (alpha <= 0.0)
      return 0.0;
    if (alpha >= 1.0)
      return 1.0; /* log1p(-1) is -inf; answer it directly */
    return -expm1 ((double)d * log1p (-alpha));
  }
#ifdef __cplusplus
}
#endif

#endif /* UTIL_CORE_H */
