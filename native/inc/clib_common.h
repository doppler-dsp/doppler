/**
 * clib_common.h — common C99 types and performance macros for doppler.
 */
#ifndef CLIB_COMMON_H
#define CLIB_COMMON_H

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

#include "jm_perf.h"

#endif /* CLIB_COMMON_H */
