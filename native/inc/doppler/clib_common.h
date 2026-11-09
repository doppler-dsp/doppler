/**
 * @file clib_common.h
 * @brief Common C99 types and performance macros for doppler.
 *
 * The header every component includes first: the C99 standard headers
 * and the complex-math surface (dp_complex.h), the doppler-wide DP_OK /
 * DP_ERR_* return codes, DP_NODISCARD, the jm performance annotations
 * (jm_perf.h), the abort-on-OOM allocators for trusted internal
 * allocations, and a few inline numeric helpers shared library-wide
 * (FFT bin frequencies, a sign-safe fmod, a reentrant lgamma).
 */
#ifndef DOPPLER_CLIB_COMMON_H
#define DOPPLER_CLIB_COMMON_H

#include "doppler/dp_complex.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** CMPLXF is C11 but missing on MinGW/ucrt64.  __builtin_complex is
 * the GCC-portable way to construct a complex value without relying
 * on _Complex_I (which can propagate NaN in some implementations).
 * Defined only where the C library does not already provide it.
 *
 * @param r  Real part, converted to float.
 * @param i  Imaginary part, converted to float. */
#ifndef CMPLXF
#  define CMPLXF(r, i) __builtin_complex ((float)(r), (float)(i))
#endif

/* ------------------------------------------------------------------ */
/* Return-code convention                                              */
/*                                                                     */
/* int-returning functions use these codes.  0 is always success.     */
/* size_t-returning functions return a sample/byte count; they        */
/* operate on already-created objects and cannot fail.                */
/* Pointer-returning functions return NULL on failure.                */
/*                                                                     */
/* This is the single, doppler-wide error vocabulary.  The streaming   */
/* API (stream/stream.h) includes this header for the same codes — one */
/* scheme everywhere, so a value never means two things in one TU.     */
/* Not every code is meaningful to every subsystem (the core DSP path  */
/* only ever returns DP_OK / DP_ERR_MEMORY / DP_ERR_INVALID).          */
/* ------------------------------------------------------------------ */
#define DP_OK 0             /**< Success. */
#define DP_ERR_INIT (-1)    /**< Initialisation failed (context/socket). */
#define DP_ERR_SEND (-2)    /**< Send failed. */
#define DP_ERR_RECV (-3)    /**< Receive failed or timed out (EAGAIN). */
#define DP_ERR_INVALID (-4) /**< Invalid argument. */
#define DP_ERR_TIMEOUT (-5) /**< Operation timed out. */
#define DP_ERR_MEMORY (-6)  /**< Memory allocation failure. */
#define DP_ERR_TOO_LARGE (-7) /**< Frame exceeds transport max payload. */
#define DP_ERR_INTERRUPTED                                                    \
  (-8) /**< A blocking call returned because dp_stream_interrupt() was     \
            called -- a request to stop, not a failure. */
#define DP_ERR_CLOSED                                                         \
  (-9) /**< The context is draining or closed: it accepts no more sends,   \
            and a message it handed out can no longer be acked. A state,   \
            not a transport failure. */
#define DP_ERR_EOF                                                            \
  (-10) /**< The producer has finished: no more data is coming, ever. A     \
             state, not a failure, and distinct from DP_ERR_TIMEOUT, which  \
             means "not yet". Every transport spells it the same way -- see \
             docs/design/io-termination.md. */

/**
 * @brief Marks a function whose status must not be dropped.
 *
 * A refusal nobody reads is invisible: the call returns DP_ERR_*, the
 * caller carries on as if it had worked, and no flag can see it, because
 * -Wunused-result only checks a function that asks for it (#1663). Put
 * this in front of the return type of any function that returns a
 * refusal, and gcc and clang warn at every call site that discards it; the
 * library's -Werror builds fail there. A `(void)` cast does NOT silence
 * gcc: handle the status. Empty where the compiler has no such attribute.
 */
