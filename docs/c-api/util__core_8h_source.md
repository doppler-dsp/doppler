

# File util\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**util**](dir_7dd94ac9e5a2e34ed236c6361f93c476.md) **>** [**util\_core.h**](util__core_8h.md)

[Go to the documentation of this file](util__core_8h.md)


```C++

#ifndef DP_UTIL_CORE_H
#define DP_UTIL_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include <math.h>

#ifdef __cplusplus
extern "C"
{
#endif

  JM_FORCEINLINE float _Complex
  dp_square_clip (float _Complex y, float lin)
  {
    float r = fminf (fmaxf (crealf (y), -lin), lin);
    float i = fminf (fmaxf (cimagf (y), -lin), lin);
    return r + i * I;
  }

  JM_FORCEINLINE size_t
  dp_next_pow_two (size_t n)
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

  JM_FORCEINLINE double
  dp_saturate (double v, double lo, double hi, double nan_to)
  {
    if (v >= lo && v <= hi)
      return v; /* the common case; false for NaN, which falls through */
    if (v < lo)
      return lo;
    if (v > hi)
      return hi;
    return nan_to; /* nothing else can reach here */
  }

  JM_FORCEINLINE double
  dp_ema_step (double state, double x, double alpha)
  {
    /* Loop-invariant, and folded away entirely when alpha is a
       compile-time constant, so the common path pays nothing. */
    if (alpha >= 1.0)
      return x;
    return state + alpha * (x - state);
  }

  JM_FORCEINLINE double
  dp_complement_power (double p, double x)
  {
    if (x == 1.0)
      return p; /* exact by construction, not by luck */
    if (x == 0.0 || p <= 0.0)
      return 0.0;
    if (p >= 1.0)
      return 1.0; /* log1p(-1) is -inf; answer it directly */
    return -expm1 (x * log1p (-p));
  }

  JM_FORCEINLINE double
  dp_ema_alpha_decim (double alpha, size_t d)
  {
    if (d <= 1)
      return alpha; /* exact by construction, not by luck */
    return dp_complement_power (alpha, (double)d);
  }

  JM_FORCEINLINE double
  dp_sinc (double u)
  {
    return (u == 0.0) ? 1.0 : sin (M_PI * u) / (M_PI * u);
  }

  JM_FORCEINLINE int
  dp_simpson_weights (double *w, size_t w_len)
  {
    if (w_len < 3 || (w_len & 1u) == 0)
      return DP_ERR_INVALID;
    const double s = 3.0 * (double)(w_len - 1);
    for (size_t i = 0; i < w_len; i++)
      w[i] = (i == 0 || i + 1 == w_len ? 1.0 : (i & 1u) ? 4.0 : 2.0) / s;
    return DP_OK;
  }

  JM_FORCEINLINE double
  dp_mean_sinc (double umax)
  {
    if (umax <= 0.0)
      return 1.0;
    /* 64-interval Simpson over segments of at most half a bin, so the error
       is one bound at any umax rather than growing with it. */
    double w[65];
    (void)dp_simpson_weights (w, 65);
    const size_t segs = umax > 0.5 ? (size_t)ceil (2.0 * umax) : 1u;
    const double len  = umax / (double)segs;
    double       m    = 0.0;
    for (size_t s = 0; s < segs; s++)
      for (size_t i = 0; i < 65; i++)
        m += w[i] * dp_sinc (len * ((double)s + (double)i / 64.0));
    return m / (double)segs;
  }

  JM_FORCEINLINE void
  dp_midpoint_nodes (double *u, size_t u_len)
  {
    for (size_t k = 0; k < u_len; k++)
      u[k] = ((double)k + 0.5) / (double)u_len;
  }

  JM_FORCEINLINE int
  dp_gauss_hermite (double *z, size_t z_len, double *p, size_t p_len)
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
```


