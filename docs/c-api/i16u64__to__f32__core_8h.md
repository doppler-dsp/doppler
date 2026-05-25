

# File i16u64\_to\_f32\_core.h



[**FileList**](files.md) **>** [**i16u64\_to\_f32**](dir_8835689c72c9893bedb52cd5868912e0.md) **>** [**i16u64\_to\_f32\_core.h**](i16u64__to__f32__core_8h.md)

[Go to the source code of this file](i16u64__to__f32__core_8h_source.md)

_I16U64ToF32 component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) <br>_I16U64ToF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) \* | [**i16u64\_to\_f32\_create**](#function-i16u64_to_f32_create) (float scale) <br>_Create a i16u64\_to\_f32 instance._  |
|  void | [**i16u64\_to\_f32\_destroy**](#function-i16u64_to_f32_destroy) ([**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) \* state) <br>_Destroy a i16u64\_to\_f32 instance and release all memory._  |
|  void | [**i16u64\_to\_f32\_reset**](#function-i16u64_to_f32_reset) ([**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) \* state) <br>_Reset i16u64\_to\_f32 to its post-create state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float | [**i16u64\_to\_f32\_step**](#function-i16u64_to_f32_step) (const [**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) \* state, uint64\_t x) <br>_Process one input sample._  |
|  void | [**i16u64\_to\_f32\_steps**](#function-i16u64_to_f32_steps) ([**i16u64\_to\_f32\_state\_t**](structi16u64__to__f32__state__t.md) \* state, const uint64\_t \* input, float \* output, size\_t n) <br>_Process a block of samples._  |




























## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
i16u64_to_f32_state_t *obj = i16u64_to_f32_create(32768.0f);
float y = i16u64_to_f32_step(obj, 0U);
i16u64_to_f32_destroy(obj);
```
 


    
## Public Functions Documentation




### function i16u64\_to\_f32\_create 

_Create a i16u64\_to\_f32 instance._ 
```C++
i16u64_to_f32_state_t * i16u64_to_f32_create (
    float scale
) 
```





**Parameters:**


* `scale` scale (default: 32768.0f). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**i16u64\_to\_f32\_destroy()**](i16u64__to__f32__core_8h.md#function-i16u64_to_f32_destroy) when done. 





        

<hr>



### function i16u64\_to\_f32\_destroy 

_Destroy a i16u64\_to\_f32 instance and release all memory._ 
```C++
void i16u64_to_f32_destroy (
    i16u64_to_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function i16u64\_to\_f32\_reset 

_Reset i16u64\_to\_f32 to its post-create state._ 
```C++
void i16u64_to_f32_reset (
    i16u64_to_f32_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function i16u64\_to\_f32\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT float i16u64_to_f32_step (
    const i16u64_to_f32_state_t * state,
    uint64_t x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (uint64\_t). 



**Returns:**

Output sample (float). 





        

<hr>



### function i16u64\_to\_f32\_steps 

_Process a block of samples._ 
```C++
void i16u64_to_f32_steps (
    i16u64_to_f32_state_t * state,
    const uint64_t * input,
    float * output,
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
The documentation for this class was generated from the following file `native/inc/i16u64_to_f32/i16u64_to_f32_core.h`