#if defined(__GNUC__) || defined(__clang__)
#define DP_NODISCARD __attribute__ ((warn_unused_result))
#else
#define DP_NODISCARD
#endif

#include "doppler/jm_perf.h"

/* ------------------------------------------------------------------ */
/* Trusted allocation                                                  */
/*                                                                     */
/* A small, fixed-size, internally-sized allocation with already-      */
/* validated arguments cannot fail in practice; the only way malloc /  */
/* a sub-object create() returns NULL is genuine OOM, an unrecoverable  */
/* condition for a compute kernel. Rather than thread a per-call unwind */
/* path no test can reach (and that inflates every create() with        */
/* uncoverable cleanup), route such allocations through these helpers:  */
/* they abort() immediately on the impossible failure. This is the      */
/* doppler-wide convention for trusted internal allocations — see the   */
/* "trust internal guarantees" rule. (Reachable failures — invalid      */
/* user arguments — still return NULL / DP_ERR_INVALID as before; only  */
/* the OOM path aborts.)                                                 */
/* ------------------------------------------------------------------ */

/** Assert a just-constructed object / allocation is non-NULL, aborting with
 *  a diagnostic on the impossible OOM. The single check point: a sub-object
 *  create() returns NULL only on OOM once its arguments are validated, so
 *  wrap the call — `x = dp_xnn (foo_create (...))` — instead of
 *  checking-and-unwinding at every call site. (Classic GNU `xmalloc`.) */
static inline void *
dp_xnn (void *p)
{
  if (!p)
    abort ();
  return p;
}

/** malloc that aborts on OOM (for a trusted internal allocation). */
static inline void *
dp_xmalloc (size_t n)
{
  return dp_xnn (malloc (n));
}

/** calloc that aborts on OOM (zero-initialised trusted allocation). */
static inline void *
dp_xcalloc (size_t nmemb, size_t size)
{
  return dp_xnn (calloc (nmemb, size));
}

/** realloc that aborts on OOM — the third member of the family.
 *
 *  A grow-on-demand scratch buffer is the shape that wants it: the new size
 *  scales with the caller's block, but the only way the call fails is still
 *  genuine exhaustion, so the unwind path is as uncoverable as malloc's.
 *  Passing NULL for @p p is a fresh allocation, exactly as realloc defines.
 */
static inline void *
dp_xrealloc (void *p, size_t n)
{
  return dp_xnn (realloc (p, n));
}

/**
 * @brief `a * b` as a `size_t`, or a refusal when the product does not fit.
 *
 * A size product that wraps is the worst way an allocation can go wrong:
 * the buffer comes back SMALL, every check against it passes, and the
 * writes that follow run past its end. The guard is one line, and that is
 * the trouble with it -- written by hand at each site it was spelled a
 * different way every time (`n > SIZE_MAX / k`, `SIZE_MAX / a / b`, a
 * strict `<` against the quotient), and one file grew three wrapping
 * products in two review rounds. Spelled once here so every product that
 * sizes memory is checked the same way; `scripts/check_size_mul_sites.py`
 * fails on a `SIZE_MAX /` written anywhere else in library C.
 *
 * Divide before multiplying, so nothing overflows on the way to the
 * answer: `a * b` fits exactly when `b == 0` or `a <= SIZE_MAX / b`. It is
 * plain C99 on purpose, not `__builtin_mul_overflow`: clang-cl on Windows
 * builds this header too.
 *
 * The contract a caller can lean on:
 *
 * - it returns 1 and writes `a * b` to @p out when the product fits,
 *   including a zero factor (the product is then 0);
 * - it returns 0 and leaves @p out UNTOUCHED when it overflows, so a
 *   refusal never hands back a wrapped count;
 * - @p out may alias the storage a factor was read from:
 *   `dp_size_mul (n, k, &n)` is safe, because both factors are taken by
 *   value before anything is written. Chain a three-way product that way.
 *
 * @code
 * #include "doppler/clib_common.h"
 * #include <stdint.h>
 * #include <stdio.h>
 *
 * int
 * main (void)
 * {
 *   size_t bytes = 0;
 *   if (!dp_size_mul (1024, sizeof (float), &bytes))
 *     return 1;
 *   printf ("%zu\n", bytes); // 4096
 *
 *   size_t n = 7;
 *   if (!dp_size_mul (n, 3, &n) || !dp_size_mul (n, 2, &n))
 *     return 1;
 *   printf ("%zu\n", n); // 42
 *
 *   size_t kept = 5;
 *   if (dp_size_mul (SIZE_MAX / 2 + 1, 2, &kept))
 *     return 1;
 *   printf ("%zu\n", kept); // 5: a refusal leaves it alone
 *   return 0;
 * }
 * @endcode
 *
 * @param a    First factor.
 * @param b    Second factor.
 * @param out  Receives `a * b` when it fits; untouched otherwise.
 * @return 1 when the product fits in a `size_t`, 0 when it would wrap.
 */
