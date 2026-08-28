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
#define DP_ERR_INTERRUPTED                                                    \
  (-8) /**< A blocking call returned because dp_stream_interrupt() was     \
            called -- a request to stop, not a failure. */
#define DP_ERR_CLOSED                                                         \
  (-9) /**< The context is draining or closed and accepts no more sends -- \
            a state, not a transport failure. */
#define DP_ERR_EOF                                                            \
  (-10) /**< The producer has finished: no more data is coming, ever. A     \
             state, not a failure, and distinct from DP_ERR_TIMEOUT, which  \
             means "not yet". Every transport spells it the same way -- see \
             docs/design/io-termination.md. */

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
 * inline, and `doppler.dsss.bin_to_signed` is a thin wrapper over it, so C
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

#endif /* DOPPLER_CLIB_COMMON_H */
