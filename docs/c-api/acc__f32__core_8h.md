

# File acc\_f32\_core.h



[**FileList**](files.md) **>** [**acc\_f32**](dir_0465294bf3f41af7dbdebf91d81a0c4a.md) **>** [**acc\_f32\_core.h**](acc__f32__core_8h.md)

[Go to the source code of this file](acc__f32__core_8h_source.md)

_AccF32 component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_f32\_state\_t**](structacc__f32__state__t.md) <br>_AccF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acc\_f32\_add2d**](#function-acc_f32_add2d) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len) <br>_add2d._  |
|  [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* | [**acc\_f32\_create**](#function-acc_f32_create) (float acc) <br>_Create a acc\_f32 instance._  |
|  void | [**acc\_f32\_destroy**](#function-acc_f32_destroy) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Destroy a acc\_f32 instance and release all memory._  |
|  float | [**acc\_f32\_dump**](#function-acc_f32_dump) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_dump._  |
|  float | [**acc\_f32\_get**](#function-acc_f32_get) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_get._  |
|  float | [**acc\_f32\_get\_acc**](#function-acc_f32_get_acc) (const [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Get current acc._  |
|  void | [**acc\_f32\_madd**](#function-acc_f32_madd) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_madd._  |
|  void | [**acc\_f32\_madd2d**](#function-acc_f32_madd2d) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_madd2d._  |
|  void | [**acc\_f32\_reset**](#function-acc_f32_reset) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Reset acc\_f32 to its post-create state._  |
|  void | [**acc\_f32\_set\_acc**](#function-acc_f32_set_acc) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, float acc) <br>_Set acc._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_f32\_step**](#function-acc_f32_step) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, float x) <br>_Consume one input sample (sink; no output)._  |
|  void | [**acc\_f32\_steps**](#function-acc_f32_steps) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* input, size\_t n) <br>_Process a block of input samples (no output)._  |




























## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
acc_f32_state_t *obj = acc_f32_create(0.0f);
acc_f32_step(obj, 0.0f);
acc_f32_destroy(obj);
```
 


    
## Public Functions Documentation




### function acc\_f32\_add2d 

_add2d._ 
```C++
void acc_f32_add2d (
    acc_f32_state_t * state,
    const float * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input (float[]). 




        

<hr>



### function acc\_f32\_create 

_Create a acc\_f32 instance._ 
```C++
acc_f32_state_t * acc_f32_create (
    float acc
) 
```





**Parameters:**


* `acc` Initial acc (default: 0.0f). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_f32\_destroy()**](acc__f32__core_8h.md#function-acc_f32_destroy) when done. 





        

<hr>



### function acc\_f32\_destroy 

_Destroy a acc\_f32 instance and release all memory._ 
```C++
void acc_f32_destroy (
    acc_f32_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function acc\_f32\_dump 

_dump._ 
```C++
float acc_f32_dump (
    acc_f32_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Result (float). 





        

<hr>



### function acc\_f32\_get 

_get._ 
```C++
float acc_f32_get (
    acc_f32_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Result (float). 





        

<hr>



### function acc\_f32\_get\_acc 

_Get current acc._ 
```C++
float acc_f32_get_acc (
    const acc_f32_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function acc\_f32\_madd 

_madd._ 
```C++
void acc_f32_madd (
    acc_f32_state_t * state,
    const float * x,
    size_t x_len,
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input (float[]). 
* `h` float[] parameter. 




        

<hr>



### function acc\_f32\_madd2d 

_madd2d._ 
```C++
void acc_f32_madd2d (
    acc_f32_state_t * state,
    const float * x,
    size_t x_len,
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input (float[]). 
* `h` float[] parameter. 




        

<hr>



### function acc\_f32\_reset 

_Reset acc\_f32 to its post-create state._ 
```C++
void acc_f32_reset (
    acc_f32_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function acc\_f32\_set\_acc 

_Set acc._ 
```C++
void acc_f32_set_acc (
    acc_f32_state_t * state,
    float acc
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `acc` New value. 




        

<hr>



### function acc\_f32\_step 

_Consume one input sample (sink; no output)._ 
```C++
JM_FORCEINLINE  JM_HOT void acc_f32_step (
    acc_f32_state_t * state,
    float x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (float). 




        

<hr>



### function acc\_f32\_steps 

_Process a block of input samples (no output)._ 
```C++
void acc_f32_steps (
    acc_f32_state_t * state,
    const float * input,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). 
* `input` Input array (length &gt;= n). 
* `n` Number of samples. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_f32/acc_f32_core.h`

