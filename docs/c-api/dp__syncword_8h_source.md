

# File dp\_syncword.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_syncword.h**](dp__syncword_8h.md)

[Go to the documentation of this file](dp__syncword_8h.md)


```C++

#ifndef DP_SYNCWORD_H
#define DP_SYNCWORD_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  size_t   offset;   
  int      inverted; 
  unsigned errors;   
} dp_syncword_hit_t;

static inline int
dp_syncword_find (const uint8_t *bits, size_t n_bits, const uint8_t *marker,
                  size_t n_marker, unsigned max_errors,
                  dp_syncword_hit_t *hit)
{
  if (n_marker == 0u || n_bits < n_marker)
    return 0;

  const size_t last = n_bits - n_marker;
  for (size_t off = 0; off <= last; off++)
    {
      /* Both polarities in one pass: a bit that disagrees with the marker
         agrees with its complement, so the two distances sum to the marker
         length and one comparison yields both. */
      unsigned d = 0;
      for (size_t i = 0; i < n_marker; i++)
        d += (unsigned)((bits[off + i] & 1u) ^ (marker[i] & 1u));

      const unsigned dinv = (unsigned)n_marker - d;
      if (d <= max_errors || dinv <= max_errors)
        {
          const int inv = dinv < d;
          hit->offset   = off;
          hit->inverted = inv;
          hit->errors   = inv ? dinv : d;
          return 1;
        }
    }
  return 0;
}

static inline double
dp_syncword_pfa (size_t n_marker, unsigned max_errors)
{
  if (n_marker == 0u)
    return 0.0;

  const double n     = (double)n_marker;
  const double lgn1  = lgamma (n + 1.0);
  const double lg2   = n * log (2.0);
  const size_t t_max = (size_t)max_errors < n_marker ? (size_t)max_errors
                                                     : n_marker;

  double s = 0.0;
  for (size_t i = 0; i <= t_max; i++)
    s += exp (lgn1 - lgamma ((double)i + 1.0)
              - lgamma (n - (double)i + 1.0) - lg2);

  const double p = 2.0 * s;
  return p > 1.0 ? 1.0 : p;
}

static inline int
dp_syncword_max_errors (size_t n_marker, size_t window_bits, double pfa)
{
  int best = -1;
  for (size_t t = 0; t <= n_marker; t++)
    {
      const double p = dp_syncword_pfa (n_marker, (unsigned)t);
      /* -expm1(W log1p(-p)) is 1 - (1-p)^W without cancelling to zero at
         the small p that a usable threshold actually produces. */
      const double win = -expm1 ((double)window_bits * log1p (-p));
      if (win > pfa)
        break;
      best = (int)t;
    }
  return best;
}

#ifdef __cplusplus
}
#endif

#endif /* DP_SYNCWORD_H */
```


