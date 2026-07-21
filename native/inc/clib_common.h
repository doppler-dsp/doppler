/**
 * clib_common.h — common C99 types and performance macros for doppler.
 */
#ifndef DOPPLER_CLIB_COMMON_H
#define DOPPLER_CLIB_COMMON_H

#include <complex.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* CMPLXF is C11 but missing on MinGW/ucrt64.  __builtin_complex is
 * the GCC-portable way to construct a complex value without relying
 * on _Complex_I (which can propagate NaN in some implementations). */
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

#include "jm_perf.h"

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

#endif /* DOPPLER_CLIB_COMMON_H */
