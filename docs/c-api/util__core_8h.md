

# File util\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**util**](dir_301ecbdb0604927cf0b3895ddfaba60f.md) **>** [**util\_core.h**](util__core_8h.md)

[Go to the source code of this file](util__core_8h_source.md)

_Util module — public C API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) float complex | [**square\_clip**](#function-square_clip) (float complex y, float lin) <br>_Square-clip a complex sample: clip the real and imaginary parts independently to [-lin, lin] (a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._  |


























## Detailed Description


The util functions are header-only and JM\_FORCEINLINE: any caller that includes this header inlines them with zero call overhead, and the util Python extension module exposes the very same definitions. There is one source of truth per function, here. 


    
## Public Static Functions Documentation




### function square\_clip 

_Square-clip a complex sample: clip the real and imaginary parts independently to [-lin, lin] (a square region in the IQ plane, not a circular magnitude limit). Each component is passed through unchanged when its magnitude is within the threshold and clamped to the nearest boundary otherwise._ 
```C++
static JM_FORCEINLINE float complex square_clip (
    float complex y,
    float lin
) 
```





**Parameters:**


* `y` Complex CF32 input sample. 
* `lin` Per-component clip threshold (linear amplitude, &gt;= 0). Values outside [-lin, lin] are clamped; values on the boundary are preserved exactly. 



**Returns:**

Sample with each component limited to [-lin, lin]. 
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

