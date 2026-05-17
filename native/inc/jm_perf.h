/**
 * @file jm_perf.h
 * @brief just-makeit performance annotation macros.
 *
 * Portable hints for the compiler and CPU: hot functions, forced
 * inlining, restrict aliasing, branch prediction, alignment,
 * prefetch, and loop unrolling.  All macros degrade gracefully to
 * safe no-ops on unknown compilers.
 *
 * Include order: jm_perf.h includes jm_simd.h automatically.
 * Either header may be included standalone — jm_simd.h guards
 * against redefining JM_RESTRICT if jm_perf.h was already included.
 *
 * Usage:
 * @code
 * JM_HOT static void process(const float * JM_RESTRICT in,
 *                             float * JM_RESTRICT out, size_t n)
 * {
 *     JM_PREFETCH(in + 16, 0, 1);
 *     JM_UNROLL(4)
 *     for (size_t i = 0; i < n; i++)
 *         out[i] = in[i] * 2.0f;
 * }
 * @endcode
 */

#ifndef JM_PERF_H
#define JM_PERF_H

/* ── Hint macros ─────────────────────────────────────────────────── */

/** Hint that x is almost always true. */
#define JM_LIKELY(x) _JM_LIKELY_ (x)
/** Hint that x is almost never true. */
#define JM_UNLIKELY(x) _JM_UNLIKELY_ (x)
/** Assert a pointer does not alias any other; enables vectorisation. */
#define JM_RESTRICT _JM_RESTRICT_
/** Force inlining regardless of the compiler's cost model. */
#define JM_FORCEINLINE _JM_FORCEINLINE_
/** Align a variable or struct member to n bytes. */
#define JM_ALIGNED(n) _JM_ALIGNED_ (n)
/** Mark a function as performance-critical (hot section). */
#define JM_HOT _JM_HOT_

/* ── GCC / Clang ─────────────────────────────────────────────────── */
#if defined(__GNUC__) || defined(__clang__)
#define _JM_LIKELY_(x) __builtin_expect (!!(x), 1)
#define _JM_UNLIKELY_(x) __builtin_expect (!!(x), 0)
#define _JM_RESTRICT_ __restrict__
#define _JM_FORCEINLINE_ __attribute__ ((always_inline)) inline
#define _JM_ALIGNED_(n) __attribute__ ((aligned (n)))
#define _JM_HOT_ __attribute__ ((hot))

/* ── MSVC ────────────────────────────────────────────────────────── */
#elif defined(_MSC_VER)
#define _JM_LIKELY_(x) (x)
#define _JM_UNLIKELY_(x) (x)
#define _JM_RESTRICT_ __restrict
#define _JM_FORCEINLINE_ __forceinline
#define _JM_ALIGNED_(n) __declspec (align (n))
#define _JM_HOT_

/* ── Unknown / strict C99 — safe no-ops ─────────────────────────── */
#else
#define _JM_LIKELY_(x) (x)
#define _JM_UNLIKELY_(x) (x)
#define _JM_RESTRICT_ restrict
#define _JM_FORCEINLINE_ inline
#define _JM_ALIGNED_(n)
#define _JM_HOT_
#endif

/* ── Loop unroll ─────────────────────────────────────────────────── */

/**
 * @brief Unroll the immediately following for-loop exactly n times.
 *
 * Applied before a for loop, instructs GCC/Clang to unroll it
 * unconditionally by the given factor, regardless of the compiler's
 * own cost model.  Use only on tight, well-measured inner loops with
 * a known iteration count — a large n on a non-trivial body will
 * bloat code size and hurt instruction-cache pressure.
 *
 * No-op on compilers that do not support the pragma.
 */
#define JM_UNROLL(n) _JM_UNROLL_ (n)

/* ── Pointer alignment hint ──────────────────────────────────────── */

/**
 * @brief Inform the compiler that ptr is aligned to n bytes.
 *
 * Enables aligned SIMD loads/stores on ISAs that penalise
 * unaligned access.  Returns ptr so it can be used in expressions.
 * Falls back to ptr unchanged on unknown compilers.
 */
#define JM_ASSUME_ALIGNED(ptr, n) _JM_ASSUME_ALIGNED_ (ptr, n)

/* ── Software prefetch ───────────────────────────────────────────── */

/**
 * @brief Issue a non-blocking prefetch hint to the CPU.
 *
 * @param ptr      Address to prefetch.
 * @param rw       0 = read (PLD), 1 = write (PSTL).
 * @param loc      Cache level: 3 = L1 (hottest), 0 = NTA (streaming).
 *
 * Prefetching 1–2 cache lines ahead of the load keeps the data
 * pipeline full on high-latency memory.  No-op on unknown compilers.
 */
#define JM_PREFETCH(ptr, rw, loc) _JM_PREFETCH_ (ptr, rw, loc)

#if defined(__GNUC__) || defined(__clang__)
#define _JM_STRINGIFY_(x) #x
#define _JM_UNROLL_(n) _Pragma (_JM_STRINGIFY_ (GCC unroll n))
#define _JM_ASSUME_ALIGNED_(p, n) __builtin_assume_aligned (p, n)
#define _JM_PREFETCH_(p, rw, loc) __builtin_prefetch (p, rw, loc)
#else
#define _JM_UNROLL_(n)
#define _JM_ASSUME_ALIGNED_(p, n) (p)
#define _JM_PREFETCH_(p, rw, loc)
#endif

#include "jm_simd.h"

#endif /* JM_PERF_H */
