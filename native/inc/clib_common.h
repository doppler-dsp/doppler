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
/* ------------------------------------------------------------------ */
#define DP_OK          0   /**< Success.                    */
#define DP_ERR_MEMORY  (-1) /**< Memory allocation failure. */
#define DP_ERR_INVALID (-2) /**< Invalid argument.          */

#include "jm_perf.h"

#endif /* CLIB_COMMON_H */
