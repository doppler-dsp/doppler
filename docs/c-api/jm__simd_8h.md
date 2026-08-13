

# File jm\_simd.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**jm\_simd.h**](jm__simd_8h.md)

[Go to the source code of this file](jm__simd_8h_source.md)




















## Public Types

| Type | Name |
| ---: | :--- |
| typedef float | [**JM\_VEC\_F32**](#typedef-jm_vec_f32)  <br> |
| typedef double | [**JM\_VEC\_F64**](#typedef-jm_vec_f64)  <br> |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  float | [**jm\_dot\_f32**](#function-jm_dot_f32) (const float \*[**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict) a, const float \*[**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict) b, int n) <br> |
|  double | [**jm\_dot\_f64**](#function-jm_dot_f64) (const double \*[**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict) a, const double \*[**JM\_RESTRICT**](jm__perf_8h.md#define-jm_restrict) b, int n) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**JM\_ADD\_F32**](jm__simd_8h.md#define-jm_add_f32) (a, b) `((a) + (b))`<br> |
| define  | [**JM\_ADD\_F64**](jm__simd_8h.md#define-jm_add_f64) (a, b) `((a) + (b))`<br> |
| define  | [**JM\_FMA\_F32**](jm__simd_8h.md#define-jm_fma_f32) (acc, a, b) `((acc) += (a) \* (b))`<br> |
| define  | [**JM\_FMA\_F64**](jm__simd_8h.md#define-jm_fma_f64) (acc, a, b) `((acc) += (a) \* (b))`<br> |
| define  | [**JM\_HSUM\_F32**](jm__simd_8h.md#define-jm_hsum_f32) (v) `(v)`<br> |
| define  | [**JM\_HSUM\_F64**](jm__simd_8h.md#define-jm_hsum_f64) (v) `(v)`<br> |
| define  | [**JM\_LOAD\_F32**](jm__simd_8h.md#define-jm_load_f32) (p) `(\*(p))`<br> |
| define  | [**JM\_LOAD\_F64**](jm__simd_8h.md#define-jm_load_f64) (p) `(\*(p))`<br> |
| define  | [**JM\_MAC\_F32**](jm__simd_8h.md#define-jm_mac_f32) (acc, ptr, s) `((acc) += (\*(ptr)) \* (s))`<br> |
| define  | [**JM\_MAC\_F64**](jm__simd_8h.md#define-jm_mac_f64) (acc, ptr, s) `((acc) += (\*(ptr)) \* (s))`<br> |
| define  | [**JM\_MUL\_F32**](jm__simd_8h.md#define-jm_mul_f32) (a, b) `((a) \* (b))`<br> |
| define  | [**JM\_MUL\_F64**](jm__simd_8h.md#define-jm_mul_f64) (a, b) `((a) \* (b))`<br> |
| define  | [**JM\_RESTRICT**](jm__simd_8h.md#define-jm_restrict)  `restrict`<br> |
| define  | [**JM\_SIMD\_WIDTH**](jm__simd_8h.md#define-jm_simd_width)  `1`<br> |
| define  | [**JM\_SIMD\_WIDTH\_F32**](jm__simd_8h.md#define-jm_simd_width_f32)  `1`<br> |
| define  | [**JM\_SIMD\_WIDTH\_F64**](jm__simd_8h.md#define-jm_simd_width_f64)  `1`<br> |
| define  | [**JM\_SPLAT\_F32**](jm__simd_8h.md#define-jm_splat_f32) (x) `(x)`<br> |
| define  | [**JM\_SPLAT\_F64**](jm__simd_8h.md#define-jm_splat_f64) (x) `(x)`<br> |
| define  | [**JM\_STORE\_F32**](jm__simd_8h.md#define-jm_store_f32) (p, v) `(\*(p) = (v))`<br> |
| define  | [**JM\_STORE\_F64**](jm__simd_8h.md#define-jm_store_f64) (p, v) `(\*(p) = (v))`<br> |
| define  | [**JM\_ZERO\_F32**](jm__simd_8h.md#define-jm_zero_f32) () `0.0f`<br> |
| define  | [**JM\_ZERO\_F64**](jm__simd_8h.md#define-jm_zero_f64) () `0.0`<br> |

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
## Public Static Functions Documentation




### function jm\_dot\_f32 

```C++
static inline float jm_dot_f32 (
    const float * JM_RESTRICT a,
    const float * JM_RESTRICT b,
    int n
) 
```




<hr>



### function jm\_dot\_f64 

```C++
static inline double jm_dot_f64 (
    const double * JM_RESTRICT a,
    const double * JM_RESTRICT b,
    int n
) 
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
) `(v)`
```




<hr>



### define JM\_HSUM\_F64 

```C++
#define JM_HSUM_F64 (
    v
) `(v)`
```




<hr>



### define JM\_LOAD\_F32 

```C++
#define JM_LOAD_F32 (
    p
) `(*(p))`
```




<hr>



### define JM\_LOAD\_F64 

```C++
#define JM_LOAD_F64 (
    p
) `(*(p))`
```




<hr>



### define JM\_MAC\_F32 

```C++
#define JM_MAC_F32 (
    acc,
    ptr,
    s
) `((acc) += (*(ptr)) * (s))`
```




<hr>



### define JM\_MAC\_F64 

```C++
#define JM_MAC_F64 (
    acc,
    ptr,
    s
) `((acc) += (*(ptr)) * (s))`
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



[**jm\_simd.h**](jm__simd_8h.md) — width-portable SIMD operation macros.


Selects the widest available instruction set at compile time: AVX-512F -&gt; 16 float / 8 double lanes (JM\_SIMD\_WIDTH\_F32 = 16) AVX2+FMA -&gt; 8 float / 4 double lanes (JM\_SIMD\_WIDTH\_F32 = 8) NEON -&gt; 4 float / 2 double lanes (JM\_SIMD\_WIDTH\_F32 = 4) (aarch64) Scalar -&gt; 1 lane (auto-vectorisation still applies)


Typical usage (FIR inner loop):


DOPPLER-LOCAL EDIT — the 
```C++
 fence is not in jm's template, and it is
load-bearing here: doppler renders these headers through mkdoxy and builds
the result with `zensical build --strict`. Unfenced, the `coeffs[k]` below
reaches markdown as prose, CommonMark reads `[k]` as a shortcut link
reference with no definition, and the docs build ABORTS — which partially
writes `site/` and made `check_site_links` report 864 broken links from one
real cause. Losing this fence to a re-vendor is the SECOND doppler edit this
file has lost that way (the first was JM_SUMSQ_F32, now in dp_simd.h).
Upstream as just-makeit#968; drop this note and re-vendor once that ships.
@code
  JM_VEC_F32 acc = JM_ZERO_F32();
  for (int k = 0; k < N_TAPS; k++)
      JM_MAC_F32(acc, window + k, coeffs[k]);
  *out = JM_HSUM_F32(acc);
```



JM\_SIMD\_WIDTH\_F32 tells you how many floats the loop above advances per iteration — stride your outer loop accordingly.


Can be included standalone; reuses JM\_RESTRICT from [**jm\_perf.h**](jm__perf_8h.md) if already defined, otherwise provides its own fallback. 


        

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
) `(x)`
```




<hr>



### define JM\_SPLAT\_F64 

```C++
#define JM_SPLAT_F64 (
    x
) `(x)`
```




<hr>



### define JM\_STORE\_F32 

```C++
#define JM_STORE_F32 (
    p,
    v
) `(*(p) = (v))`
```




<hr>



### define JM\_STORE\_F64 

```C++
#define JM_STORE_F64 (
    p,
    v
) `(*(p) = (v))`
```




<hr>



### define JM\_ZERO\_F32 

```C++
#define JM_ZERO_F32 (
    
) `0.0f`
```




<hr>



### define JM\_ZERO\_F64 

```C++
#define JM_ZERO_F64 (
    
) `0.0`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/jm_simd.h`

