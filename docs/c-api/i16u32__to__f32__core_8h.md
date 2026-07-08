

# File i16u32\_to\_f32\_core.h



[**FileList**](files.md) **>** [**i16u32\_to\_f32**](dir_a216b988e44f4b34f41ebc1122731aa5.md) **>** [**i16u32\_to\_f32\_core.h**](i16u32__to__f32__core_8h.md)

[Go to the source code of this file](i16u32__to__f32__core_8h_source.md)

_Q15-in-uint32 to float converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) <br>_I16U32ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) \* | [**i16u32\_to\_f32\_create**](#function-i16u32_to_f32_create) (float scale) <br>_Create a i16u32\_to\_f32 instance._  |
|  void | [**i16u32\_to\_f32\_destroy**](#function-i16u32_to_f32_destroy) ([**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) \* state) <br>_Destroy a i16u32\_to\_f32 instance and release all memory._  |
|  void | [**i16u32\_to\_f32\_reset**](#function-i16u32_to_f32_reset) ([**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) \* state) <br>_Reset i16u32\_to\_f32 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**i16u32\_to\_f32\_step**](#function-i16u32_to_f32_step) (const [**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) \* state, uint32\_t x) <br>_Process one input sample._  |
|  void | [**i16u32\_to\_f32\_steps**](#function-i16u32_to_f32_steps) ([**i16u32\_to\_f32\_state\_t**](structi16u32__to__f32__state__t.md) \* state, const uint32\_t \* input, float \* output, size\_t n) <br>_Process a block of Q15-in-uint32 samples to float32._  |




























## Detailed Description


Extracts the lower 16 bits of a uint32, re-interprets them as a signed int16 (two's complement), then multiplies by `1/scale` to produce a normalised float. This is the exact inverse of F32ToI16U32: a value written by that converter can be recovered here with the same scale.


uint32 0x00008000 → int16 -32768 → float -1.0 uint32 0x00007FFF → int16 32767 → float ~+1.0 uint32 0x00000000 → int16 0 → float 0.0


Upper 16 bits of the uint32 are masked off and ignored, so values carrying CIC bit-growth headroom in those bits are handled correctly.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import I16U32ToF32
>>> import numpy as np
>>> obj = I16U32ToF32(scale=32768.0)
>>> float(obj.step(0x8000))
-1.0
>>> float(obj.step(0x0000))
0.0
>>> x = np.array([0x8000, 0x0000, 0x7FFF], dtype=np.uint32)
>>> [round(v, 6) for v in obj.steps(x).tolist()]
[-1.0, 0.0, 0.999969]
```
 


    
## Public Functions Documentation




### function i16u32\_to\_f32\_create 

_Create a i16u32\_to\_f32 instance._ 
```C++
i16u32_to_f32_state_t * i16u32_to_f32_create (
    float scale
) 
```



Pre-computes `iscale` = 1.0f / `scale` so the hot step path is a single multiply after the lower-16-bit extraction.




**Parameters:**


* `scale` Denominator scale; 1/scale is applied after sign-extension (default: 32768.0f). Use 32768.0 to match F32ToI16U32 at its default scale. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**i16u32\_to\_f32\_destroy()**](i16u32__to__f32__core_8h.md#function-i16u32_to_f32_destroy) when done. 





        

<hr>



### function i16u32\_to\_f32\_destroy 

_Destroy a i16u32\_to\_f32 instance and release all memory._ 
```C++
void i16u32_to_f32_destroy (
    i16u32_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function i16u32\_to\_f32\_reset 

_Reset i16u32\_to\_f32 to its post-create state._ 
```C++
void i16u32_to_f32_reset (
    i16u32_to_f32_state_t * state
) 
```



No mutable state exists beyond the immutable `iscale`; reset is a no-op provided for lifecycle symmetry.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function i16u32\_to\_f32\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT float i16u32_to_f32_step (
    const i16u32_to_f32_state_t * state,
    uint32_t x
) 
```



Masks the lower 16 bits, sign-extends to int16, then multiplies by iscale. Upper 16 bits are ignored.




**Parameters:**


* `state` Must be non-NULL. 
* `x` uint32 carrying a Q15 sample in its lower 16 bits. 



**Returns:**

Scaled float32 output. 





        

<hr>



### function i16u32\_to\_f32\_steps 

_Process a block of Q15-in-uint32 samples to float32._ 
```C++
void i16u32_to_f32_steps (
    i16u32_to_f32_state_t * state,
    const uint32_t * input,
    float * output,
    size_t n
) 
```



Applies step() to every element. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input uint32 array (Q15 packed in lower 16 bits); must contain at least `n` elements. 
* `output` Output float32 array; must contain at least `n` elements. 
* `n` Number of samples to process.


```C++
>>> from doppler.cvt import I16U32ToF32
>>> import numpy as np
>>> I16U32ToF32().steps(np.array([0, 16384], dtype=np.uint32)).tolist()
[0.0, 0.5]
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/i16u32_to_f32/i16u32_to_f32_core.h`

