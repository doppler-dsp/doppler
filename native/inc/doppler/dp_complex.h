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
 * language half is free. The library half is not: MSVC's `<complex.h>`
 * declares every name struct-shaped. So this header supplies the surface
 * from clang builtins plus REAL-valued libm, under doppler-owned names, and
 * points the C99 names at them by macro.
 *
 * It does include MSVC's `<complex.h>` -- FIRST, and only so that nothing
 * else includes it later. numpy's `npy_common.h` includes it on every MSVC
 * build, so every Python extension pulled it in AFTER this header: its
 * `cabsf(_Fcomplex)` then collided with the definition here, and its
 * `I` -- a `_Fcomplex` struct -- replaced this header's, so `x * I` in any
 * inline header function stopped compiling (doppler#1457, the first Windows
 * `make pyext`). Included first, its declarations are made before any name
 * below is a macro, and its include guard makes every later include a no-op.
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

/** @brief The imaginary unit, as C99 spells it. */
#define I (__extension__ 1.0fi)
/* `_Complex_I` is reserved to the implementation, and on this path that is
   exactly what this header is standing in for: it supplies the C99 surface
   the platform's <complex.h> does not, so defining the name C99 requires that
   header to define is the correct thing rather than an intrusion. Two benchmarks spell it. */
/* NOLINTNEXTLINE(bugprone-reserved-identifier,cert-dcl37-c) */
#define _Complex_I I

/** @brief |z|, via the real-valued hypotf rather than the CRT's complex face. */
static inline float
dp_cabsf (float _Complex z)
{
  return hypotf (__builtin_crealf (z), __builtin_cimagf (z));
}

/** @brief arg(z), via the real-valued atan2f. */
static inline float
dp_cargf (float _Complex z)
{
  return atan2f (__builtin_cimagf (z), __builtin_crealf (z));
}

/** @brief exp(z), from the real exponential and a sin/cos pair. */
static inline float _Complex
dp_cexpf (float _Complex z)
{
  float e = expf (__builtin_crealf (z));
  float im = __builtin_cimagf (z);
  return __builtin_complex (e * cosf (im), e * sinf (im));
}

/** @brief |z| in double precision. */
static inline double
dp_cabs (double _Complex z)
{
  return hypot (__builtin_creal (z), __builtin_cimag (z));
}

/** @brief arg(z) in double precision. */
static inline double
dp_carg (double _Complex z)
{
  return atan2 (__builtin_cimag (z), __builtin_creal (z));
}

/** @brief exp(z) in double precision. */
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
