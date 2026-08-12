

# File util\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md) **>** [**util\_core.h**](util__core_8h.md)

[Go to the source code of this file](util__core_8h_source.md)

_Util module — public C API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`





































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**saturate**](#function-saturate) (double v, double lo, double hi, double nan\_to) <br>_Saturate a value into_ `[lo, hi]` _,_**total over every double** _— including NaN and both infinities._ |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float complex | [**square\_clip**](#function-square_clip) (float complex y, float lin) <br>_Square-clip a complex sample: clip the real and imaginary parts independently to_ `[-lin, lin]` _(a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._ |




























## Detailed Description


The util functions are header-only and JM\_FORCEINLINE: any caller that includes this header inlines them with zero call overhead, and the util Python extension module exposes the very same definitions. There is one source of truth per function, here. 


    
## Public Functions Documentation




### function saturate 

_Saturate a value into_ `[lo, hi]` _,_**total over every double** _— including NaN and both infinities._
```C++
JM_FORCEINLINE double saturate (
    double v,
    double lo,
    double hi,
    double nan_to
) 
```



`fmin`/`fmax` are not enough for this job. A plain `fmin(fmax(v, lo), hi)` propagates NaN on some platforms and silently returns a bound on others, and a hand-written `v > hi ? hi : v` leaves NaN untouched, because every comparison against NaN is false. This function has no fall-through: a value that is neither inside the interval, nor below it, nor above it can only be NaN.




**
**

Which end is _safe_ is domain knowledge, not arithmetic. A gain control guarding a measured power wants NaN at the **ceiling** — an unknown level must drive the gain down, because too little gain loses a signal while too much rails everything downstream. A lock statistic wants NaN at the **floor** — an unknown lock is not a lock. Baking either choice in would hand the wrong default to half its callers, so `nan_to` is a parameter and each call site states its own safe direction.




**
**

At the boundary where an untrusted value first becomes **persistent state** — the input of an EMA, an accumulator, or an integrator. Ahead of that boundary a bad value corrupts one output and is gone; past it, it is remembered and every quantity derived from it inherits the damage. One guard there makes the whole downstream chain total, where a clamp at each stage is several chances to miss one.




**Parameters:**


* `v` Value to saturate. Any double. 
* `lo` Lower bound, returned for any `v < lo`. 
* `hi` Upper bound, returned for any `v > hi`. 
* `nan_to` Returned when `v` is NaN. Pick the end that is safe in the caller's own terms; it is usually `lo` or `hi`. 



**Returns:**

`v` when `lo <= v <= hi`, otherwise `lo`, `hi` or `nan_to`. 
```C++
>>> from doppler.util import saturate
>>> saturate(0.5, 0.0, 1.0, 1.0)     # inside the interval
0.5
>>> saturate(2.0, 0.0, 1.0, 1.0)     # above the ceiling
1.0
>>> saturate(-3.0, 0.0, 1.0, 1.0)    # below the floor
0.0
>>> saturate(float("inf"), 0.0, 1.0, 1.0)   # infinity is just above
1.0
>>> saturate(float("nan"), 0.0, 1.0, 1.0)   # NaN takes the caller's end
1.0
>>> saturate(float("nan"), 0.0, 1.0, 0.0)   # ... which may be the other
0.0
```
 





        

<hr>



### function square\_clip 

_Square-clip a complex sample: clip the real and imaginary parts independently to_ `[-lin, lin]` _(a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._
```C++
JM_FORCEINLINE float complex square_clip (
    float complex y,
    float lin
) 
```





**Parameters:**


* `y` Complex CF32 input sample. 
* `lin` Per-component clip threshold (linear amplitude, &gt;= 0). Values outside `[-lin, lin]` are clamped; values on the boundary are preserved exactly. 



**Returns:**

Sample with each component limited to `[-lin, lin]`. 
```C++
>>> from doppler.util import square_clip
>>> square_clip(0.5+0.25j, 1.0)   # within bounds, passed through
(0.5+0.25j)
>>> square_clip(2.0+0.5j, 1.0)    # real clipped, imag unchanged
(1+0.5j)
>>> square_clip(3.0-4.0j, 1.0)    # both components clipped
(1-1j)
>>> square_clip(0.5+0.5j, 0.25)   # smaller threshold clips both
(0.25+0.25j)
>>> square_clip(-2.0+0.0j, 1.0)   # negative real clipped
(-1+0j)
```
 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/util/util_core.h`

