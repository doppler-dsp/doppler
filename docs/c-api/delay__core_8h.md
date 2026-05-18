

# File delay\_core.h



[**FileList**](files.md) **>** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md) **>** [**delay\_core.h**](delay__core_8h.md)

[Go to the source code of this file](delay__core_8h_source.md)

_Delay component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**delay\_state\_t**](structdelay__state__t.md) <br>_Delay state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**delay\_state\_t**](structdelay__state__t.md) \* | [**delay\_create**](#function-delay_create) (size\_t num\_taps) <br>_Create a delay instance._  |
|  void | [**delay\_destroy**](#function-delay_destroy) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Destroy a delay instance and release all memory._  |
|  size\_t | [**delay\_ptr**](#function-delay_ptr) ([**delay\_state\_t**](structdelay__state__t.md) \* state, size\_t n, double complex \* out) <br> |
|  size\_t | [**delay\_ptr\_max\_out**](#function-delay_ptr_max_out) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_ptr._  |
|  void | [**delay\_push**](#function-delay_push) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x) <br>_push._  |
|  size\_t | [**delay\_push\_ptr**](#function-delay_push_ptr) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x, double complex \* out) <br> |
|  size\_t | [**delay\_push\_ptr\_max\_out**](#function-delay_push_ptr_max_out) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br> |
|  void | [**delay\_reset**](#function-delay_reset) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Reset delay to its post-create state._  |
|  void | [**delay\_write**](#function-delay_write) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x) <br>_write._  |




























## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
delay_state_t *obj = delay_create();
float complex y = delay_step(obj, 0.0f + 0.0f * I);
delay_destroy(obj);
```
 


    
## Public Functions Documentation




### function delay\_create 

_Create a delay instance._ 
```C++
delay_state_t * delay_create (
    size_t num_taps
) 
```



Allocates a dual-buffer circular delay line of length num\_taps. The internal buffer is rounded up to the next power of two for efficient modular addressing.




**Parameters:**


* `num_taps` Window length (&gt;= 1). Rounded up internally. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**delay\_destroy()**](delay__core_8h.md#function-delay_destroy) when done. 





        

<hr>



### function delay\_destroy 

_Destroy a delay instance and release all memory._ 
```C++
void delay_destroy (
    delay_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function delay\_ptr 

```C++
size_t delay_ptr (
    delay_state_t * state,
    size_t n,
    double complex * out
) 
```




<hr>



### function delay\_ptr\_max\_out 

_ptr._ 
```C++
size_t delay_ptr_max_out (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Result (double complex). 





        

<hr>



### function delay\_push 

_push._ 
```C++
void delay_push (
    delay_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` double complex parameter. 




        

<hr>



### function delay\_push\_ptr 

```C++
size_t delay_push_ptr (
    delay_state_t * state,
    double complex x,
    double complex * out
) 
```




<hr>



### function delay\_push\_ptr\_max\_out 

```C++
size_t delay_push_ptr_max_out (
    delay_state_t * state
) 
```



Maximum output samples per delay\_push\_ptr call. 


        

<hr>



### function delay\_reset 

_Reset delay to its post-create state._ 
```C++
void delay_reset (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function delay\_write 

_write._ 
```C++
void delay_write (
    delay_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input (double complex). 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/delay/delay_core.h`

