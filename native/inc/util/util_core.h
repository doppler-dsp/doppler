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
  JM_FORCEINLINE float _Complex
  square_clip (float _Complex y, float lin)
  {
    float r = fminf (fmaxf (crealf (y), -lin), lin);
    float i = fminf (fmaxf (cimagf (y), -lin), lin);
    return r + i * I;
  }

  /**
   * @brief Smallest power of two greater than or equal to @p n.
   *
   * The transform-sizing primitive. A zero-padded FFT length, a ring
   * capacity, a grow-on-demand buffer -- all of them want the same
   * "round up to a power of two", and all of them had been writing the
   * doubling loop out where they stood. FIVE identical private copies
   * were in the tree when this landed -- `detector/det_private.h`,
   * `delay_core.c`, `psd_core.c`, `specan_core.c` and `ppe_core.c`, each
   * a `static` one none of the others could reach -- alongside bare
   * `while (c < n) c *= 2` loops seeded from whatever each caller
   * happened to start at, which is the shape that lets one quietly start
   * at 4 and another at 1 and neither be wrong until they are compared.
   * None of the four guarded the overflow below.
   *
   * Saturating rather than wrapping: a doubling loop run past the top of
   * `size_t` shifts to zero and spins forever, so the one case that
   * cannot be expressed returns 0 instead of hanging. A caller sizing an
   * allocation gets a refusal it can see.
   *
   * @param n  Value to round up. 0 and 1 both give 1.
   * @return The smallest power of two >= @p n, or 0 if that exceeds
   *         `SIZE_MAX`.
   * @code
   * >>> from doppler.util import next_pow_two
   * >>> next_pow_two(0), next_pow_two(1), next_pow_two(2)
   * (1, 1, 2)
   * >>> next_pow_two(3), next_pow_two(4), next_pow_two(5)
   * (4, 4, 8)
   * >>> next_pow_two(1000)        # a zero-padded transform length
   * 1024
   * >>> next_pow_two(1 << 20)     # already a power of two, unchanged
   * 1048576
   * @endcode
   */
  JM_FORCEINLINE size_t
  next_pow_two (size_t n)
  {
    size_t c = 1u;
    while (c < n)
      {
        if (c > ((size_t)-1) / 2u)
          return 0u;
        c <<= 1;
      }
    return c;
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
   * @brief `1 - (1 - p)^x`, accurate for small `p`: the probability that at
   * least one of `x` independent trials succeeds, each with probability `p`.
   *
   * Written directly, `1 - pow(1 - p, x)` loses everything `1 - p` rounded
   * away: at `p = 1e-5` it is 26865 ulps off. `-expm1(x * log1p(-p))` is
   * the same quantity with nothing cancelled.
   *
   * Two library quantities are this one expression, and both call it:
   * - the EMA coefficient that advances `d` samples in one step,
   *   ema_alpha_decim(alpha, d) (`x = d`);
   * - the per-cell false-alarm probability that splits a search's `pfa`
   *   over `n` independent cells, det_pfa_cell(pfa, n) (`x = 1/n`, Šidák).
   *
   * @param p  Per-trial probability, in `[0, 1]`.
   * @param x  Number of trials, any real `x >= 0`.
   * @return `1 - (1 - p)^x`; exactly `p` at `x == 1`, 0 at `x == 0` or
   *         `p <= 0`, and 1 at `p >= 1` (for `x > 0`).
   * @code
   * >>> from doppler.util import complement_power
   * >>> complement_power(0.05, 1.0)          # one trial is p exactly
   * 0.05
   * >>> round(complement_power(0.5, 2.0), 12)  # 1 - 0.25
   * 0.75
   * >>> round(complement_power(1e-3, 1 / 1000) * 1e6, 6)  # Sidak split
   * 1.0005
   * >>> complement_power(0.3, 0.0)
   * 0.0
   * @endcode
   */
  JM_FORCEINLINE double
  complement_power (double p, double x)
  {
    if (x == 1.0)
      return p; /* exact by construction, not by luck */
    if (x == 0.0 || p <= 0.0)
      return 0.0;
    if (p >= 1.0)
      return 1.0; /* log1p(-1) is -inf; answer it directly */
    return -expm1 (x * log1p (-p));
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
    return complement_power (alpha, (double)d);
  }

  /**
   * @brief Normalized sinc, `sin(pi u) / (pi u)`, with `sinc(0) = 1`.
   *
   * The amplitude response of a rectangular window, which makes it the
   * straddle loss of every correlator and DFT: a signal `u` bins off a
   * bin's centre keeps `sinc(u)` of its amplitude in that bin.
   *
   * @param u  Offset, in bins (any real).
   * @return `sin(pi u) / (pi u)`, and exactly 1 at `u == 0`.
   * @code
   * >>> from doppler.util import sinc
   * >>> sinc(0.0)
   * 1.0
   * >>> round(sinc(0.5), 12)                 # half a bin: 2/pi
   * 0.636619772368
   * >>> abs(sinc(1.0)) < 1e-15               # the first null
   * True
   * @endcode
   */
  JM_FORCEINLINE double
  sinc (double u)
  {
    return (u == 0.0) ? 1.0 : sin (M_PI * u) / (M_PI * u);
  }

  /**
   * @brief Fill @p w with composite Simpson weights for the MEAN of a
   * function over an interval.
   *
   * With `n = w_len` points, `sum(w[i] * f(a + i*(b - a)/(n - 1)))` is the
   * mean of `f` over `[a, b]` (multiply by `b - a` for the integral). The
   * weights are `1, 4, 2, 4, ..., 2, 4, 1` over `3 (n - 1)` and sum to 1.
   * Exact for any cubic; the error falls as `(n - 1)^-4` for a smooth `f`.
   *
   * @param w      Output, `w_len` weights.
   * @param w_len  Number of points: odd and at least 3.
   * @return DP_OK, or DP_ERR_INVALID (and @p w untouched) for any other
   *         length.
   * @code
   * >>> import numpy as np
   * >>> from doppler.util import simpson_weights
   * >>> w = np.empty(5)
   * >>> simpson_weights(w)
   * >>> w * 12                               # 1, 4, 2, 4, 1 over 12
   * array([1., 4., 2., 4., 1.])
   * >>> u = np.linspace(0.0, 1.0, 5)
   * >>> round(float(w @ u**3), 12)           # mean of u^3 over [0, 1]
   * 0.25
   * @endcode
   */
  JM_FORCEINLINE int
  simpson_weights (double *w, size_t w_len)
  {
    if (w_len < 3 || (w_len & 1u) == 0)
      return DP_ERR_INVALID;
    const double s = 3.0 * (double)(w_len - 1);
    for (size_t i = 0; i < w_len; i++)
      w[i] = (i == 0 || i + 1 == w_len ? 1.0 : (i & 1u) ? 4.0 : 2.0) / s;
    return DP_OK;
  }

  /**
   * @brief The mean of sinc(u) over `u` in `[0, umax]`.
   *
   * The average amplitude loss of a signal whose offset from the nearest
   * bin centre is uniform over `umax` bins: the scalloping a Pd model
   * averages over, where sinc(umax) would be only the worst case.
   * 64-interval Simpson (simpson_weights()) over segments of at most half a
   * bin: within 3e-10 at any umax, far below any model this feeds.
   *
   * @param umax  Upper end of the offset, in bins.
   * @return The mean; 1 for `umax <= 0`.
   * @code
   * >>> from doppler.util import mean_sinc
   * >>> mean_sinc(0.0)
   * 1.0
   * >>> round(mean_sinc(0.5), 9)             # uniform over half a bin
   * 0.8726543
   * @endcode
   */
  JM_FORCEINLINE double
  mean_sinc (double umax)
  {
    if (umax <= 0.0)
      return 1.0;
    /* 64-interval Simpson over segments of at most half a bin, so the error
       is one bound at any umax rather than growing with it. */
    double w[65];
    (void)simpson_weights (w, 65);
    const size_t segs = umax > 0.5 ? (size_t)ceil (2.0 * umax) : 1u;
    const double len  = umax / (double)segs;
    double       m    = 0.0;
    for (size_t s = 0; s < segs; s++)
      for (size_t i = 0; i < 65; i++)
        m += w[i] * sinc (len * ((double)s + (double)i / 64.0));
    return m / (double)segs;
  }

  /**
   * @brief Fill @p u with the midpoint-rule nodes on `[0, 1]`:
   * `u[k] = (k + 1/2) / n` for `n = u_len`.
   *
   * The points a uniform average over `n` equal cells is evaluated at, each
   * weighted `1/n`. Scale to `[a, b]` as `a + (b - a) * u[k]`.
   *
   * @param u      Output, `u_len` nodes, ascending.
   * @param u_len  Number of cells.
   * @code
   * >>> import numpy as np
   * >>> from doppler.util import midpoint_nodes
   * >>> u = np.empty(4)
   * >>> midpoint_nodes(u)
   * >>> u
   * array([0.125, 0.375, 0.625, 0.875])
   * @endcode
   */
  JM_FORCEINLINE void
  midpoint_nodes (double *u, size_t u_len)
  {
    for (size_t k = 0; k < u_len; k++)
      u[k] = ((double)k + 0.5) / (double)u_len;
  }

  /**
   * @brief Fill @p z and @p p with the n-point Gauss-Hermite rule for a
   * STANDARD NORMAL.
   *
   * `sum(p[i] * f(z[i]))` approximates `E[f(Z)]`, `Z ~ N(0, 1)`, and is
   * exact for any polynomial `f` of degree up to `2n - 1`. For
   * `X ~ N(mu, sigma^2)`, evaluate `f(mu + sigma * z[i])`. The nodes ascend
   * and are symmetric about 0; the weights sum to 1.
   *
   * The nodes are the roots of the probabilists' Hermite polynomial
   * `He_n`, found by Newton's method on its orthonormal recurrence
   * `h[k+1] = (z h[k] - sqrt(k) h[k-1]) / sqrt(k+1)`, which cannot
   * overflow the way `He_n` and `n!` do. Each starts from the classical
   * asymptotic guesses (Numerical Recipes' `gauher`). The weight of a root
   * is `1 / (n h[n-1](z)^2)`.
   *
   * @param z      Output, `n` nodes.
   * @param z_len  `n`, at least 1.
   * @param p      Output, `n` weights.
   * @param p_len  Must equal @p z_len.
   * @return DP_OK, or DP_ERR_INVALID (outputs untouched) for mismatched or
   *         zero lengths.
   * @code
   * >>> import numpy as np
   * >>> from doppler.util import gauss_hermite
   * >>> z, p = np.empty(2), np.empty(2)
   * >>> gauss_hermite(z, p)
   * >>> z, p                                 # +-1, each half
   * (array([-1.,  1.]), array([0.5, 0.5]))
   * >>> z, p = np.empty(5), np.empty(5)
   * >>> gauss_hermite(z, p)
   * >>> round(float(p @ z**4), 12)           # E[Z^4] = 3
   * 3.0
   * @endcode
   */
  JM_FORCEINLINE int
  gauss_hermite (double *z, size_t z_len, double *p, size_t p_len)
  {
    if (z_len == 0 || z_len != p_len)
      return DP_ERR_INVALID;
    const size_t n = z_len;
    double       x = 0.0; /* the guess, in physicists' units: z = x sqrt 2 */
    for (size_t i = 0; i < (n + 1) / 2; i++)
      {
        /* The i-th largest root. Guesses from the roots already found,
           which sit at z[n-1], z[n-2], ... */
        if (i == 0)
          x = sqrt (2.0 * (double)n + 1.0)
              - 1.85575 * pow (2.0 * (double)n + 1.0, -1.0 / 6.0);
        else if (i == 1)
          x -= 1.14 * pow ((double)n, 0.426) / x;
        else if (i == 2)
          x = 1.86 * x - 0.86 * z[n - 1] / M_SQRT2;
        else if (i == 3)
          x = 1.91 * x - 0.91 * z[n - 2] / M_SQRT2;
        else
          x = 2.0 * x - z[n - 1 - (i - 2)] / M_SQRT2;
        double r = M_SQRT2 * x, hm;
        for (int it = 0; it < 100; it++)
          {
            double h0 = 1.0, h1 = r; /* h[k-1], h[k] from k = 1 */
            for (size_t k = 1; k < n; k++)
              {
                const double h2
                    = (r * h1 - sqrt ((double)k) * h0) / sqrt ((double)k + 1.0);
                h0 = h1;
                h1 = h2;
              }
            /* h1 is h[n]; its derivative is sqrt(n) h[n-1] = sqrt(n) h0. */
            const double dz = h1 / (sqrt ((double)n) * h0);
            r -= dz;
            if (fabs (dz) <= 1e-15 * fmax (1.0, fabs (r)))
              break;
          }
        /* h[n-1] at the converged root, for its weight. */
        {
          double h0 = 1.0, h1 = r;
          for (size_t k = 1; k + 1 < n; k++)
            {
              const double h2
                  = (r * h1 - sqrt ((double)k) * h0) / sqrt ((double)k + 1.0);
              h0 = h1;
              h1 = h2;
            }
          hm = n == 1 ? 1.0 : h1;
        }
        x              = r / M_SQRT2;
        const double w = 1.0 / ((double)n * hm * hm);
        z[n - 1 - i]   = r;
        p[n - 1 - i]   = w;
        z[i]           = -r;
        p[i]           = w;
      }
    if (n & 1u)
      z[n / 2] = 0.0; /* the middle root, exactly */
    return DP_OK;
  }

#ifdef __cplusplus
}
#endif

#endif /* UTIL_CORE_H */
