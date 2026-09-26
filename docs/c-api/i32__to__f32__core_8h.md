

# File i32\_to\_f32\_core.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**i32\_to\_f32**](dir_04852aba3b033ffe951e6f7f0437e86c.md) **>** [**i32\_to\_f32\_core.h**](i32__to__f32__core_8h.md)

[Go to the source code of this file](i32__to__f32__core_8h_source.md)

_int32-to-float converter with configurable inverse scale._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) <br>_I32ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) \* | [**dp\_i32\_to\_f32\_create**](#function-dp_i32_to_f32_create) (float scale) <br>_Create a i32\_to\_f32 instance._  |
|  void | [**dp\_i32\_to\_f32\_destroy**](#function-dp_i32_to_f32_destroy) ([**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) \* state) <br>_Destroy a i32\_to\_f32 instance and release all memory._  |
|  void | [**dp\_i32\_to\_f32\_reset**](#function-dp_i32_to_f32_reset) ([**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) \* state) <br>_No-op reset, provided only for lifecycle symmetry._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**dp\_i32\_to\_f32\_step**](#function-dp_i32_to_f32_step) (const [**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) \* state, int32\_t x) <br>_Convert one signed int32 sample to a normalised float via_ `1/scale` _._ |
|  void | [**dp\_i32\_to\_f32\_steps**](#function-dp_i32_to_f32_steps) ([**dp\_i32\_to\_f32\_state\_t**](structdp__i32__to__f32__state__t.md) \* state, const int32\_t \* input, float \* output, size\_t n) <br>_Process a block of int32 samples to float32._  |




























## Detailed Description


Multiplies each int32 sample by `1/scale` and returns a float32 result. The default scale of 2147483648.0 (2^31) maps the full int32 range `[-2147483648, 2147483647]` to `[-1.0, ~+1.0)`, recovering the normalised float representation from a 32-bit fixed-point stream. Note: float32 has 23 mantissa bits, so int32 values beyond ±16777217 will be rounded to the nearest representable float. Use I32ToF32 when only the magnitude matters or the source is genuinely 32-bit fixed-point. The inverse scale is pre-computed at construction time.


Lifecycle: create -&gt; `[step / steps / reset]*` -&gt; destroy



```C++
>>> from doppler.cvt import I32ToF32
>>> import numpy as np
>>> obj = I32ToF32(scale=2147483648.0)
>>> float(obj.step(-2147483648))
-1.0
>>> float(obj.step(0))
0.0
>>> x = np.array([-2147483648, 0, 2147483647], dtype=np.int32)
>>> obj.steps(x).tolist()
[-1.0, 0.0, 1.0]
```
 


    
## Public Functions Documentation




### function dp\_i32\_to\_f32\_create 

_Create a i32\_to\_f32 instance._ 
```C++
dp_i32_to_f32_state_t * dp_i32_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale`. Any non-zero finite float is a valid scale.




**Parameters:**


* `scale` Denominator scale; 1/scale is applied to each sample (default: 2147483648.0f). Use 2^31 to recover normalised floats from a full-range int32 stream. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**dp\_i32\_to\_f32\_destroy()**](i32__to__f32__core_8h.md#function-dp_i32_to_f32_destroy) when done. 





        

<hr>



### function dp\_i32\_to\_f32\_destroy 

_Destroy a i32\_to\_f32 instance and release all memory._ 
```C++
void dp_i32_to_f32_destroy (
    dp_i32_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_i32\_to\_f32\_reset 

_No-op reset, provided only for lifecycle symmetry._ 
```C++
void dp_i32_to_f32_reset (
    dp_i32_to_f32_state_t * state
) 
```



No mutable state exists beyond the immutable `iscale`, so there is nothing to clear; the method exists so every converter in the module presents the same create / step / reset / destroy lifecycle.




**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.cvt import I32ToF32
>>> c = I32ToF32()
>>> c.reset()           # stateless converter -> reset is a no-op
>>> round(c.step(-2**31), 4)
-1.0
```
 


        

<hr>



### function dp\_i32\_to\_f32\_step 

_Convert one signed int32 sample to a normalised float via_ `1/scale` _._
```C++
JM_FORCEINLINE  JM_HOT float dp_i32_to_f32_step (
    const dp_i32_to_f32_state_t * state,
    int32_t x
) 
```



Returns ``(float)x \* iscale, a single multiply on the hot path. At the default scale of 2^31 the full int32 range recovers `[-1.0, ~+1.0)`. Note that float32 carries only 23 mantissa bits, so int32 magnitudes beyond 2^24 are rounded to the nearest representable float.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Signed int32 code, normally a full-range fixed-point sample. 



**Returns:**

Normalised float, `x / scale`.



```C++
>>> from doppler.cvt import I32ToF32
>>> c = I32ToF32(scale=2147483648.0)  # 2**31: int32 -> [-1, 1)
>>> round(c.step(2**30), 4)            # quarter-scale code -> 0.5
0.5
>>> round(c.step(-2**31), 4)           # full-negative code -> -1.0
-1.0
```
 


        

<hr>



### function dp\_i32\_to\_f32\_steps 

_Process a block of int32 samples to float32._ 
```C++
void dp_i32_to_f32_steps (
    dp_i32_to_f32_state_t * state,
    const int32_t * input,
    float * output,
    size_t n
) 
```



Applies step() to every element. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input int32 array; must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import I32ToF32
>>> import numpy as np
>>> I32ToF32().steps(
...     np.array([0, 2**30, -2**31], dtype=np.int32)).tolist()
[0.0, 0.5, -1.0]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/i32_to_f32/i32_to_f32_core.h`

