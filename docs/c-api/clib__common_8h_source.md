

# File clib\_common.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the documentation of this file](clib__common_8h.md)


```C++

#ifndef DOPPLER_CLIB_COMMON_H
#define DOPPLER_CLIB_COMMON_H

#include "doppler/dp_complex.h"
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
#define DP_OK 0             
#define DP_ERR_INIT (-1)    
#define DP_ERR_SEND (-2)    
#define DP_ERR_RECV (-3)    
#define DP_ERR_INVALID (-4) 
#define DP_ERR_TIMEOUT (-5) 
#define DP_ERR_MEMORY (-6)  
#define DP_ERR_TOO_LARGE (-7) 
#define DP_ERR_INTERRUPTED                                                    \
  (-8) 
#define DP_ERR_CLOSED                                                         \
  (-9) 
#define DP_ERR_EOF                                                            \
  (-10) 
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

static inline void *
dp_xnn (void *p)
{
  if (!p)
    abort ();
  return p;
}

static inline void *
dp_xmalloc (size_t n)
{
  return dp_xnn (malloc (n));
}

static inline void *
dp_xcalloc (size_t nmemb, size_t size)
{
  return dp_xnn (calloc (nmemb, size));
}

static inline void *
dp_xrealloc (void *p, size_t n)
{
  return dp_xnn (realloc (p, n));
}

static inline int
dp_size_mul (size_t a, size_t b, size_t *out)
{
  if (b != 0 && a > SIZE_MAX / b)
    return 0;
  *out = a * b;
  return 1;
}

static inline int
dp_size_add (size_t a, size_t b, size_t *out)
{
  if (a > SIZE_MAX - b)
    return 0;
  *out = a + b;
  return 1;
}

static inline size_t
dp_size_mul_sat (size_t a, size_t b)
{
  size_t p = SIZE_MAX;
  (void)dp_size_mul (a, b, &p);
  return p;
}

/* ------------------------------------------------------------------ */
/* Array allocations: @p n elements of @p size bytes, the product      */
/* checked by dp_size_mul(). `malloc (n * sizeof *p)` wraps silently   */
/* when n is large, returns a SMALL buffer, and the loop that fills    */
/* all n elements writes past it. These are malloc / dp_xmalloc /      */
/* dp_xrealloc with the product taken out of the caller's hands, as    */
/* calloc already takes it for a zeroed allocation (calloc and         */
/* dp_xcalloc check nmemb * size themselves, so they need no sibling   */
/* -- but neither checks a product written INSIDE an argument:         */
/* `calloc (a * b, size)` wraps `a * b` first).                        */
/*                                                                     */
/* Which one:                                                          */
/*                                                                     */
/*  - dp_mallocn, when the count comes from a CALLER on a path that    */
/*    can refuse (a create(), a resize, a decode): it returns NULL on  */
/*    overflow as well as on OOM, and the caller refuses.              */
/*  - dp_xmallocn / dp_xreallocn, when the count is already validated  */
/*    and only genuine OOM could fail it: they abort, like dp_xmalloc. */
/*    An overflow there is a broken internal guarantee, not a caller's */
/*    mistake, so it aborts too rather than handing back a short      */
/*    buffer.                                                          */
/*                                                                     */
/* scripts/check_alloc_helpers.py fails on a new allocation with a    */
/* product written inline in any size argument.                        */
/* ------------------------------------------------------------------ */

static inline void *
dp_mallocn (size_t n, size_t size)
{
  size_t bytes;
  if (!dp_size_mul (n, size, &bytes))
    return NULL;
  return malloc (bytes ? bytes : 1u);
}

static inline void *
dp_xmallocn (size_t n, size_t size)
{
  size_t bytes;
  if (!dp_size_mul (n, size, &bytes))
    abort ();
  return dp_xmalloc (bytes ? bytes : 1u); /* zero: one byte (@note) */
}

static inline void *
dp_xreallocn (void *p, size_t n, size_t size)
{
  size_t bytes;
  if (!dp_size_mul (n, size, &bytes))
    abort ();
  /* zero: one byte, never realloc (p, 0) -- see the @note */
  return dp_xrealloc (p, bytes ? bytes : 1u);
}

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

static inline int
dp_positive_finite (double x)
{
  return isfinite (x) && x > 0.0;
}

static inline long
dp_fftfreq_index (size_t bin, size_t n)
{
  return (bin <= (n - 1) / 2) ? (long)bin : (long)bin - (long)n;
}

static inline double
dp_fftfreq (size_t bin, size_t n, double fs)
{
  return (n == 0) ? 0.0 : (double)dp_fftfreq_index (bin, n) * fs / (double)n;
}

JM_FORCEINLINE double
dp_fmod_pos (double x, double m)
{
  double r = fmod (x, m);
  if (r >= 0.0)
    return r;
  double y = r + m;
  return y >= m ? 0.0 : y;
}

double dp_lgamma (double x);

#endif /* DOPPLER_CLIB_COMMON_H */
```


