

# File jm\_perf.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**jm\_perf.h**](jm__perf_8h.md)

[Go to the documentation of this file](jm__perf_8h.md)


```C++


#ifndef JM_PERF_H
#define JM_PERF_H

/* ── Hint macros ─────────────────────────────────────────────────── */

#define JM_LIKELY(x) _JM_LIKELY_ (x)
#define JM_UNLIKELY(x) _JM_UNLIKELY_ (x)
#define JM_RESTRICT _JM_RESTRICT_
#define JM_FORCEINLINE _JM_FORCEINLINE_
#define JM_ALIGNED(n) _JM_ALIGNED_ (n)
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

#define JM_UNROLL(n) _JM_UNROLL_ (n)

/* ── Pointer alignment hint ──────────────────────────────────────── */

#define JM_ASSUME_ALIGNED(ptr, n) _JM_ASSUME_ALIGNED_ (ptr, n)

/* ── Software prefetch ───────────────────────────────────────────── */

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
```
