/**
 * @file dp_complex.h
 * @brief The complex-math surface, routed so it survives Windows.
 *
 * Include this instead of `<complex.h>`. On Linux and macOS it IS
 * `<complex.h>` and nothing changes; on Windows it supplies the same names
 * without touching the platform header.
 *
 * Why the indirection
 * -------------------
 * MSVC has no `_Complex` keyword at all. Its `<complex.h>` is built around
 * `_Fcomplex`/`_Dcomplex` STRUCTS, with arithmetic in function form
 * (`_FCmulcc`) -- so `crealf` there is declared as taking an `_Fcomplex`,
 * while doppler passes a native `float _Complex`. The two cannot meet.
 *
 * clang-cl does implement `_Complex` while targeting the MSVC ABI, so the
 * language half is free. The library half is not: pulling in MSVC's
 * `<complex.h>` reintroduces the struct-shaped declarations. So this header
 * never includes it on Windows, and supplies the surface from clang builtins
 * plus REAL-valued libm instead.
 *
 * Measured on clang 22 targeting `x86_64-pc-windows-msvc` with doppler's own
 * flags (`-O3 -march=x86-64-v2 -ffast-math -fno-finite-math-only`): a complex
 * DSP kernel compiled through this header leaves exactly these undefined
 * symbols.
 *
 *     _fltused  atan2f  cosf  expf  hypotf  sinf
 *
 * All five are plain-float UCRT exports. No `_Fcomplex` crosses the ABI, and
 * `__mulsc3`/`__divsc3` inline away under `-ffast-math`, so there is no
 * compiler-rt dependency either. That is why `cabsf`/`cargf`/`cexpf` are
 * routed through `hypotf`/`atan2f`/`expf`+`cosf`/`sinf` rather than being
 * left to lower to the CRT's complex-typed entry points.
 *
 * @note On Linux and macOS this is a pure passthrough -- the same
 *       `<complex.h>` symbols, unchanged -- so numerics cannot drift between
 *       platforms by way of this header.
 */
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

/** @brief The imaginary unit, as C99 spells it. */
#define I (__extension__ 1.0fi)
#ifndef _Complex_I
#define _Complex_I I
#endif

/** @brief |z|, via the real-valued hypotf rather than the CRT's complex face. */
static inline float
cabsf (float _Complex z)
{
  return hypotf (__builtin_crealf (z), __builtin_cimagf (z));
}

/** @brief arg(z), via the real-valued atan2f. */
static inline float
cargf (float _Complex z)
{
  return atan2f (__builtin_cimagf (z), __builtin_crealf (z));
}

/** @brief exp(z), from the real exponential and a sin/cos pair. */
static inline float _Complex
cexpf (float _Complex z)
{
  float e = expf (__builtin_crealf (z));
  float im = __builtin_cimagf (z);
  return __builtin_complex (e * cosf (im), e * sinf (im));
}

/** @brief |z| in double precision. */
static inline double
cabs (double _Complex z)
{
  return hypot (__builtin_creal (z), __builtin_cimag (z));
}

/** @brief arg(z) in double precision. */
static inline double
carg (double _Complex z)
{
  return atan2 (__builtin_cimag (z), __builtin_creal (z));
}

/** @brief exp(z) in double precision. */
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
