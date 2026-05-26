

# File f32\_to\_i16\_core.h



[**FileList**](files.md) **>** [**f32\_to\_i16**](dir_e25c96329f88166d8f87eefdc2ba64fa.md) **>** [**f32\_to\_i16\_core.h**](f32__to__i16__core_8h.md)

[Go to the source code of this file](f32__to__i16__core_8h_source.md)

_Scale-and-saturate float-to-int16 converter._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include <math.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) <br>_F32ToI16 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* | [**f32\_to\_i16\_create**](#function-f32_to_i16_create) (float scale) <br>_Create a f32\_to\_i16 instance._  |
|  void | [**f32\_to\_i16\_destroy**](#function-f32_to_i16_destroy) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state) <br>_Destroy a f32\_to\_i16 instance and release all memory._  |
|  void | [**f32\_to\_i16\_reset**](#function-f32_to_i16_reset) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state) <br>_Reset f32\_to\_i16 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) int16\_t | [**f32\_to\_i16\_step**](#function-f32_to_i16_step) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, float x) <br>_Process one input sample._  |
|  void | [**f32\_to\_i16\_steps**](#function-f32_to_i16_steps) ([**f32\_to\_i16\_state\_t**](structf32__to__i16__state__t.md) \* state, const float \* input, int16\_t \* output, size\_t n) <br>_Process a block of samples._  |




























## Detailed Description


Multiplies by `scale` then round-to-nearest and clamps to `[-32768, 32767]`. The default scale of 32768.0 converts a `[-1, +1]` normalised float to full Q15 range.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
f32_to_i16_state_t *obj = f32_to_i16_create(32768.0f);
int16_t y = f32_to_i16_step(obj, 1.0f);  // y == 32767
f32_to_i16_destroy(obj);
```
 


    
## Public Functions Documentation




### function f32\_to\_i16\_create 

_Create a f32\_to\_i16 instance._ 
```C++
f32_to_i16_state_t * f32_to_i16_create (
    float scale
) 
```





**Parameters:**


* `scale` scale (default: 32768.0f). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**f32\_to\_i16\_destroy()**](f32__to__i16__core_8h.md#function-f32_to_i16_destroy) when done. 





        

<hr>



### function f32\_to\_i16\_destroy 

_Destroy a f32\_to\_i16 instance and release all memory._ 
```C++
void f32_to_i16_destroy (
    f32_to_i16_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function f32\_to\_i16\_reset 

_Reset f32\_to\_i16 to its post-create state._ 
```C++
void f32_to_i16_reset (
    f32_to_i16_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function f32\_to\_i16\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT int16_t f32_to_i16_step (
    f32_to_i16_state_t * state,
    float x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (float). 



**Returns:**

Output sample (int16\_t). 





        

<hr>



### function f32\_to\_i16\_steps 

_Process a block of samples._ 
```C++
void f32_to_i16_steps (
    f32_to_i16_state_t * state,
    const float * input,
    int16_t * output,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). 
* `input` Input array (length &gt;= n). 
* `output` Output array (length &gt;= n; may alias input for in-place). 
* `n` Number of samples. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_i16/f32_to_i16_core.h`