static inline int
dp_size_mul (size_t a, size_t b, size_t *out)
{
  if (b != 0 && a > SIZE_MAX / b)
    return 0;
  *out = a * b;
  return 1;
}

/**
 * @brief `a * b`, or `SIZE_MAX` when the product does not fit.
 *
 * The saturating face of dp_size_mul(), for a function whose job is an
 * UPPER BOUND -- the most output a call can produce, the records a ring
 * must hold. A wrapped bound sizes a buffer small, which is the one
 * failure such a function exists to prevent; `SIZE_MAX` instead is a size
 * no allocation satisfies, so a caller that trusts it is refused at the
 * allocation rather than overrun after it. Built on dp_size_mul() so the
 * overflow test has one spelling.
 *
 * @code
 * #include "doppler/clib_common.h"
 * #include <stdint.h>
 * #include <stdio.h>
 *
 * int
 * main (void)
 * {
 *   printf ("%zu\n", dp_size_mul_sat (6, 7));                   // 42
 *   printf ("%d\n", dp_size_mul_sat (SIZE_MAX, 2) == SIZE_MAX); // 1
 *   return 0;
 * }
 * @endcode
 *
 * @param a  First factor.
 * @param b  Second factor.
 * @return `a * b` when it fits in a `size_t`, else `SIZE_MAX`.
 */
static inline size_t
dp_size_mul_sat (size_t a, size_t b)
{
  size_t p = SIZE_MAX;
  (void)dp_size_mul (a, b, &p);
  return p;
}

/**
 * @brief @p d as a `size_t`, truncated, or a refusal when it does not fit.
 *
 * The "does this double fit a size_t" check, which every site wrote as
 * `d <= (double)SIZE_MAX` -- and that is wrong where size_t is 64 bits:
 * SIZE_MAX is 2^64 - 1, which a double cannot hold, so the cast rounds UP
 * to 2^64, the bound admits 2^64 itself, and the conversion of 2^64 to
 * `size_t` is undefined behaviour (in practice 0 or SIZE_MAX, by target).
 * The right bound is a strict `<` against 2^w, w the width of size_t: a
 * power of two, so exactly representable whatever w is. It is formed as
 * `(SIZE_MAX / 2 + 1) * 2.0`: SIZE_MAX / 2 + 1 is 2^(w-1), exact as a
 * double, and doubling it is exact. Where size_t is 32 bits this is the
 * same as `<= SIZE_MAX` for a whole number, and every double below 2^32
 * truncates into range. `scripts/check_size_mul_sites.py` fails on a
 * `(double)SIZE_MAX` written anywhere else in library C.
 *
 * The contract:
 *
 * - it returns 1 and writes `(size_t)d`, truncated toward zero, when
 *   `0 <= d < 2^w`. -0.0 compares equal to 0 and converts to 0;
 * - it returns 0 and leaves @p out UNTOUCHED for a negative @p d, for
 *   2^w and above, for +inf and for NaN (every comparison with NaN is
 *   false, and doppler builds with -fno-finite-math-only so that holds).
 *
 * Whether @p d must also be whole is the caller's rule, not this one's:
 * test `d == floor (d)` beside it.
 *
 * @code
 * #include "doppler/clib_common.h"
 * #include <stdio.h>
 *
 * int
 * main (void)
 * {
 *   size_t n = 7;
 *   if (!dp_size_from_double (4096.75, &n))
 *     return 1;
 *   printf ("%zu\n", n); // 4096
 *
 *   if (dp_size_from_double (-1.0, &n) || dp_size_from_double (NAN, &n))
 *     return 1;
 *   printf ("%zu\n", n); // 4096: a refusal leaves it alone
 *   return 0;
 * }
 * @endcode
 *
 * @param d    The value to convert.
 * @param out  Receives `(size_t)d` when it fits; untouched otherwise.
 * @return 1 when `0 <= d < 2^w`, else 0.
 */
