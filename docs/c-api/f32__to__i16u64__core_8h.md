

# File f32\_to\_i16u64\_core.h



[**FileList**](files.md) **>** [**f32\_to\_i16u64**](dir_212e21299d76aa740bbad8810e4bf50a.md) **>** [**f32\_to\_i16u64\_core.h**](f32__to__i16u64__core_8h.md)

[Go to the source code of this file](f32__to__i16u64__core_8h_source.md)

_Scale-and-saturate float to Q15-in-uint64 converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) <br>_F32ToI16U64 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* | [**f32\_to\_i16u64\_create**](#function-f32_to_i16u64_create) (float scale) <br>_Create a f32\_to\_i16u64 instance._  |
|  void | [**f32\_to\_i16u64\_destroy**](#function-f32_to_i16u64_destroy) ([**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state) <br>_Destroy a f32\_to\_i16u64 instance and release all memory._  |
|  void | [**f32\_to\_i16u64\_reset**](#function-f32_to_i16u64_reset) ([**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state) <br>_Reset f32\_to\_i16u64 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) uint64\_t | [**f32\_to\_i16u64\_step**](#function-f32_to_i16u64_step) ([**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state, float x) <br>_Process one input sample._  |
|  void | [**f32\_to\_i16u64\_steps**](#function-f32_to_i16u64_steps) ([**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state, const float \* input, uint64\_t \* output, size\_t n) <br>_Process a block of float samples to Q15-in-uint64._  |




























## Detailed Description


Identical semantics to F32ToI16U32 but the zero-extended Q15 result occupies the lower 16 bits of a uint64, providing 48 bits of upper headroom. This is the input format for the NCO's uint64 phase accumulator pipeline, where the upper bits carry phase increment headroom across accumulations.


input +1.0 → int16 32767 → uint64 0x0000000000007FFF input -1.0 → int16 -32768 → uint64 0x0000000000008000


The default scale of 32768.0 maps `[-1, +1]` float to Q15 range. A sticky `clipped` flag is raised on saturation and cleared only by reset().


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy



```C++
>>> from doppler.cvt import F32ToI16U64
>>> import numpy as np
>>> obj = F32ToI16U64(scale=32768.0)
>>> hex(obj.step(-1.0))
'0x8000'
>>> hex(obj.step(1.0))
'0x7fff'
>>> obj.step(0.0)
0
>>> x = np.array([-1.0, 0.0, 1.0], dtype=np.float32)
>>> obj.steps(x).tolist()
[32768, 0, 32767]
```
 


    
## Public Functions Documentation




### function f32\_to\_i16u64\_create 

_Create a f32\_to\_i16u64 instance._ 
```C++
f32_to_i16u64_state_t * f32_to_i16u64_create (
    float scale
) 
```



Stores `scale` and initialises the sticky `clipped` flag to 0.




**Parameters:**


* `scale` Multiply factor applied before quantisation and saturation (default: 32768.0f). Use 32768.0 to convert normalised `[-1, +1]` samples to Q15 packed into the low 16 bits of a uint64. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**f32\_to\_i16u64\_destroy()**](f32__to__i16u64__core_8h.md#function-f32_to_i16u64_destroy) when done. 





        

<hr>



### function f32\_to\_i16u64\_destroy 

_Destroy a f32\_to\_i16u64 instance and release all memory._ 
```C++
void f32_to_i16u64_destroy (
    f32_to_i16u64_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function f32\_to\_i16u64\_reset 

_Reset f32\_to\_i16u64 to its post-create state._ 
```C++
void f32_to_i16u64_reset (
    f32_to_i16u64_state_t * state
) 
```



Clears the sticky `clipped` flag. The `scale` is preserved.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function f32\_to\_i16u64\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT uint64_t f32_to_i16u64_step (
    f32_to_i16u64_state_t * state,
    float x
) 
```



Computes `round(x * scale)`, saturates to `[-32768, 32767]`, then zero-extends the int16 bit pattern into the lower 16 bits of a uint64. The `clipped` flag is set if saturation occurred.




**Parameters:**


* `state` Must be non-NULL. 
* `x` Normalised float input sample. 



**Returns:**

Q15 value packed into the lower 16 bits of a uint64. 





        

<hr>



### function f32\_to\_i16u64\_steps 

_Process a block of float samples to Q15-in-uint64._ 
```C++
void f32_to_i16u64_steps (
    f32_to_i16u64_state_t * state,
    const float * input,
    uint64_t * output,
    size_t n
) 
```



Applies step() to every element. The `clipped` flag is updated cumulatively across the block. Accepts an optional pre-allocated output array; allocates a fresh one when `output` is NULL.




**Parameters:**


* `state` Must be non-NULL. 
* `input` Input float32 array; must contain at least `n` elements. 
* `output` Output uint64 array; must contain at least `n` elements. 
* `n` Number of samples to process. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_i16u64/f32_to_i16u64_core.h`

