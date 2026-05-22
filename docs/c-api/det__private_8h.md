

# File det\_private.h



[**FileList**](files.md) **>** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md) **>** [**det\_private.h**](det__private_8h.md)

[Go to the source code of this file](det__private_8h_source.md)

_Shared internals for detector\_core.c and detector2d\_core.c._ [More...](#detailed-description)

* `#include <stdlib.h>`
* `#include <string.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**\_cmp\_f32\_asc**](#function-_cmp_f32_asc) (const void \* a, const void \* b) <br> |
|  float | [**\_noise\_estimate**](#function-_noise_estimate) (const float \* mag, size\_t lo, size\_t hi, float \* scratch, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) mode) <br>_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._  |
|  dp\_f32\_t \* | [**\_ring\_create**](#function-_ring_create) (size\_t cap\_min) <br> |
|  size\_t | [**next\_pow2**](#function-next_pow2) (size\_t n) <br> |


























## Detailed Description


Not part of the public API. Include after the module's own header so that [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) is already defined via the DET\_NOISE\_MODE\_T\_DEFINED guard in [**detector\_core.h**](detector__core_8h.md) / [**detector2d\_core.h**](detector2d__core_8h.md). 


    
## Public Static Functions Documentation




### function \_cmp\_f32\_asc 

```C++
static int _cmp_f32_asc (
    const void * a,
    const void * b
) 
```




<hr>



### function \_noise\_estimate 

_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._ 
```C++
static float _noise_estimate (
    const float * mag,
    size_t lo,
    size_t hi,
    float * scratch,
    det_noise_mode_t mode
) 
```



Returns 0 if lo &gt; hi (empty range) — the caller maps that to test\_stat=0.




**Parameters:**


* `mag` Magnitude vector (length &gt;= hi+1). 
* `lo` First bin, inclusive. 
* `hi` Last bin, inclusive. 
* `scratch` Caller-allocated buffer of length &gt;= (hi-lo+1) floats; used only for DET\_NOISE\_MEDIAN (avoids a heap alloc per push). 
* `mode` Aggregation mode. 



**Returns:**

Aggregated noise estimate, or 0 if lo &gt; hi. 





        

<hr>



### function \_ring\_create 

```C++
static dp_f32_t * _ring_create (
    size_t cap_min
) 
```




<hr>



### function next\_pow2 

```C++
static size_t next_pow2 (
    size_t n
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector/det_private.h`

