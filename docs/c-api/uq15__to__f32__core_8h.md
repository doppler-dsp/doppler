

# File uq15\_to\_f32\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**uq15\_to\_f32**](dir_b44b8aae78dd39801a4344596faf709f.md) **>** [**uq15\_to\_f32\_core.h**](uq15__to__f32__core_8h.md)

[Go to the source code of this file](uq15__to__f32__core_8h_source.md)

_UQ15 (offset-binary uint16) to float converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) <br>_UQ15ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) \* | [**uq15\_to\_f32\_create**](#function-uq15_to_f32_create) (float scale) <br>_Create a uq15\_to\_f32 instance._  |
|  void | [**uq15\_to\_f32\_destroy**](#function-uq15_to_f32_destroy) ([**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) \* state) <br>_Destroy a uq15\_to\_f32 instance and release all memory._  |
|  void | [**uq15\_to\_f32\_reset**](#function-uq15_to_f32_reset) ([**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) \* state) <br>_Reset uq15\_to\_f32 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**uq15\_to\_f32\_step**](#function-uq15_to_f32_step) (const [**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) \* state, uint16\_t x) <br>_Process one input sample._  |
|  void | [**uq15\_to\_f32\_steps**](#function-uq15_to_f32_steps) ([**uq15\_to\_f32\_state\_t**](structuq15__to__f32__state__t.md) \* state, const uint16\_t \* input, float \* output, size\_t n) <br>_Process a block of UQ15 samples to float32._  |




























## Detailed Description


Decodes an offset-binary uint16 (UQ15) sample back to a normalised float by removing the +32768 bias and dividing by `scale:` 



```C++
x̂ = ((int32_t)u - 32768) * (1 / scale)
```



This is the exact inverse of F32ToUQ15 with the same scale. The bias removal uses int32\_t arithmetic to avoid signed overflow for the u=0 (full-negative) case. The inverse scale is pre-computed at construction time so the step path is a single subtract and multiply.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import UQ15ToF32
>>> import numpy as np
>>> obj = UQ15ToF32(scale=32768.0)
>>> float(obj.step(32768))
0.0
>>> float(obj.step(0))
-1.0
>>> x = np.array([0, 32768, 65535], dtype=np.uint16)
>>> [round(v, 6) for v in obj.steps(x).tolist()]
[-1.0, 0.0, 0.999969]
```
 


    
## Public Functions Documentation




### function uq15\_to\_f32\_create 

_Create a uq15\_to\_f32 instance._ 
```C++
uq15_to_f32_state_t * uq15_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale` so the hot step path is a single subtract and multiply.




**Parameters:**


* `scale` Denominator applied after offset-binary bias removal (default: 32768.0f). Use 32768.0 to recover normalised `[-1, +1]` floats from UQ15 data written by F32ToUQ15. Must be &gt; 0; returns NULL otherwise. 



**Returns:**

Heap-allocated state, or NULL on invalid args or allocation failure. 




**Note:**

Caller must call [**uq15\_to\_f32\_destroy()**](uq15__to__f32__core_8h.md#function-uq15_to_f32_destroy) when done. 





        

<hr>



### function uq15\_to\_f32\_destroy 

_Destroy a uq15\_to\_f32 instance and release all memory._ 
```C++
void uq15_to_f32_destroy (
    uq15_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function uq15\_to\_f32\_reset 

_Reset uq15\_to\_f32 to its post-create state._ 
```C++
void uq15_to_f32_reset (
    uq15_to_f32_state_t * state
) 
```



No mutable state exists beyond the immutable `iscale`; reset is a no-op provided for lifecycle symmetry with other converters.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function uq15\_to\_f32\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT float uq15_to_f32_step (
    const uq15_to_f32_state_t * state,
    uint16_t x
) 
```



Computes ``((int32\_t)x - 32768) \* iscale. The int32\_t cast prevents signed overflow when `x` is 0 (which yields -32768 after bias removal).




**Parameters:**


* `state` Must be non-NULL. 
* `x` UQ15 offset-binary uint16 sample: 0x0000 → -1.0f, 0x8000 → 0.0f, 0xFFFF → +32767/32768. 



**Returns:**

Decoded float sample in `[-1.0, ~+1.0)`. 





        

<hr>



### function uq15\_to\_f32\_steps 

_Process a block of UQ15 samples to float32._ 
```C++
void uq15_to_f32_steps (
    uq15_to_f32_state_t * state,
    const uint16_t * input,
    float * output,
    size_t n
) 
```



Applies step() to every element. State is not mutated (no clipped flag). Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input uint16 offset-binary array; must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import UQ15ToF32
>>> import numpy as np
>>> UQ15ToF32().steps(np.array([0, 32768], dtype=np.uint16)).tolist()
[-1.0, 0.0]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/uq15_to_f32/uq15_to_f32_core.h`

