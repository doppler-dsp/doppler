

# File jm\_perf.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**jm\_perf.h**](jm__perf_8h.md)

[Go to the source code of this file](jm__perf_8h_source.md)



* `#include "jm_simd.h"`
































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**JM\_ALIGNED**](jm__perf_8h.md#define-jm_aligned) (n) `[**JM\_ALIGNED\_IMPL**](jm__perf_8h.md#define-jm_aligned_impl)(n)`<br> |
| define  | [**JM\_ALIGNED\_IMPL**](jm__perf_8h.md#define-jm_aligned_impl) (n) <br> |
| define  | [**JM\_ASSUME\_ALIGNED**](jm__perf_8h.md#define-jm_assume_aligned) (ptr, n) `[**JM\_ASSUME\_ALIGNED\_IMPL**](jm__perf_8h.md#define-jm_assume_aligned_impl)(ptr, n)`<br> |
| define  | [**JM\_ASSUME\_ALIGNED\_IMPL**](jm__perf_8h.md#define-jm_assume_aligned_impl) (p, n) `(p)`<br> |
| define  | [**JM\_DEFINE\_STEPS**](jm__perf_8h.md#define-jm_define_steps) (fn, state\_t, sample\_t, LENGTH, BATCH, CHUNK) `[**JM\_DEFINE\_STEPS\_EX**](jm__perf_8h.md#define-jm_define_steps_ex)(fn, state\_t, sample\_t, LENGTH, BATCH, CHUNK, (), ())`<br> |
| define  | [**JM\_DEFINE\_STEPS\_EX**](jm__perf_8h.md#define-jm_define_steps_ex) (fn, state\_t, sample\_t, LENGTH, BATCH, CHUNK, CPARAMS, CARGS) `/* multi line expression */`<br> |
| define  | [**JM\_EVAL\_IMPL**](jm__perf_8h.md#define-jm_eval_impl) (...) `\_\_VA\_ARGS\_\_`<br> |
| define  | [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline)  `[**JM\_FORCEINLINE\_IMPL**](jm__perf_8h.md#define-jm_forceinline_impl)`<br> |
| define  | [**JM\_FORCEINLINE\_IMPL**](jm__perf_8h.md#define-jm_forceinline_impl)  `inline`<br> |
| define  | [**JM\_HOT**](jm__perf_8h.md#define-jm_hot)  `[**JM\_HOT\_IMPL**](jm__perf_8h.md#define-jm_hot_impl)`<br> |
| define  | [**JM\_HOT\_IMPL**](jm__perf_8h.md#define-jm_hot_impl)  <br> |
| define  | [**JM\_LIKELY**](jm__perf_8h.md#define-jm_likely) (x) `[**JM\_LIKELY\_IMPL**](jm__perf_8h.md#define-jm_likely_impl)(x)`<br> |
| define  | [**JM\_LIKELY\_IMPL**](jm__perf_8h.md#define-jm_likely_impl) (x) `(x)`<br> |
| define  | [**JM\_PREFETCH**](jm__perf_8h.md#define-jm_prefetch) (ptr, rw, loc) `[**JM\_PREFETCH\_IMPL**](jm__perf_8h.md#define-jm_prefetch_impl)(ptr, rw, loc)`<br> |
| define  | [**JM\_PREFETCH\_IMPL**](jm__perf_8h.md#define-jm_prefetch_impl) (p, rw, loc) <br> |
| define  | [**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict)  `[**JM\_RESTRICT\_IMPL**](jm__perf_8h.md#define-jm_restrict_impl)`<br> |
| define  | [**JM\_RESTRICT\_IMPL**](jm__perf_8h.md#define-jm_restrict_impl)  `restrict`<br> |
| define  | [**JM\_STEPS\_SIMD\_IMPL**](jm__perf_8h.md#define-jm_steps_simd_impl) (fn, st, samp, LENGTH, BATCH, CHUNK, CARGS) `/\* scalar: no batching \*/`<br> |
| define  | [**JM\_UNLIKELY**](jm__perf_8h.md#define-jm_unlikely) (x) `[**JM\_UNLIKELY\_IMPL**](jm__perf_8h.md#define-jm_unlikely_impl)(x)`<br> |
| define  | [**JM\_UNLIKELY\_IMPL**](jm__perf_8h.md#define-jm_unlikely_impl) (x) `(x)`<br> |
| define  | [**JM\_UNROLL**](jm__perf_8h.md#define-jm_unroll) (n) `[**JM\_UNROLL\_IMPL**](jm__perf_8h.md#define-jm_unroll_impl)(n)`<br> |
| define  | [**JM\_UNROLL\_IMPL**](jm__perf_8h.md#define-jm_unroll_impl) (n) <br> |

## Macro Definition Documentation





### define JM\_ALIGNED 

```C++
#define JM_ALIGNED (
    n
) `JM_ALIGNED_IMPL (n)`
```




<hr>



### define JM\_ALIGNED\_IMPL 

```C++
#define JM_ALIGNED_IMPL (
    n
) 
```




<hr>



### define JM\_ASSUME\_ALIGNED 

```C++
#define JM_ASSUME_ALIGNED (
    ptr,
    n
) `JM_ASSUME_ALIGNED_IMPL (ptr, n)`
```




<hr>



### define JM\_ASSUME\_ALIGNED\_IMPL 

```C++
#define JM_ASSUME_ALIGNED_IMPL (
    p,
    n
) `(p)`
```




<hr>



### define JM\_DEFINE\_STEPS 

```C++
#define JM_DEFINE_STEPS (
    fn,
    state_t,
    sample_t,
    LENGTH,
    BATCH,
    CHUNK
) `JM_DEFINE_STEPS_EX (fn, state_t, sample_t, LENGTH, BATCH, CHUNK, (), ())`
```




<hr>



### define JM\_DEFINE\_STEPS\_EX 

```C++
#define JM_DEFINE_STEPS_EX (
    fn,
    state_t,
    sample_t,
    LENGTH,
    BATCH,
    CHUNK,
    CPARAMS,
    CARGS
) `/* multi line expression */`
```




<hr>



### define JM\_EVAL\_IMPL 

```C++
#define JM_EVAL_IMPL (
    ...
) `__VA_ARGS__`
```




<hr>



### define JM\_FORCEINLINE 

```C++
#define JM_FORCEINLINE `JM_FORCEINLINE_IMPL`
```




<hr>



### define JM\_FORCEINLINE\_IMPL 

```C++
#define JM_FORCEINLINE_IMPL `inline`
```




<hr>



### define JM\_HOT 

```C++
#define JM_HOT `JM_HOT_IMPL`
```




<hr>



### define JM\_HOT\_IMPL 

```C++
#define JM_HOT_IMPL 
```




<hr>



### define JM\_LIKELY 

```C++
#define JM_LIKELY (
    x
) `JM_LIKELY_IMPL (x)`
```



[**jm\_perf.h**](jm__perf_8h.md) — compiler-hint macros for doppler.


All macros expand to safe no-ops on unknown compilers. Include freely; zero runtime cost. 


        

<hr>



### define JM\_LIKELY\_IMPL 

```C++
#define JM_LIKELY_IMPL (
    x
) `(x)`
```




<hr>



### define JM\_PREFETCH 

```C++
#define JM_PREFETCH (
    ptr,
    rw,
    loc
) `JM_PREFETCH_IMPL (ptr, rw, loc)`
```




<hr>



### define JM\_PREFETCH\_IMPL 

```C++
#define JM_PREFETCH_IMPL (
    p,
    rw,
    loc
) 
```




<hr>



### define JM\_RESTRICT 

```C++
#define JM_RESTRICT `JM_RESTRICT_IMPL`
```




<hr>



### define JM\_RESTRICT\_IMPL 

```C++
#define JM_RESTRICT_IMPL `restrict`
```




<hr>



### define JM\_STEPS\_SIMD\_IMPL 

```C++
#define JM_STEPS_SIMD_IMPL (
    fn,
    st,
    samp,
    LENGTH,
    BATCH,
    CHUNK,
    CARGS
) `/* scalar: no batching */`
```




<hr>



### define JM\_UNLIKELY 

```C++
#define JM_UNLIKELY (
    x
) `JM_UNLIKELY_IMPL (x)`
```




<hr>



### define JM\_UNLIKELY\_IMPL 

```C++
#define JM_UNLIKELY_IMPL (
    x
) `(x)`
```




<hr>



### define JM\_UNROLL 

```C++
#define JM_UNROLL (
    n
) `JM_UNROLL_IMPL (n)`
```




<hr>



### define JM\_UNROLL\_IMPL 

```C++
#define JM_UNROLL_IMPL (
    n
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/jm_perf.h`

