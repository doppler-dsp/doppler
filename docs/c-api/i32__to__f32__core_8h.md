

# File i32\_to\_f32\_core.h



[**FileList**](files.md) **>** [**i32\_to\_f32**](dir_3ce16833ebcc9c0a9fe9c8f4deb663cc.md) **>** [**i32\_to\_f32\_core.h**](i32__to__f32__core_8h.md)

[Go to the source code of this file](i32__to__f32__core_8h_source.md)

_int32-to-float converter with configurable inverse scale._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) <br>_I32ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) \* | [**i32\_to\_f32\_create**](#function-i32_to_f32_create) (float scale) <br>_Create a i32\_to\_f32 instance._  |
|  void | [**i32\_to\_f32\_destroy**](#function-i32_to_f32_destroy) ([**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) \* state) <br>_Destroy a i32\_to\_f32 instance and release all memory._  |
|  void | [**i32\_to\_f32\_reset**](#function-i32_to_f32_reset) ([**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) \* state) <br>_Reset I32ToF32 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**i32\_to\_f32\_step**](#function-i32_to_f32_step) (const [**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) \* state, int32\_t x) <br>_Process one input sample._  |
|  void | [**i32\_to\_f32\_steps**](#function-i32_to_f32_steps) ([**i32\_to\_f32\_state\_t**](structi32__to__f32__state__t.md) \* state, const int32\_t \* input, float \* output, size\_t n) <br>_Process a block of int32 samples to float32._  |




























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




### function i32\_to\_f32\_create 

_Create a i32\_to\_f32 instance._ 
```C++
i32_to_f32_state_t * i32_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale`. Any non-zero finite float is a valid scale.




**Parameters:**


* `scale` Denominator scale; 1/scale is applied to each sample (default: 2147483648.0f). Use 2^31 to recover normalised floats from a full-range int32 stream. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**i32\_to\_f32\_destroy()**](i32__to__f32__core_8h.md#function-i32_to_f32_destroy) when done. 





        

<hr>



### function i32\_to\_f32\_destroy 

_Destroy a i32\_to\_f32 instance and release all memory._ 
```C++
void i32_to_f32_destroy (
    i32_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function i32\_to\_f32\_reset 

_Reset I32ToF32 to its post-create state._ 
```C++
void i32_to_f32_reset (
    i32_to_f32_state_t * state
) 
```



No mutable state exists beyond the immutable `iscale`; reset is a no-op provided for lifecycle symmetry with other converters.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function i32\_to\_f32\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT float i32_to_f32_step (
    const i32_to_f32_state_t * state,
    int32_t x
) 
```



Returns ``(float)x \* iscale.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Signed int32 input sample. 



**Returns:**

Scaled float32 output. 





        

<hr>



### function i32\_to\_f32\_steps 

_Process a block of int32 samples to float32._ 
```C++
void i32_to_f32_steps (
    i32_to_f32_state_t * state,
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
>>> I32ToF32().steps(np.array([0, 2**30, -2**31], dtype=np.int32)).tolist()
[0.0, 0.5, -1.0]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/i32_to_f32/i32_to_f32_core.h`

