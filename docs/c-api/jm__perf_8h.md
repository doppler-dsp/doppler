

# File jm\_perf.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**jm\_perf.h**](jm__perf_8h.md)

[Go to the source code of this file](jm__perf_8h_source.md)

_just-makeit performance annotation macros._ [More...](#detailed-description)

* `#include "jm_simd.h"`
































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**JM\_ALIGNED**](jm__perf_8h.md#define-jm_aligned) (n) `[**\_JM\_ALIGNED\_**](jm__perf_8h.md#define-_jm_aligned_)(n)`<br> |
| define  | [**JM\_ASSUME\_ALIGNED**](jm__perf_8h.md#define-jm_assume_aligned) (ptr, n) `[**\_JM\_ASSUME\_ALIGNED\_**](jm__perf_8h.md#define-_jm_assume_aligned_)(ptr, n)`<br>_Inform the compiler that ptr is aligned to n bytes._  |
| define  | [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline)  `[**\_JM\_FORCEINLINE\_**](jm__perf_8h.md#define-_jm_forceinline_)`<br> |
| define  | [**JM\_HOT**](jm__perf_8h.md#define-jm_hot)  `[**\_JM\_HOT\_**](jm__perf_8h.md#define-_jm_hot_)`<br> |
| define  | [**JM\_LIKELY**](jm__perf_8h.md#define-jm_likely) (x) `[**\_JM\_LIKELY\_**](jm__perf_8h.md#define-_jm_likely_)(x)`<br> |
| define  | [**JM\_PREFETCH**](jm__perf_8h.md#define-jm_prefetch) (ptr, rw, loc) `[**\_JM\_PREFETCH\_**](jm__perf_8h.md#define-_jm_prefetch_)(ptr, rw, loc)`<br>_Issue a non-blocking prefetch hint to the CPU._  |
| define  | [**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict)  `[**\_JM\_RESTRICT\_**](jm__perf_8h.md#define-_jm_restrict_)`<br> |
| define  | [**JM\_UNLIKELY**](jm__perf_8h.md#define-jm_unlikely) (x) `[**\_JM\_UNLIKELY\_**](jm__perf_8h.md#define-_jm_unlikely_)(x)`<br> |
| define  | [**JM\_UNROLL**](jm__perf_8h.md#define-jm_unroll) (n) `[**\_JM\_UNROLL\_**](jm__perf_8h.md#define-_jm_unroll_)(n)`<br>_Unroll the immediately following for-loop exactly n times._  |
| define  | [**\_JM\_ALIGNED\_**](jm__perf_8h.md#define-_jm_aligned_) (n) <br> |
| define  | [**\_JM\_ASSUME\_ALIGNED\_**](jm__perf_8h.md#define-_jm_assume_aligned_) (p, n) `(p)`<br> |
| define  | [**\_JM\_FORCEINLINE\_**](jm__perf_8h.md#define-_jm_forceinline_)  `inline`<br> |
| define  | [**\_JM\_HOT\_**](jm__perf_8h.md#define-_jm_hot_)  <br> |
| define  | [**\_JM\_LIKELY\_**](jm__perf_8h.md#define-_jm_likely_) (x) `(x)`<br> |
| define  | [**\_JM\_PREFETCH\_**](jm__perf_8h.md#define-_jm_prefetch_) (p, rw, loc) <br> |
| define  | [**\_JM\_RESTRICT\_**](jm__perf_8h.md#define-_jm_restrict_)  `restrict`<br> |
| define  | [**\_JM\_UNLIKELY\_**](jm__perf_8h.md#define-_jm_unlikely_) (x) `(x)`<br> |
| define  | [**\_JM\_UNROLL\_**](jm__perf_8h.md#define-_jm_unroll_) (n) <br> |

## Detailed Description


Portable hints for the compiler and CPU: hot functions, forced inlining, restrict aliasing, branch prediction, alignment, prefetch, and loop unrolling. All macros degrade gracefully to safe no-ops on unknown compilers.


Include order: [**jm\_perf.h**](jm__perf_8h.md) includes [**jm\_simd.h**](jm__simd_8h.md) automatically. Either header may be included standalone — [**jm\_simd.h**](jm__simd_8h.md) guards against redefining JM\_RESTRICT if [**jm\_perf.h**](jm__perf_8h.md) was already included.


Usage:
```C++
JM_HOT static void process(const float * JM_RESTRICT in,
                            float * JM_RESTRICT out, size_t n)
{
    JM_PREFETCH(in + 16, 0, 1);
    JM_UNROLL(4)
    for (size_t i = 0; i < n; i++)
        out[i] = in[i] * 2.0f;
}
```




## Macro Definition Documentation





### define JM\_ALIGNED

```C++
#define JM_ALIGNED (
    n
) `_JM_ALIGNED_ (n)`
```



Align a variable or struct member to n bytes.




<hr>



### define JM\_ASSUME\_ALIGNED

_Inform the compiler that ptr is aligned to n bytes._
```C++
#define JM_ASSUME_ALIGNED (
    ptr,
    n
) `_JM_ASSUME_ALIGNED_ (ptr, n)`
```



Enables aligned SIMD loads/stores on ISAs that penalise unaligned access. Returns ptr so it can be used in expressions. Falls back to ptr unchanged on unknown compilers.




<hr>



### define JM\_FORCEINLINE

```C++
#define JM_FORCEINLINE `_JM_FORCEINLINE_`
```



Force inlining regardless of the compiler's cost model.




<hr>



### define JM\_HOT

```C++
#define JM_HOT `_JM_HOT_`
```



Mark a function as performance-critical (hot section).




<hr>



### define JM\_LIKELY

```C++
#define JM_LIKELY (
    x
) `_JM_LIKELY_ (x)`
```



Hint that x is almost always true.




<hr>



### define JM\_PREFETCH

_Issue a non-blocking prefetch hint to the CPU._
```C++
#define JM_PREFETCH (
    ptr,
    rw,
    loc
) `_JM_PREFETCH_ (ptr, rw, loc)`
```





**Parameters:**


* `ptr` Address to prefetch.
* `rw` 0 = read (PLD), 1 = write (PSTL).
* `loc` Cache level: 3 = L1 (hottest), 0 = NTA (streaming).

Prefetching 1–2 cache lines ahead of the load keeps the data pipeline full on high-latency memory. No-op on unknown compilers.




<hr>



### define JM\_RESTRICT

```C++
#define JM_RESTRICT `_JM_RESTRICT_`
```



Assert a pointer does not alias any other; enables vectorisation.




<hr>



### define JM\_UNLIKELY

```C++
#define JM_UNLIKELY (
    x
) `_JM_UNLIKELY_ (x)`
```



Hint that x is almost never true.




<hr>



### define JM\_UNROLL

_Unroll the immediately following for-loop exactly n times._
```C++
#define JM_UNROLL (
    n
) `_JM_UNROLL_ (n)`
```



Applied before a for loop, instructs GCC/Clang to unroll it unconditionally by the given factor, regardless of the compiler's own cost model. Use only on tight, well-measured inner loops with a known iteration count — a large n on a non-trivial body will bloat code size and hurt instruction-cache pressure.


No-op on compilers that do not support the pragma.




<hr>



### define \_JM\_ALIGNED\_

```C++
#define _JM_ALIGNED_ (
    n
)
```




<hr>



### define \_JM\_ASSUME\_ALIGNED\_

```C++
#define _JM_ASSUME_ALIGNED_ (
    p,
    n
) `(p)`
```




<hr>



### define \_JM\_FORCEINLINE\_

```C++
#define _JM_FORCEINLINE_ `inline`
```




<hr>



### define \_JM\_HOT\_

```C++
#define _JM_HOT_
```




<hr>



### define \_JM\_LIKELY\_

```C++
#define _JM_LIKELY_ (
    x
) `(x)`
```




<hr>



### define \_JM\_PREFETCH\_

```C++
#define _JM_PREFETCH_ (
    p,
    rw,
    loc
)
```




<hr>



### define \_JM\_RESTRICT\_

```C++
#define _JM_RESTRICT_ `restrict`
```




<hr>



### define \_JM\_UNLIKELY\_

```C++
#define _JM_UNLIKELY_ (
    x
) `(x)`
```




<hr>



### define \_JM\_UNROLL\_

```C++
#define _JM_UNROLL_ (
    n
)
```




<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/jm_perf.h`
