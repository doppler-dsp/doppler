

# File jm\_perf.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**jm\_perf.h**](jm__perf_8h.md)

[Go to the source code of this file](jm__perf_8h_source.md)



* `#include "doppler/jm_simd.h"`
































































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
| define  | [**JM\_PP\_IS\_ZERO**](jm__perf_8h.md#define-jm_pp_is_zero) (x) `[**JM\_PP\_IS\_ZERO\_SEL**](jm__perf_8h.md#define-jm_pp_is_zero_sel)([**JM\_PP\_SECOND**](jm__perf_8h.md#define-jm_pp_second)([**JM\_PP\_PAREN\_PROBE\_**](jm__perf_8h.md#define-jm_pp_paren_probe_) x, 0), x)`<br> |
| define  | [**JM\_PP\_IS\_ZERO\_PAREN\_0**](jm__perf_8h.md#define-jm_pp_is_zero_paren_0) (x) `[**JM\_PP\_IS\_ZERO\_TOKEN\_**](jm__perf_8h.md#define-jm_pp_is_zero_token_)(x)`<br> |
| define  | [**JM\_PP\_IS\_ZERO\_PAREN\_1**](jm__perf_8h.md#define-jm_pp_is_zero_paren_1) (x) `0`<br> |
| define  | [**JM\_PP\_IS\_ZERO\_SEL**](jm__perf_8h.md#define-jm_pp_is_zero_sel) (paren, x) `[**JM\_PP\_IS\_ZERO\_SEL\_**](jm__perf_8h.md#define-jm_pp_is_zero_sel_)(paren, x)`<br> |
| define  | [**JM\_PP\_IS\_ZERO\_SEL\_**](jm__perf_8h.md#define-jm_pp_is_zero_sel_) (paren, x) `JM\_PP\_IS\_ZERO\_PAREN\_##paren(x)`<br> |
| define  | [**JM\_PP\_IS\_ZERO\_TOKEN\_**](jm__perf_8h.md#define-jm_pp_is_zero_token_) (x) `[**JM\_PP\_SECOND**](jm__perf_8h.md#define-jm_pp_second)(JM\_PP\_ZERO\_PROBE\_##x, 0)`<br> |
| define  | [**JM\_PP\_PAREN\_PROBE\_**](jm__perf_8h.md#define-jm_pp_paren_probe_) (...) `~, 1`<br> |
| define  | [**JM\_PP\_SECOND**](jm__perf_8h.md#define-jm_pp_second) (...) `[**JM\_PP\_SECOND\_**](jm__perf_8h.md#define-jm_pp_second_)(\_\_VA\_ARGS\_\_, )`<br> |
| define  | [**JM\_PP\_SECOND\_**](jm__perf_8h.md#define-jm_pp_second_) (a, b, ...) `b`<br> |
| define  | [**JM\_PP\_ZERO\_PROBE\_0**](jm__perf_8h.md#define-jm_pp_zero_probe_0)  `~, 1`<br> |
| define  | [**JM\_PREFETCH**](jm__perf_8h.md#define-jm_prefetch) (ptr, rw, loc) `[**JM\_PREFETCH\_IMPL**](jm__perf_8h.md#define-jm_prefetch_impl)(ptr, rw, loc)`<br> |
| define  | [**JM\_PREFETCH\_IMPL**](jm__perf_8h.md#define-jm_prefetch_impl) (p, rw, loc) <br> |
| define  | [**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict)  `[**JM\_RESTRICT\_IMPL**](jm__perf_8h.md#define-jm_restrict_impl)`<br> |
| define  | [**JM\_RESTRICT\_IMPL**](jm__perf_8h.md#define-jm_restrict_impl)  `restrict`<br> |
| define  | [**JM\_STEPS\_HISTORY**](jm__perf_8h.md#define-jm_steps_history) (dir, LENGTH) `[**JM\_STEPS\_HISTORY\_SEL**](jm__perf_8h.md#define-jm_steps_history_sel)(dir, [**JM\_PP\_IS\_ZERO**](jm__perf_8h.md#define-jm_pp_is_zero)(LENGTH), LENGTH)`<br> |
| define  | [**JM\_STEPS\_HISTORY\_IN\_0**](jm__perf_8h.md#define-jm_steps_history_in_0) (LENGTH) `/* multi line expression */`<br> |
| define  | [**JM\_STEPS\_HISTORY\_IN\_1**](jm__perf_8h.md#define-jm_steps_history_in_1) (LENGTH) <br> |
| define  | [**JM\_STEPS\_HISTORY\_OUT\_0**](jm__perf_8h.md#define-jm_steps_history_out_0) (LENGTH) `/* multi line expression */`<br> |
| define  | [**JM\_STEPS\_HISTORY\_OUT\_1**](jm__perf_8h.md#define-jm_steps_history_out_1) (LENGTH) <br> |
| define  | [**JM\_STEPS\_HISTORY\_SEL**](jm__perf_8h.md#define-jm_steps_history_sel) (dir, zero, LENGTH) `[**JM\_STEPS\_HISTORY\_SEL\_**](jm__perf_8h.md#define-jm_steps_history_sel_)(dir, zero, LENGTH)`<br> |
| define  | [**JM\_STEPS\_HISTORY\_SEL\_**](jm__perf_8h.md#define-jm_steps_history_sel_) (dir, zero, LENGTH) `JM\_STEPS\_HISTORY\_##dir##zero(LENGTH)`<br> |
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



### define JM\_PP\_IS\_ZERO 

```C++
#define JM_PP_IS_ZERO (
    x
) `JM_PP_IS_ZERO_SEL ( JM_PP_SECOND ( JM_PP_PAREN_PROBE_ x, 0), x)`
```




<hr>



### define JM\_PP\_IS\_ZERO\_PAREN\_0 

```C++
#define JM_PP_IS_ZERO_PAREN_0 (
    x
) `JM_PP_IS_ZERO_TOKEN_ (x)`
```




<hr>



### define JM\_PP\_IS\_ZERO\_PAREN\_1 

```C++
#define JM_PP_IS_ZERO_PAREN_1 (
    x
) `0`
```




<hr>



### define JM\_PP\_IS\_ZERO\_SEL 

```C++
#define JM_PP_IS_ZERO_SEL (
    paren,
    x
) `JM_PP_IS_ZERO_SEL_ (paren, x)`
```




<hr>



### define JM\_PP\_IS\_ZERO\_SEL\_ 

```C++
#define JM_PP_IS_ZERO_SEL_ (
    paren,
    x
) `JM_PP_IS_ZERO_PAREN_##paren(x)`
```




<hr>



### define JM\_PP\_IS\_ZERO\_TOKEN\_ 

```C++
#define JM_PP_IS_ZERO_TOKEN_ (
    x
) `JM_PP_SECOND (JM_PP_ZERO_PROBE_##x, 0)`
```




<hr>



### define JM\_PP\_PAREN\_PROBE\_ 

```C++
#define JM_PP_PAREN_PROBE_ (
    ...
) `~, 1`
```




<hr>



### define JM\_PP\_SECOND 

```C++
#define JM_PP_SECOND (
    ...
) `JM_PP_SECOND_ (__VA_ARGS__, )`
```




<hr>



### define JM\_PP\_SECOND\_ 

```C++
#define JM_PP_SECOND_ (
    a,
    b,
    ...
) `b`
```




<hr>



### define JM\_PP\_ZERO\_PROBE\_0 

```C++
#define JM_PP_ZERO_PROBE_0 `~, 1`
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



### define JM\_STEPS\_HISTORY 

```C++
#define JM_STEPS_HISTORY (
    dir,
    LENGTH
) `JM_STEPS_HISTORY_SEL (dir, JM_PP_IS_ZERO (LENGTH), LENGTH)`
```




<hr>



### define JM\_STEPS\_HISTORY\_IN\_0 

```C++
#define JM_STEPS_HISTORY_IN_0 (
    LENGTH
) `/* multi line expression */`
```




<hr>



### define JM\_STEPS\_HISTORY\_IN\_1 

```C++
#define JM_STEPS_HISTORY_IN_1 (
    LENGTH
) 
```




<hr>



### define JM\_STEPS\_HISTORY\_OUT\_0 

```C++
#define JM_STEPS_HISTORY_OUT_0 (
    LENGTH
) `/* multi line expression */`
```




<hr>



### define JM\_STEPS\_HISTORY\_OUT\_1 

```C++
#define JM_STEPS_HISTORY_OUT_1 (
    LENGTH
) 
```




<hr>



### define JM\_STEPS\_HISTORY\_SEL 

```C++
#define JM_STEPS_HISTORY_SEL (
    dir,
    zero,
    LENGTH
) `JM_STEPS_HISTORY_SEL_ (dir, zero, LENGTH)`
```




<hr>



### define JM\_STEPS\_HISTORY\_SEL\_ 

```C++
#define JM_STEPS_HISTORY_SEL_ (
    dir,
    zero,
    LENGTH
) `JM_STEPS_HISTORY_##dir##zero(LENGTH)`
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
The documentation for this class was generated from the following file `native/inc/doppler/jm_perf.h`

