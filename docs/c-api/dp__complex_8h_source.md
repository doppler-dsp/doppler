

# File dp\_complex.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**dp\_complex.h**](dp__complex_8h.md)

[Go to the documentation of this file](dp__complex_8h.md)


```C++

#ifndef DP_COMPLEX_H
#define DP_COMPLEX_H

#if defined(_MSC_VER) && !defined(__clang__)
#error "doppler requires C99 _Complex; on Windows build with clang-cl, not cl.exe"
#endif

#ifdef _WIN32

#include <math.h>

/* Deliberately NOT <complex.h>: see the file comment. These are clang
   builtins, so they lower to register moves rather than calls. */
#define crealf __builtin_crealf
#define cimagf __builtin_cimagf
#define conjf __builtin_conjf
#define creal __builtin_creal
#define cimag __builtin_cimag
#define conj __builtin_conj

#define I (__extension__ 1.0fi)
#ifndef _Complex_I
#define _Complex_I I
#endif

static inline float
cabsf (float _Complex z)
{
  return hypotf (__builtin_crealf (z), __builtin_cimagf (z));
}

static inline float
cargf (float _Complex z)
{
  return atan2f (__builtin_cimagf (z), __builtin_crealf (z));
}

static inline float _Complex
cexpf (float _Complex z)
{
  float e = expf (__builtin_crealf (z));
  float im = __builtin_cimagf (z);
  return __builtin_complex (e * cosf (im), e * sinf (im));
}

static inline double
cabs (double _Complex z)
{
  return hypot (__builtin_creal (z), __builtin_cimag (z));
}

static inline double
carg (double _Complex z)
{
  return atan2 (__builtin_cimag (z), __builtin_creal (z));
}

static inline double _Complex
cexp (double _Complex z)
{
  double e = exp (__builtin_creal (z));
  double im = __builtin_cimag (z);
  return __builtin_complex (e * cos (im), e * sin (im));
}

#else /* POSIX: the platform header, unchanged. */

#include <complex.h>

#endif /* _WIN32 */

#endif /* DP_COMPLEX_H */
```


