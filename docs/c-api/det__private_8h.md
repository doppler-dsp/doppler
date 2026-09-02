

# File det\_private.h



[**FileList**](files.md) **>** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md) **>** [**det\_private.h**](det__private_8h.md)

[Go to the source code of this file](det__private_8h_source.md)

_Shared internals for detector\_core.c and detector2d\_core.c._ [More...](#detailed-description)

* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_peak\_t**](structdet__peak__t.md) <br> |
























## Public Static Functions

| Type | Name |
| ---: | :--- |
|  int | [**det\_cmp\_f32\_asc**](#function-det_cmp_f32_asc) (const void \* a, const void \* b) <br> |
|  float | [**det\_noise\_estimate**](#function-det_noise_estimate) (const float \* mag, size\_t lo, size\_t hi, float \* scratch, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) mode) <br>_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._  |
|  size\_t | [**det\_peak\_list**](#function-det_peak_list) (const float \* surf, size\_t ny, size\_t nx, float gate, size\_t excl\_rows, size\_t excl\_cols, uint8\_t \* mask, [**det\_peak\_t**](structdet__peak__t.md) \* out, size\_t max\_peaks) <br>_The maximum of a surface, iterated with exclusion zones: every peak above a gate, strongest first, at most_ `max_peaks` _of them._ |
|  dp\_f32\_t \* | [**det\_ring\_create**](#function-det_ring_create) (size\_t cap\_min) <br> |
|  size\_t | [**next\_pow2**](#function-next_pow2) (size\_t n) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DET\_PEAK\_T\_DEFINED**](det__private_8h.md#define-det_peak_t_defined)  <br> |

## Detailed Description


Not part of the public API. Include after the module's own header so that det\_noise\_mode\_t is already defined via the DET\_NOISE\_MODE\_T\_DEFINED guard in [**detector\_core.h**](detector__core_8h.md) / [**detector2d\_core.h**](detector2d__core_8h.md). 


    
## Public Static Functions Documentation




### function det\_cmp\_f32\_asc 

```C++
static int det_cmp_f32_asc (
    const void * a,
    const void * b
) 
```




<hr>



### function det\_noise\_estimate 

_Aggregate \|corr\| over bins &#91;lo, hi&#93; using the selected mode._ 
```C++
static float det_noise_estimate (
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



### function det\_peak\_list 

_The maximum of a surface, iterated with exclusion zones: every peak above a gate, strongest first, at most_ `max_peaks` _of them._
```C++
static size_t det_peak_list (
    const float * surf,
    size_t ny,
    size_t nx,
    float gate,
    size_t excl_rows,
    size_t excl_cols,
    uint8_t * mask,
    det_peak_t * out,
    size_t max_peaks
) 
```



The one argmax under both detectors (docs/design/async-dsss-receiver.md §7.1, §8 (a)). Each pick is the largest unmasked cell; if it is not above `gate` the list ends there (the gate is `eta` in the surface's own units, so a second peak is another draw from the same cells against the same union bound  the threshold does not change with the list). A pick's zone  `excl_rows` either side along the rows and `excl_cols` along the columns, CIRCULAR on both axes, since every surface this serves is an FFT bin axis by a circular correlation lag axis  is masked so the emitter just reported cannot be reported again from its own shoulders; outside the zone a second emitter has its own maximum. The zone is therefore the detector's resolution, and it is the caller's to size from the code and the dwell (one Doppler bin by one chip: the main lobe's first nulls).


`mask` is the caller's, `ny * nx` bytes, initialised by the caller: 0 for a candidate cell, non-zero for one that is never a candidate (a Doppler band the engine does not search). On return every listed peak's zone is marked as well. Nothing here allocates, and the cost is `max_peaks` scans of the surface plus the zones  the duration rule of §5.1.




**Parameters:**


* `surf` The surface, row-major `ny x nx`. 
* `ny` Its geometry. 
* `gate` A peak must exceed this (strictly) to be listed. 
* `excl_rows` Zone half-width along rows (0 = the row alone). 
* `excl_cols` Zone half-width along columns (0 = the column alone). 
* `mask` `ny * nx` bytes, 0 = candidate; updated in place. 
* `out` Receives up to `max_peaks` peaks, strongest first. 
* `max_peaks` Capacity of `out`. 



**Returns:**

Peaks listed (0 when nothing exceeds the gate). 





        

<hr>



### function det\_ring\_create 

```C++
static dp_f32_t * det_ring_create (
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
## Macro Definition Documentation





### define DET\_PEAK\_T\_DEFINED 

```C++
#define DET_PEAK_T_DEFINED 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector/det_private.h`

