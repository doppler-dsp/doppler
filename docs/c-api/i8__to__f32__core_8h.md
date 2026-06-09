

# File i8\_to\_f32\_core.h



[**FileList**](files.md) **>** [**i8\_to\_f32**](dir_fd8e995fbd9a7d674714f99e992f90b2.md) **>** [**i8\_to\_f32\_core.h**](i8__to__f32__core_8h.md)

[Go to the source code of this file](i8__to__f32__core_8h_source.md)

_int8-to-float converter with configurable inverse scale._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) <br>_I8ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) \* | [**i8\_to\_f32\_create**](#function-i8_to_f32_create) (float scale) <br>_Create a i8\_to\_f32 instance._  |
|  void | [**i8\_to\_f32\_destroy**](#function-i8_to_f32_destroy) ([**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) \* state) <br>_Destroy a i8\_to\_f32 instance and release all memory._  |
|  void | [**i8\_to\_f32\_reset**](#function-i8_to_f32_reset) ([**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) \* state) <br>_Reset I8ToF32 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**i8\_to\_f32\_step**](#function-i8_to_f32_step) (const [**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) \* state, int8\_t x) <br>_Process one input sample._  |
|  void | [**i8\_to\_f32\_steps**](#function-i8_to_f32_steps) ([**i8\_to\_f32\_state\_t**](structi8__to__f32__state__t.md) \* state, const int8\_t \* input, float \* output, size\_t n) <br>_Process a block of int8 samples to float32._  |




























## Detailed Description


Multiplies each signed int8 sample by `1/scale` and returns a float32. The default scale of 128.0 maps the full int8 range [-128, 127] to [-1.0, ~+1.0), which is the natural inverse of an 8-bit ADC path. This converter is used in the 8-bit IQ sample pipeline (e.g., RTL-SDR signed-8 I/Q streams) where samples arrive as int8 and must be converted to normalised complex floats. The inverse scale is pre-computed at construction time.


Lifecycle: create -&gt; [step / steps / reset]\* -&gt; destroy



```C++
>>> from doppler.cvt import I8ToF32
>>> import numpy as np
>>> obj = I8ToF32(scale=128.0)
>>> float(obj.step(-128))
-1.0
>>> float(obj.step(0))
0.0
>>> x = np.array([-128, 0, 127], dtype=np.int8)
>>> [round(v, 7) for v in obj.steps(x).tolist()]
[-1.0, 0.0, 0.9921875]
```
 


    
## Public Functions Documentation




### function i8\_to\_f32\_create 

_Create a i8\_to\_f32 instance._ 
```C++
i8_to_f32_state_t * i8_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale`.




**Parameters:**


* `scale` Denominator scale; 1/scale is applied to each sample (default: 128.0f). Use 128.0 to recover normalised floats from a signed 8-bit stream. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**i8\_to\_f32\_destroy()**](i8__to__f32__core_8h.md#function-i8_to_f32_destroy) when done. 





        

<hr>



### function i8\_to\_f32\_destroy 

_Destroy a i8\_to\_f32 instance and release all memory._ 
```C++
void i8_to_f32_destroy (
    i8_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function i8\_to\_f32\_reset 

_Reset I8ToF32 to its post-create state._ 
```C++
void i8_to_f32_reset (
    i8_to_f32_state_t * state
) 
```



No mutable state exists beyond the immutable `iscale`; reset is a no-op provided for lifecycle symmetry with other converters.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function i8\_to\_f32\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT float i8_to_f32_step (
    const i8_to_f32_state_t * state,
    int8_t x
) 
```



Returns ``(float)x \* iscale.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Signed int8 input sample. 



**Returns:**

Scaled float32 output. 





        

<hr>



### function i8\_to\_f32\_steps 

_Process a block of int8 samples to float32._ 
```C++
void i8_to_f32_steps (
    i8_to_f32_state_t * state,
    const int8_t * input,
    float * output,
    size_t n
) 
```



Applies step() to every element. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input int8 array; must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples to process. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/i8_to_f32/i8_to_f32_core.h`

