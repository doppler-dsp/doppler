

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
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) uint64\_t | [**f32\_to\_i16u64\_step**](#function-f32_to_i16u64_step) (const [**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state, float x) <br>_Process one input sample._  |
|  void | [**f32\_to\_i16u64\_steps**](#function-f32_to_i16u64_steps) ([**f32\_to\_i16u64\_state\_t**](structf32__to__i16u64__state__t.md) \* state, const float \* input, uint64\_t \* output, size\_t n) <br>_Process a block of samples._  |




























## Detailed Description


Identical semantics to F32ToI16U32 but the zero-extended result occupies the lower 16 bits of a uint64, providing 48 bits of upper headroom. This is the input format for the NCO's uint64 phase accumulator pipeline, where the upper bits carry phase increment headroom across accumulations.


input +1.0 → int16 32767 → uint64 0x0000000000007FFF input -1.0 → int16 -32768 → uint64 0x0000000000008000


The default scale of 32768.0 maps `[-1, +1]` float to Q15 range.


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
f32_to_i16u64_state_t *obj = f32_to_i16u64_create(32768.0f);
uint64_t y = f32_to_i16u64_step(obj, -1.0f);  // y == 0x0000000000008000
f32_to_i16u64_destroy(obj);
```
 


    
## Public Functions Documentation




### function f32\_to\_i16u64\_create 

_Create a f32\_to\_i16u64 instance._ 
```C++
f32_to_i16u64_state_t * f32_to_i16u64_create (
    float scale
) 
```





**Parameters:**


* `scale` scale (default: 32768.0f). 



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





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function f32\_to\_i16u64\_step 

_Process one input sample._ 
```C++
JM_FORCEINLINE  JM_HOT uint64_t f32_to_i16u64_step (
    const f32_to_i16u64_state_t * state,
    float x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (float). 



**Returns:**

Output sample (uint64\_t). 





        

<hr>



### function f32\_to\_i16u64\_steps 

_Process a block of samples._ 
```C++
void f32_to_i16u64_steps (
    f32_to_i16u64_state_t * state,
    const float * input,
    uint64_t * output,
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
The documentation for this class was generated from the following file `native/inc/f32_to_i16u64/f32_to_i16u64_core.h`

