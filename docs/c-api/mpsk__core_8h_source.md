

# File mpsk\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**mpsk**](dir_ca9d413705226c109a44c5982d79aa0f.md) **>** [**mpsk\_core.h**](mpsk__core_8h.md)

[Go to the documentation of this file](mpsk__core_8h.md)


```C++

#ifndef MPSK_CORE_H
#define MPSK_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include <complex.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifndef MPSK_PI
#define MPSK_PI 3.14159265358979323846
#endif

JM_FORCEINLINE int
mpsk_bps (int m)
{
  return (m == 2) ? 1 : (m == 4) ? 2 : (m == 8) ? 3 : 0;
}

JM_FORCEINLINE double
mpsk_phi0 (int m)
{
  return (m == 4) ? (MPSK_PI / 4.0) : 0.0;
}

JM_FORCEINLINE unsigned
mpsk_gray_encode (unsigned k)
{
  return k ^ (k >> 1);
}

JM_FORCEINLINE unsigned
mpsk_gray_decode (unsigned g)
{
  unsigned k = g;
  while (g >>= 1)
    k ^= g;
  return k;
}

JM_FORCEINLINE float complex
mpsk_constellation (unsigned g, int m)
{
  unsigned k    = mpsk_gray_decode (g & (unsigned)(m - 1));
  double   theta = 2.0 * MPSK_PI * (double)k / (double)m + mpsk_phi0 (m);
  return (float)cos (theta) + (float)sin (theta) * I;
}

JM_FORCEINLINE unsigned
mpsk_slice (float complex y, int m, float complex *ahat)
{
  double phi0 = mpsk_phi0 (m);
  double th   = atan2 ((double)cimagf (y), (double)crealf (y)) - phi0;
  long   k    = lround (th * (double)m / (2.0 * MPSK_PI));
  unsigned ki = (unsigned)(k & (long)(m - 1)); /* mod M (M power of two) */
  double   ta = 2.0 * MPSK_PI * (double)ki / (double)m + phi0;
  *ahat       = (float)cos (ta) + (float)sin (ta) * I;
  return mpsk_gray_encode (ki);
}

void mpsk_map(const uint8_t *sym, size_t sym_len, float complex *out, int m);

void mpsk_demap(const float complex *x, size_t x_len, uint8_t *out, int m);

void mpsk_diff_map(const uint8_t *sym, size_t sym_len, float complex *out,
                   int m);

void mpsk_diff_demap(const float complex *x, size_t x_len, uint8_t *out, int m);

int mpsk_bits_per_symbol(int m);

#ifdef __cplusplus
}
#endif

#endif /* MPSK_CORE_H */
```


