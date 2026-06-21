

# File f32\_to\_uq15\_core.h



[**FileList**](files.md) **>** [**f32\_to\_uq15**](dir_4e8c99e54919bb49218552fb8f2fb678.md) **>** [**f32\_to\_uq15\_core.h**](f32__to__uq15__core_8h.md)

[Go to the source code of this file](f32__to__uq15__core_8h_source.md)

_Scale-and-saturate float-to-UQ15 (offset-binary uint16) converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) <br>_F32ToUQ15 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) \* | [**f32\_to\_uq15\_create**](#function-f32_to_uq15_create) (float scale) <br>_Create a f32\_to\_uq15 instance._  |
|  void | [**f32\_to\_uq15\_destroy**](#function-f32_to_uq15_destroy) ([**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) \* state) <br>_Destroy a f32\_to\_uq15 instance and release all memory._  |
|  void | [**f32\_to\_uq15\_reset**](#function-f32_to_uq15_reset) ([**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) \* state) <br>_Reset f32\_to\_uq15 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) uint16\_t | [**f32\_to\_uq15\_step**](#function-f32_to_uq15_step) ([**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) \* state, float x) <br>_Process one input sample._  |
|  void | [**f32\_to\_uq15\_steps**](#function-f32_to_uq15_steps) ([**f32\_to\_uq15\_state\_t**](structf32__to__uq15__state__t.md) \* state, const float \* input, uint16\_t \* output, size\_t n) <br>_Process a block of float samples to UQ15 uint16._  |




























## Detailed Description


Converts a normalised float sample to offset-binary uint16 (UQ15 format). The Q15 quantised value is biased by +32768 so that the full unsigned range maps to the signed float domain: -1.0 → uint16 0 (0x0000) 0.0 → uint16 32768 (0x8000) +1.0 → uint16 65535 (0xFFFF)


Encoding: 
```C++
v_Q15 = clamp(round(x * scale), -32768, 32767)
u     = (uint16_t)((int32_t)v_Q15 + 32768)
```



This is the unsigned wire format used by some DAC and file-container conventions that cannot represent negative integer values. UQ15ToF32 performs the exact inverse. A sticky `clipped` flag is raised on saturation and cleared only by reset().


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import F32ToUQ15
>>> import numpy as np
>>> obj = F32ToUQ15(scale=32768.0)
>>> obj.step(0.0)
32768
>>> obj.step(-1.0)
0
>>> obj.clipped
False
>>> obj.step(1.0)
65535
>>> obj.clipped
True
>>> obj.reset()
>>> x = np.array([-1.0, 0.0, 1.0], dtype=np.float32)
>>> obj.steps(x).tolist()
[0, 32768, 65535]
```
 


    
## Public Functions Documentation




### function f32\_to\_uq15\_create 

_Create a f32\_to\_uq15 instance._ 
```C++
f32_to_uq15_state_t * f32_to_uq15_create (
    float scale
) 
```



Stores `scale` and initialises the sticky `clipped` flag to 0.




**Parameters:**


* `scale` Multiply factor applied before quantisation and saturation (default: 32768.0f). Use 32768.0 to convert normalised `[-1, +1]` floats to the full UQ15 range `[0, 65535]`. Must be &gt; 0; returns NULL otherwise. 



**Returns:**

Heap-allocated state, or NULL on invalid args or allocation failure. 




**Note:**

Caller must call [**f32\_to\_uq15\_destroy()**](f32__to__uq15__core_8h.md#function-f32_to_uq15_destroy) when done. 





        

<hr>



### function f32\_to\_uq15\_destroy 

_Destroy a f32\_to\_uq15 instance and release all memory._ 
```C++
void f32_to_uq15_destroy (
    f32_to_uq15_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function f32\_to\_uq15\_reset 

_Reset f32\_to\_uq15 to its post-create state._ 
```C++
void f32_to_uq15_reset (
    f32_to_uq15_state_t * state
) 
```



Clears the sticky `clipped` flag. The `scale` is preserved.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function f32\_to\_uq15\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT uint16_t f32_to_uq15_step (
    f32_to_uq15_state_t * state,
    float x
) 
```



Computes `round(x * scale)`, clamps to `[-32768, 32767]`, then adds 32768 to produce the offset-binary uint16 result. Sets `clipped` if saturation occurred before clamping.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Normalised float input sample. 



**Returns:**

Offset-binary uint16 in `[0, 65535]`: x = -1.0 → 0x0000, x = 0.0 → 0x8000, x ≈ +1.0 → 0xFFFF. 





        

<hr>



### function f32\_to\_uq15\_steps 

_Process a block of float samples to UQ15 uint16._ 
```C++
void f32_to_uq15_steps (
    f32_to_uq15_state_t * state,
    const float * input,
    uint16_t * output,
    size_t n
) 
```



Applies step() to every element. The `clipped` flag is updated cumulatively across the block. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output uint16 offset-binary array; must contain at least `n` elements. 
* `n` Number of samples to process. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_uq15/f32_to_uq15_core.h`