static inline int
dp_size_from_double (double d, size_t *out)
{
  /* 2^w, exactly: see the comment above. */
  const double two_w = (double)(SIZE_MAX / 2 + 1) * 2.0;
  if (!(d >= 0.0 && d < two_w))
    return 0;
  *out = (size_t)d;
  return 1;
}

/**
 * @brief Whether @p x is a positive, finite number.
 *
 * The domain of a scale or gain that a constructor stores and later
 * divides by or multiplies with: zero, a negative, NaN and either infinity
 * are all refused. Spelled once here so every converter's create() refuses
 * the same set; `x <= 0` alone lets NaN through (every comparison with NaN
 * is false) and lets +inf through, which then maps every input to a full
 * scale or to zero.
 *
 * @param x  The value to test.
 * @return 1 when `0 < x < inf`, else 0.
 */
static inline int
dp_positive_finite (double x)
{
  return isfinite (x) && x > 0.0;
}

/**
 * @brief `numpy.fft.fftfreq(n)[bin] * n` — the SIGNED index of an FFT bin.
 *
 * `0 = DC`, ascending positive to `(n-1)/2`, then wrapping negative, so an
 * even-length grid puts its Nyquist bin at `-n/2`. Multiply by the grid's
 * bin spacing for Hz, or use dp_fftfreq() for the normalised frequency.
 *
 * Named for what it is. It arrived as an acquisition-specific helper called
 * `dp_fftfreq_index`, which is how it came to disagree with numpy at
 * exactly one index: it reported `+n/2` at the Nyquist bin. That is not
 * wrong on its own -- `+n/2` and `-n/2` are the same frequency, and a
 * search on this grid cannot tell them apart -- but every formula ported in
 * from numpy then disagreed with the engine at the one bin the engine was
 * most careful about. Following the universal convention deletes that class
 * of surprise rather than documenting it.
 *
 * What must not vary is the READER: a consumer seeded on one side of the
 * fold while the search meant the other is off by the full span. That
 * happened here once -- an acquisition's wideband search and its hand-off
 * spelled the fold differently -- and it surfaced as a receiver reporting
 * `tracking == 1` while decoding noise. So this lives in the COMMON header,
 * inline, and `doppler.acquire.bin_to_signed` is a thin wrapper over it, so C
 * and Python call the same code instead of restating the arithmetic.
 *
 * @param bin  Bin index in `[0, n)`.
 * @param n    Grid size.
 * @return Signed index in `[-(n/2), +((n-1)/2)]`.
 */
static inline long
dp_fftfreq_index (size_t bin, size_t n)
{
  return (bin <= (n - 1) / 2) ? (long)bin : (long)bin - (long)n;
}

