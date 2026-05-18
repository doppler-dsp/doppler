

# File acc\_cf64\_core.h



[**FileList**](files.md) **>** [**acc\_cf64**](dir_a31d3897e2036bab462df07bf5a3b557.md) **>** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md)

[Go to the source code of this file](acc__cf64__core_8h_source.md)

_AccCf64 component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) <br>_AccCf64 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acc\_cf64\_add2d**](#function-acc_cf64_add2d) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len) <br>_Accumulate a 2-D array: acc += sum of all elements in x._  |
|  [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* | [**acc\_cf64\_create**](#function-acc_cf64_create) (double \_Complex acc) <br>_Create a acc\_cf64 instance._  |
|  void | [**acc\_cf64\_destroy**](#function-acc_cf64_destroy) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Destroy a acc\_cf64 instance and release all memory._  |
|  double complex | [**acc\_cf64\_dump**](#function-acc_cf64_dump) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_dump._  |
|  double complex | [**acc\_cf64\_get**](#function-acc_cf64_get) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_get._  |
|  double \_Complex | [**acc\_cf64\_get\_acc**](#function-acc_cf64_get_acc) (const [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Get current acc._  |
|  void | [**acc\_cf64\_madd**](#function-acc_cf64_madd) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_Multiply-accumulate: acc += sum(x \* h) over x\_len samples._  |
|  void | [**acc\_cf64\_madd2d**](#function-acc_cf64_madd2d) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_2-D multiply-accumulate: acc += sum(x \* h) over x\_len elements._  |
|  void | [**acc\_cf64\_reset**](#function-acc_cf64_reset) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Reset acc\_cf64 to its post-create state._  |
|  void | [**acc\_cf64\_set\_acc**](#function-acc_cf64_set_acc) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, double \_Complex acc) <br>_Set acc._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_cf64\_step**](#function-acc_cf64_step) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, double complex x) <br>_Consume one input sample (sink; no output)._  |
|  void | [**acc\_cf64\_steps**](#function-acc_cf64_steps) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* input, size\_t n) <br>_Process a block of input samples (no output)._  |




























## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
acc_cf64_state_t *obj = acc_cf64_create(0.0 + 0.0 * I);
acc_cf64_step(obj, 0.0 + 0.0 * I);
acc_cf64_destroy(obj);
```
 


    
## Public Functions Documentation




### function acc\_cf64\_add2d 

_Accumulate a 2-D array: acc += sum of all elements in x._ 
```C++
void acc_cf64_add2d (
    acc_cf64_state_t * state,
    const double complex * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input array (double complex), x\_len elements total. 
* `x_len` Total number of elements. 




        

<hr>



### function acc\_cf64\_create 

_Create a acc\_cf64 instance._ 
```C++
acc_cf64_state_t * acc_cf64_create (
    double _Complex acc
) 
```





**Parameters:**


* `acc` Initial acc (default: 0.0 + 0.0 \* I). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_cf64\_destroy()**](acc__cf64__core_8h.md#function-acc_cf64_destroy) when done. 





        

<hr>



### function acc\_cf64\_destroy 

_Destroy a acc\_cf64 instance and release all memory._ 
```C++
void acc_cf64_destroy (
    acc_cf64_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function acc\_cf64\_dump 

_dump._ 
```C++
double complex acc_cf64_dump (
    acc_cf64_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Result (double complex). 





        

<hr>



### function acc\_cf64\_get 

_get._ 
```C++
double complex acc_cf64_get (
    acc_cf64_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Result (double complex). 





        

<hr>



### function acc\_cf64\_get\_acc 

_Get current acc._ 
```C++
double _Complex acc_cf64_get_acc (
    const acc_cf64_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function acc\_cf64\_madd 

_Multiply-accumulate: acc += sum(x \* h) over x\_len samples._ 
```C++
void acc_cf64_madd (
    acc_cf64_state_t * state,
    const double complex * x,
    size_t x_len,
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input array (double complex), length x\_len. 
* `x_len` Number of input samples. 
* `h` Coefficient array (float), length h\_len. 
* `h_len` Number of coefficients. 




        

<hr>



### function acc\_cf64\_madd2d 

_2-D multiply-accumulate: acc += sum(x \* h) over x\_len elements._ 
```C++
void acc_cf64_madd2d (
    acc_cf64_state_t * state,
    const double complex * x,
    size_t x_len,
    const float * h,
    size_t h_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input array (double complex), length x\_len. 
* `x_len` Total number of elements. 
* `h` Coefficient array (float), length h\_len. 
* `h_len` Number of coefficients. 




        

<hr>



### function acc\_cf64\_reset 

_Reset acc\_cf64 to its post-create state._ 
```C++
void acc_cf64_reset (
    acc_cf64_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function acc\_cf64\_set\_acc 

_Set acc._ 
```C++
void acc_cf64_set_acc (
    acc_cf64_state_t * state,
    double _Complex acc
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `acc` New value. 




        

<hr>



### function acc\_cf64\_step 

_Consume one input sample (sink; no output)._ 
```C++
JM_FORCEINLINE  JM_HOT void acc_cf64_step (
    acc_cf64_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (double complex). 




        

<hr>



### function acc\_cf64\_steps 

_Process a block of input samples (no output)._ 
```C++
void acc_cf64_steps (
    acc_cf64_state_t * state,
    const double complex * input,
    size_t n
) 
```





**Parameters:**


* `state` Component state (mutated). 
* `input` Input array (length &gt;= n). 
* `n` Number of samples. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_cf64/acc_cf64_core.h`

