

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
#include <complex.h> /* first, so it is never included after: see above */

/* The UCRT's <math.h> hijacks the identifier `complex`.
 *
 * `corecrt_math.h` carries `#define complex _complex` -- a legacy alias for
 * its `struct _complex`, the argument type of the old `_cabs`. C99 spells a
 * complex type `float complex`, which doppler writes in 196 places, and under
 * that macro each one becomes `float _complex`: a struct, not a complex
 * number, reported as `error: redefinition of '_complex'` a line later.
 *
 * On POSIX `<complex.h>` is what defines `complex` as `_Complex`, and the
 * UCRT's does not -- so the C99 meaning has to be restored explicitly, after
 * <math.h> has had its say. Found by the Windows runner;
 * no amount of Linux CI can see it, because no POSIX libc does this. */
#ifdef complex
#undef complex
#endif
#define complex _Complex

/* Not the UCRT's struct-typed functions: see the file comment. These are
   clang builtins, so they lower to register moves rather than calls. */
#undef crealf
#undef cimagf
#undef conjf
#undef creal
#undef cimag
#undef conj
#define crealf __builtin_crealf
#define cimagf __builtin_cimagf
#define conjf __builtin_conjf
#define creal __builtin_creal
#define cimag __builtin_cimag
#define conj __builtin_conj

/* The UCRT's <complex.h> defines both as a `_Fcomplex` struct built by
   `_FCbuild`, which no arithmetic operator accepts. */
#undef I
#undef _Complex_I

#define I (__extension__ 1.0fi)
/* `_Complex_I` is reserved to the implementation, and on this path that is
   exactly what this header is standing in for: it supplies the C99 surface
   the platform's <complex.h> does not, so defining the name C99 requires that
   header to define is the correct thing rather than an intrusion. Two benchmarks spell it. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c) */
#define _Complex_I I

static inline float
dp_cabsf (float _Complex z)
{
  return hypotf (__builtin_crealf (z), __builtin_cimagf (z));
}

static inline float
dp_cargf (float _Complex z)
{
  return atan2f (__builtin_cimagf (z), __builtin_crealf (z));
}

static inline float _Complex
dp_cexpf (float _Complex z)
{
  float e = expf (__builtin_crealf (z));
  float im = __builtin_cimagf (z);
  return __builtin_complex (e * cosf (im), e * sinf (im));
}

static inline double
dp_cabs (double _Complex z)
{
  return hypot (__builtin_creal (z), __builtin_cimag (z));
}

static inline double
dp_carg (double _Complex z)
{
  return atan2 (__builtin_cimag (z), __builtin_creal (z));
}

static inline double _Complex
dp_cexp (double _Complex z)
{
  double e = exp (__builtin_creal (z));
  double im = __builtin_cimag (z);
  return __builtin_complex (e * cos (im), e * sin (im));
}

/* The C99 names, pointed at the definitions above. A macro rather than the
   name itself because the UCRT has already declared that name, taking a
   struct, and C has no overloading. */
#define cabsf dp_cabsf
#define cargf dp_cargf
#define cexpf dp_cexpf
#define cabs dp_cabs
#define carg dp_carg
#define cexp dp_cexp

#else /* POSIX: the platform header, unchanged. */

#include <complex.h>

#endif /* _WIN32 */

#endif /* DP_COMPLEX_H */
```