/**
 * @brief The frequency of an FFT bin, in the units of @p fs.
 *
 * `dp_fftfreq_index(bin, n) * fs / n` — numpy's `fftfreq(n, d)[bin]` with
 * the sample RATE where numpy takes the sample SPACING. That is the one
 * deliberate difference from the numpy signature, and it is the right way
 * round for this library: every caller here has `fs` in hand and would
 * otherwise write `1.0 / fs` at the call site, which is a reciprocal to get
 * wrong for no benefit. Pass `fs = 1.0` for normalised cycles/sample, which
 * is numpy's default.
 *
 * @param bin  Bin index in `[0, n)`.
 * @param n    Grid size (> 0).
 * @param fs   Sample rate; the result is in these units.
 * @return Bin frequency in `[-fs/2, +fs/2)`.
 */
static inline double
dp_fftfreq (size_t bin, size_t n, double fs)
{
  return (n == 0) ? 0.0 : (double)dp_fftfreq_index (bin, n) * fs / (double)n;
}

/**
 * @brief @p x folded into `[0, m)` for either sign of @p x.
 *
 * C's `fmod()` keeps the dividend's sign, so a negative @p x comes back in
 * `(-m, 0]` and every caller that wants a phase, a frequency or an index on
 * a periodic axis has to add @p m back. That fix-up had been written by hand
 * five times in this library -- a code phase in chips, a harmonic into the
 * analysed band, a table index, the DLL's replica tap and a hand-over phase
 * -- before this became its one home (doppler#1249); the gate
 * `scripts/check_fmod_fold_sites.py` fails on the sixth.
 *
 * External inline (JM_FORCEINLINE), not `static inline` like its
 * neighbours: the DLL's replica tap is an external inline in a header, and
 * C99 6.7.4 forbids such a function from referencing one with internal
 * linkage -- the compiler said so.
 *
 * A negative remainder within half an ulp of zero rounds `r + m` up to
 * exactly @p m, outside the range; it folds to 0, its equal on the circle.
 * The DLL's replica tap indexes the code with this result, and read
 * `code[sf]`, one past the copy, at a spacing just off a phase-grid point
 * (doppler#2110).
 *
 * The test is `y >= m`, not `y < m`: doppler builds with -ffast-math,
 * and gcc 14 at -O1 and above rewrote `r + m < m` as `r < 0`, always true
 * here, so for a constant period that spelling returned @p m at run time.
 * Written this way a NaN @p x also stays NaN rather than folding to a
 * plausible phase 0. test_dll_core pins both with run-time inputs, over a
 * constant and a run-time period.
 *
 * @param x  Any real value.
 * @param m  The period (> 0).
 * @return @p x modulo @p m, in `[0, m)`; NaN for a NaN @p x.
 */
JM_FORCEINLINE double
dp_fmod_pos (double x, double m)
{
  double r = fmod (x, m);
  if (r >= 0.0)
    return r;
  double y = r + m;
  return y >= m ? 0.0 : y;
}

/**
 * @brief log|Gamma(x)|, reentrant -- the one spelling of `lgamma` in this
 *        library.
 *
 * C's `lgamma()` writes the sign of Gamma into the global `signgam`, so
 * two threads calling it race on a variable neither of them reads. That
 * is exactly what happens when a pool of receivers rebuilds their tracking
 * chains on different threads at once: every chain sizes a detector
 * threshold through `dp_marcum_q()`, and ThreadSanitizer stopped the first
 * multi-threaded test on it (doppler#1260). `lgamma_r()` takes the sign by
 * pointer and touches no global. Declared here and defined in
 * `native/src/detection/marcum_q.c` (detection_core, which every threshold
 * links) rather than inline, because an installed header may not name
 * libm's `lgamma_r` -- the shipped libraries do not define it, and
 * `scripts/check_installed_headers.py` says so -- and a strict C99 dialect
 * does not declare it. `scripts/check_lgamma_sites.py` fails on a bare
 * `lgamma` in `native/inc` or `native/src`.
 *
 * @param x  Any real value.
 * @return `log|Gamma(x)|`, as `lgamma()` returns it.
 */
double dp_lgamma (double x);

#endif /* DOPPLER_CLIB_COMMON_H */
