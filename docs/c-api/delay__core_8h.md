

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
|  [**delay\_state\_t**](structdelay__state__t.md) \* | [**delay\_create**](#function-delay_create) (size\_t num\_taps) <br>_Create a dual-buffer circular delay line of length num\_taps. The internal capacity is rounded up to the next power of two so that modular indexing reduces to a single bitwise AND. Any window of num\_taps consecutive samples is always contiguous in the backing store; no wrap-around copy is ever needed._  |
|  void | [**delay\_destroy**](#function-delay_destroy) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Destroy a delay instance and release all memory. Frees the internal dual buffer and the state struct itself. Safe to call with a NULL pointer (no-op). After this call the pointer must not be used; the Python binding raises RuntimeError on any subsequent method call._  |
|  size\_t | [**delay\_ptr**](#function-delay_ptr) ([**delay\_state\_t**](structdelay__state__t.md) \* state, size\_t n, double complex \* out) <br>_Return a zero-copy view of the n most recent samples. Copies at most min(n, num\_taps) samples starting from buf[head] into out. Because the dual-buffer layout guarantees contiguity, this is a single memcpy of up to num\_taps elements; no wrap-around logic is needed. The Python binding returns a NumPy array backed directly by the pre-allocated output buffer (base object is the DelayCf64 itself)._  |
|  size\_t | [**delay\_ptr\_max\_out**](#function-delay_ptr_max_out) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Return the maximum output capacity for_ [_**delay\_ptr()**_](delay__core_8h.md#function-delay_ptr) _. Returns num\_taps; the Python binding uses this to pre-allocate the output buffer before calling_[_**delay\_ptr()**_](delay__core_8h.md#function-delay_ptr) _._ |
|  void | [**delay\_push**](#function-delay_push) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x) <br>_Advance the write pointer and insert a new sample. The head pointer decrements (mod capacity) before the write so that buf[head] always holds the most recent sample. The same value is simultaneously written at buf[head + capacity] to keep the mirror half in sync; this ensures any num\_taps-length window starting at head is contiguous without an extra copy._  |
|  size\_t | [**delay\_push\_ptr**](#function-delay_push_ptr) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x, double complex \* out) <br>_Atomically push a sample and snapshot the current window. Equivalent to calling_ [_**delay\_push()**_](delay__core_8h.md#function-delay_push) _then delay\_ptr(num\_taps), but avoids the overhead of a second function call. Always writes exactly num\_taps samples to out. The Python binding returns a NumPy array backed by the pre-allocated push\_ptr output buffer._ |
|  size\_t | [**delay\_push\_ptr\_max\_out**](#function-delay_push_ptr_max_out) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Return the maximum output capacity for_ [_**delay\_push\_ptr()**_](delay__core_8h.md#function-delay_push_ptr) _. Returns num\_taps; the Python binding uses this to pre-allocate the output buffer before calling_[_**delay\_push\_ptr()**_](delay__core_8h.md#function-delay_push_ptr) _._ |
|  void | [**delay\_reset**](#function-delay_reset) ([**delay\_state\_t**](structdelay__state__t.md) \* state) <br>_Reset the delay line to its post-create state. Zeroes the entire dual buffer and resets the write pointer to 0, discarding all previously pushed samples. The num\_taps and capacity are preserved; only the sample history is cleared._  |
|  void | [**delay\_write**](#function-delay_write) ([**delay\_state\_t**](structdelay__state__t.md) \* state, double complex x) <br>_Alias for_ [_**delay\_push()**_](delay__core_8h.md#function-delay_push) _; insert a sample without reading back. Provided for API symmetry with write-then-read patterns where the caller wants to decouple sample ingestion from window inspection. Internally delegates to_[_**delay\_push()**_](delay__core_8h.md#function-delay_push) _with no additional overhead._ |




























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

_Create a dual-buffer circular delay line of length num\_taps. The internal capacity is rounded up to the next power of two so that modular indexing reduces to a single bitwise AND. Any window of num\_taps consecutive samples is always contiguous in the backing store; no wrap-around copy is ever needed._ 
```C++
delay_state_t * delay_create (
    size_t num_taps
) 
```





**Parameters:**


* `num_taps` Number of delay taps (window length, &gt;= 1). Internally rounded up to the next power of two. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.num_taps
3
>>> d.capacity   # next power-of-two >= 3
4
```
 





        

<hr>



### function delay\_destroy 

_Destroy a delay instance and release all memory. Frees the internal dual buffer and the state struct itself. Safe to call with a NULL pointer (no-op). After this call the pointer must not be used; the Python binding raises RuntimeError on any subsequent method call._ 
```C++
void delay_destroy (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Heap-allocated delay state, or NULL. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=2)
>>> d.push(1+0j)
>>> d.destroy()
>>> try:
...     d.push(2+0j)
... except RuntimeError as e:
...     print(e)
destroyed
```
 




        

<hr>



### function delay\_ptr 

_Return a zero-copy view of the n most recent samples. Copies at most min(n, num\_taps) samples starting from buf[head] into out. Because the dual-buffer layout guarantees contiguity, this is a single memcpy of up to num\_taps elements; no wrap-around logic is needed. The Python binding returns a NumPy array backed directly by the pre-allocated output buffer (base object is the DelayCf64 itself)._ 
```C++
size_t delay_ptr (
    delay_state_t * state,
    size_t n,
    double complex * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Number of samples to copy; clamped to num\_taps. 
* `out` Output buffer; must hold at least min(n, num\_taps) elements. 



**Returns:**

Number of samples written. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.push(1+0j)
>>> d.push(2+0j)
>>> y = d.ptr()
>>> y.tolist()
[(2+0j), (1+0j), 0j]
>>> y.dtype
dtype('complex128')
>>> y.shape
(3,)
```
 





        

<hr>



### function delay\_ptr\_max\_out 

_Return the maximum output capacity for_ [_**delay\_ptr()**_](delay__core_8h.md#function-delay_ptr) _. Returns num\_taps; the Python binding uses this to pre-allocate the output buffer before calling_[_**delay\_ptr()**_](delay__core_8h.md#function-delay_ptr) _._
```C++
size_t delay_ptr_max_out (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

num\_taps (maximum samples [**delay\_ptr()**](delay__core_8h.md#function-delay_ptr) can write). 





        

<hr>



### function delay\_push 

_Advance the write pointer and insert a new sample. The head pointer decrements (mod capacity) before the write so that buf[head] always holds the most recent sample. The same value is simultaneously written at buf[head + capacity] to keep the mirror half in sync; this ensures any num\_taps-length window starting at head is contiguous without an extra copy._ 
```C++
void delay_push (
    delay_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` New complex sample to insert. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.push(1+2j)
>>> d.push(3+4j)
>>> d.ptr().tolist()
[(3+4j), (1+2j), 0j]
```
 




        

<hr>



### function delay\_push\_ptr 

_Atomically push a sample and snapshot the current window. Equivalent to calling_ [_**delay\_push()**_](delay__core_8h.md#function-delay_push) _then delay\_ptr(num\_taps), but avoids the overhead of a second function call. Always writes exactly num\_taps samples to out. The Python binding returns a NumPy array backed by the pre-allocated push\_ptr output buffer._
```C++
size_t delay_push_ptr (
    delay_state_t * state,
    double complex x,
    double complex * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` New complex sample to insert. 
* `out` Output buffer; must hold at least num\_taps elements. 



**Returns:**

num\_taps (always equal to the window length). 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.push_ptr(1+0j).tolist()
[(1+0j), 0j, 0j]
>>> d.push_ptr(2+0j).tolist()
[(2+0j), (1+0j), 0j]
```
 





        

<hr>



### function delay\_push\_ptr\_max\_out 

_Return the maximum output capacity for_ [_**delay\_push\_ptr()**_](delay__core_8h.md#function-delay_push_ptr) _. Returns num\_taps; the Python binding uses this to pre-allocate the output buffer before calling_[_**delay\_push\_ptr()**_](delay__core_8h.md#function-delay_push_ptr) _._
```C++
size_t delay_push_ptr_max_out (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

num\_taps (number of samples [**delay\_push\_ptr()**](delay__core_8h.md#function-delay_push_ptr) will write). 





        

<hr>



### function delay\_reset 

_Reset the delay line to its post-create state. Zeroes the entire dual buffer and resets the write pointer to 0, discarding all previously pushed samples. The num\_taps and capacity are preserved; only the sample history is cleared._ 
```C++
void delay_reset (
    delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.push(1+2j)
>>> d.push(3+4j)
>>> d.ptr().tolist()
[(3+4j), (1+2j), 0j]
>>> d.reset()
>>> d.ptr().tolist()
[0j, 0j, 0j]
```
 




        

<hr>



### function delay\_write 

_Alias for_ [_**delay\_push()**_](delay__core_8h.md#function-delay_push) _; insert a sample without reading back. Provided for API symmetry with write-then-read patterns where the caller wants to decouple sample ingestion from window inspection. Internally delegates to_[_**delay\_push()**_](delay__core_8h.md#function-delay_push) _with no additional overhead._
```C++
void delay_write (
    delay_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` New complex sample to insert. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=2)
>>> d.write(5+6j)
>>> d.ptr().tolist()
[(5+6j), 0j]
```
 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/delay/delay_core.h`

