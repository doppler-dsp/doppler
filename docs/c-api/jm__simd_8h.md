

# File jm\_simd.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**jm\_simd.h**](jm__simd_8h.md)

[Go to the source code of this file](jm__simd_8h_source.md)

_Width-portable SIMD operation macros._ [More...](#detailed-description)

* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef float | [**JM\_VEC\_F32**](#typedef-jm_vec_f32)  <br> |
| typedef double | [**JM\_VEC\_F64**](#typedef-jm_vec_f64)  <br> |















































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**JM\_ADD\_F32**](jm__simd_8h.md#define-jm_add_f32) (a, b) `((a) + (b))`<br> |
| define  | [**JM\_ADD\_F64**](jm__simd_8h.md#define-jm_add_f64) (a, b) `((a) + (b))`<br> |
| define  | [**JM\_FMA\_F32**](jm__simd_8h.md#define-jm_fma_f32) (acc, a, b) `((acc) += (a) \* (b))`<br> |
| define  | [**JM\_FMA\_F64**](jm__simd_8h.md#define-jm_fma_f64) (acc, a, b) `((acc) += (a) \* (b))`<br> |
| define  | [**JM\_HSUM\_F32**](jm__simd_8h.md#define-jm_hsum_f32) (v) `((float)(v))`<br> |
| define  | [**JM\_HSUM\_F64**](jm__simd_8h.md#define-jm_hsum_f64) (v) `((double)(v))`<br> |
| define  | [**JM\_LOAD\_F32**](jm__simd_8h.md#define-jm_load_f32) (p) `(\*(const float \*)(p))`<br> |
| define  | [**JM\_LOAD\_F64**](jm__simd_8h.md#define-jm_load_f64) (p) `(\*(const double \*)(p))`<br> |
| define  | [**JM\_MAC\_F32**](jm__simd_8h.md#define-jm_mac_f32) (acc, ptr, s) `((acc) += (\*(const float \*)(ptr)) \* (float)(s))`<br> |
| define  | [**JM\_MAC\_F64**](jm__simd_8h.md#define-jm_mac_f64) (acc, ptr, s) `((acc) += (\*(const double \*)(ptr)) \* (double)(s))`<br> |
| define  | [**JM\_MUL\_F32**](jm__simd_8h.md#define-jm_mul_f32) (a, b) `((a) \* (b))`<br> |
| define  | [**JM\_MUL\_F64**](jm__simd_8h.md#define-jm_mul_f64) (a, b) `((a) \* (b))`<br> |
| define  | [**JM\_RESTRICT**](jm__simd_8h.md#define-jm_restrict)  `restrict`<br> |
| define  | [**JM\_SIMD\_WIDTH**](jm__simd_8h.md#define-jm_simd_width)  `1`<br> |
| define  | [**JM\_SIMD\_WIDTH\_F32**](jm__simd_8h.md#define-jm_simd_width_f32)  `1`<br> |
| define  | [**JM\_SIMD\_WIDTH\_F64**](jm__simd_8h.md#define-jm_simd_width_f64)  `1`<br> |
| define  | [**JM\_SPLAT\_F32**](jm__simd_8h.md#define-jm_splat_f32) (x) `((float)(x))`<br> |
| define  | [**JM\_SPLAT\_F64**](jm__simd_8h.md#define-jm_splat_f64) (x) `((double)(x))`<br> |
| define  | [**JM\_STORE\_F32**](jm__simd_8h.md#define-jm_store_f32) (p, v) `(\*(float \*)(p) = (v))`<br> |
| define  | [**JM\_STORE\_F64**](jm__simd_8h.md#define-jm_store_f64) (p, v) `(\*(double \*)(p) = (v))`<br> |
| define  | [**JM\_SUMSQ\_F32**](jm__simd_8h.md#define-jm_sumsq_f32) (dst, ptr, n) `/* multi line expression */`<br>_Sum of squares: dst = Σ ptr&#91;i&#93;² for i in &#91;0, n)._  |
| define  | [**JM\_ZERO\_F32**](jm__simd_8h.md#define-jm_zero_f32) () `(0.0f)`<br> |
| define  | [**JM\_ZERO\_F64**](jm__simd_8h.md#define-jm_zero_f64) () `(0.0)`<br> |

## Detailed Description


Selects the widest available instruction set at compile time: AVX-512F -&gt; 16 float / 8 double lanes (JM\_SIMD\_WIDTH\_F32 = 16) AVX2+FMA -&gt; 8 float / 4 double lanes (JM\_SIMD\_WIDTH\_F32 = 8) Scalar -&gt; 1 lane (auto-vectorisation still applies)


Typical usage (FIR inner loop, processes JM\_SIMD\_WIDTH\_F32 taps): 
```C++
JM_VEC_F32 acc = JM_ZERO_F32();
for (int k = 0; k < N_TAPS; k++)
    JM_MAC_F32(acc, window + k, coeffs[k]);
*out += JM_HSUM_F32(acc);
```



For algorithms that require ISA-specific operations not covered here (gather loads, prefix scans, permutes) use #ifdef **AVX512F** guards around the raw intrinsics. JM\_SIMD\_WIDTH\_F32 is still useful in that context as the canonical loop-stride constant.


Can be included standalone; if JM\_RESTRICT is not already defined (from [**jm\_perf.h**](jm__perf_8h.md)) a local fallback is provided. 


    
## Public Types Documentation




### typedef JM\_VEC\_F32 

```C++
typedef float JM_VEC_F32;
```




<hr>



### typedef JM\_VEC\_F64 

```C++
typedef double JM_VEC_F64;
```




<hr>
## Macro Definition Documentation





### define JM\_ADD\_F32 

```C++
#define JM_ADD_F32 (
    a,
    b
) `((a) + (b))`
```




<hr>



### define JM\_ADD\_F64 

```C++
#define JM_ADD_F64 (
    a,
    b
) `((a) + (b))`
```




<hr>



### define JM\_FMA\_F32 

```C++
#define JM_FMA_F32 (
    acc,
    a,
    b
) `((acc) += (a) * (b))`
```




<hr>



### define JM\_FMA\_F64 

```C++
#define JM_FMA_F64 (
    acc,
    a,
    b
) `((acc) += (a) * (b))`
```




<hr>



### define JM\_HSUM\_F32 

```C++
#define JM_HSUM_F32 (
    v
) `((float)(v))`
```




<hr>



### define JM\_HSUM\_F64 

```C++
#define JM_HSUM_F64 (
    v
) `((double)(v))`
```




<hr>



### define JM\_LOAD\_F32 

```C++
#define JM_LOAD_F32 (
    p
) `(*(const float *)(p))`
```




<hr>



### define JM\_LOAD\_F64 

```C++
#define JM_LOAD_F64 (
    p
) `(*(const double *)(p))`
```




<hr>



### define JM\_MAC\_F32 

```C++
#define JM_MAC_F32 (
    acc,
    ptr,
    s
) `((acc) += (*(const float *)(ptr)) * (float)(s))`
```




<hr>



### define JM\_MAC\_F64 

```C++
#define JM_MAC_F64 (
    acc,
    ptr,
    s
) `((acc) += (*(const double *)(ptr)) * (double)(s))`
```




<hr>



### define JM\_MUL\_F32 

```C++
#define JM_MUL_F32 (
    a,
    b
) `((a) * (b))`
```




<hr>



### define JM\_MUL\_F64 

```C++
#define JM_MUL_F64 (
    a,
    b
) `((a) * (b))`
```




<hr>



### define JM\_RESTRICT 

```C++
#define JM_RESTRICT `restrict`
```




<hr>



### define JM\_SIMD\_WIDTH 

```C++
#define JM_SIMD_WIDTH `1`
```




<hr>



### define JM\_SIMD\_WIDTH\_F32 

```C++
#define JM_SIMD_WIDTH_F32 `1`
```




<hr>



### define JM\_SIMD\_WIDTH\_F64 

```C++
#define JM_SIMD_WIDTH_F64 `1`
```




<hr>



### define JM\_SPLAT\_F32 

```C++
#define JM_SPLAT_F32 (
    x
) `((float)(x))`
```




<hr>



### define JM\_SPLAT\_F64 

```C++
#define JM_SPLAT_F64 (
    x
) `((double)(x))`
```




<hr>



### define JM\_STORE\_F32 

```C++
#define JM_STORE_F32 (
    p,
    v
) `(*(float *)(p) = (v))`
```




<hr>



### define JM\_STORE\_F64 

```C++
#define JM_STORE_F64 (
    p,
    v
) `(*(double *)(p) = (v))`
```




<hr>



### define JM\_SUMSQ\_F32 

_Sum of squares: dst = Σ ptr&#91;i&#93;² for i in &#91;0, n)._ 
```C++
#define JM_SUMSQ_F32 (
    dst,
    ptr,
    n
) `/* multi line expression */`
```



The bulk runs JM\_SIMD\_WIDTH\_F32-wide via FMA accumulation; the trailing `n` % JM\_SIMD\_WIDTH\_F32 elements are summed scalar. When `n` is a multiple of the SIMD width (e.g. a power-of-two block whose length is &gt;= the width) the remainder loop has zero trips and folds away, leaving a pure vector reduction.




**Parameters:**


* `dst` lvalue of type float — receives the sum. 
* `ptr` const float \* — base of the contiguous input. 
* `n` element count (size\_t-convertible).


```C++
float e;
JM_SUMSQ_F32 (e, buf, 256);   // e = energy of buf[0..255]
```
 


        

<hr>



### define JM\_ZERO\_F32 

```C++
#define JM_ZERO_F32 (
    
) `(0.0f)`
```




<hr>



### define JM\_ZERO\_F64 

```C++
#define JM_ZERO_F64 (
    
) `(0.0)`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/jm_simd.h`

