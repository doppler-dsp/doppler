

# File delay\_core.h



[**FileList**](files.md) **>** [**delay**](dir_e9520af345bba2408e131802acc7e37b.md) **>** [**delay\_core.h**](delay__core_8h.md)

[Go to the source code of this file](delay__core_8h_source.md)

_Delay component API._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/jm_perf.h"`
* `#include "doppler/dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_delay\_state\_t**](structdp__delay__state__t.md) <br>_Delay state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* | [**dp\_delay\_create**](#function-dp_delay_create) (size\_t num\_taps) <br>_Create a dual-buffer circular delay line of length num\_taps. The internal capacity is rounded up to the next power of two so that modular indexing reduces to a single bitwise AND. Any window of num\_taps consecutive samples is always contiguous in the backing store; no wrap-around copy is ever needed._  |
|  void | [**dp\_delay\_destroy**](#function-dp_delay_destroy) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state) <br>_Destroy a delay instance and release all memory. Frees the internal dual buffer and the state struct itself. Safe to call with a NULL pointer (no-op). After this call the pointer must not be used; the Python binding raises RuntimeError on any subsequent method call._  |
|  void | [**dp\_delay\_get\_state**](#function-dp_delay_get_state) (const [**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**dp\_delay\_ptr**](#function-dp_delay_ptr) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, size\_t n, double \_Complex \* out, size\_t max\_out) <br>_Snapshot the n most recent samples. Copies at most min(n, num\_taps) samples starting from_ `buf[head]` _into out. Because the dual-buffer layout guarantees contiguity, this is a single memcpy of up to num\_taps elements; no wrap-around logic is needed. The Python binding returns an independent NumPy array per call, so an earlier snapshot is never overwritten by a later one; pass_`out=` _to fill a caller-owned buffer instead of allocating._ |
|  size\_t | [**dp\_delay\_ptr\_max\_out**](#function-dp_delay_ptr_max_out) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, size\_t n) <br>_Maximum samples_ [_**dp\_delay\_ptr()**_](delay__core_8h.md#function-dp_delay_ptr) _writes for a request of n. Returns min(n, num\_taps) — the tight per-call bound (gh-607)._ |
|  void | [**dp\_delay\_push**](#function-dp_delay_push) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, double \_Complex x) <br>_Advance the write pointer and insert a new sample. The head pointer decrements (mod capacity) before the write so that_ `buf[head]` _always holds the most recent sample. The same value is simultaneously written at_`buf[head + capacity]` _to keep the mirror half in sync; this ensures any num\_taps-length window starting at head is contiguous without an extra copy._ |
|  size\_t | [**dp\_delay\_push\_ptr**](#function-dp_delay_push_ptr) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, double \_Complex x, double \_Complex \* out, size\_t max\_out) <br>_Atomically push a sample and snapshot the current window. Equivalent to calling_ [_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _then dp\_delay\_ptr(num\_taps), but avoids the overhead of a second function call. Always writes exactly num\_taps samples to out. The Python binding returns an independent NumPy array per call; pass_`out=` _to reuse one buffer across pushes._ |
|  size\_t | [**dp\_delay\_push\_ptr\_max\_out**](#function-dp_delay_push_ptr_max_out) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state) <br>_Return the maximum output capacity for_ [_**dp\_delay\_push\_ptr()**_](delay__core_8h.md#function-dp_delay_push_ptr) _. Returns num\_taps; the Python binding sizes each call's output array with it, and checks a caller's_`out=` _buffer against it._ |
|  void | [**dp\_delay\_reset**](#function-dp_delay_reset) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state) <br>_Reset the delay line to its post-create state. Zeroes the entire dual buffer and resets the write pointer to 0, discarding all previously pushed samples. The num\_taps and capacity are preserved; only the sample history is cleared._  |
|  int | [**dp\_delay\_set\_state**](#function-dp_delay_set_state) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**dp\_delay\_state\_bytes**](#function-dp_delay_state_bytes) (const [**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state) <br> |
|  void | [**dp\_delay\_write**](#function-dp_delay_write) ([**dp\_delay\_state\_t**](structdp__delay__state__t.md) \* state, double \_Complex x) <br>_Alias for_ [_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _; insert a sample without reading back. Provided for API symmetry with write-then-read patterns where the caller wants to decouple sample ingestion from window inspection. Internally delegates to_[_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _with no additional overhead._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DELAY\_STATE\_MAGIC**](delay__core_8h.md#define-delay_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','L','A','Y')`<br> |
| define  | [**DELAY\_STATE\_VERSION**](delay__core_8h.md#define-delay_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
dp_delay_state_t *obj = dp_delay_create();
float _Complex y = delay_step(obj, 0.0f + 0.0f * I);
dp_delay_destroy(obj);
```
 


    
## Public Functions Documentation




### function dp\_delay\_create 

_Create a dual-buffer circular delay line of length num\_taps. The internal capacity is rounded up to the next power of two so that modular indexing reduces to a single bitwise AND. Any window of num\_taps consecutive samples is always contiguous in the backing store; no wrap-around copy is ever needed._ 
```C++
dp_delay_state_t * dp_delay_create (
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



### function dp\_delay\_destroy 

_Destroy a delay instance and release all memory. Frees the internal dual buffer and the state struct itself. Safe to call with a NULL pointer (no-op). After this call the pointer must not be used; the Python binding raises RuntimeError on any subsequent method call._ 
```C++
void dp_delay_destroy (
    dp_delay_state_t * state
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



### function dp\_delay\_get\_state 

```C++
void dp_delay_get_state (
    const dp_delay_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_delay\_ptr 

_Snapshot the n most recent samples. Copies at most min(n, num\_taps) samples starting from_ `buf[head]` _into out. Because the dual-buffer layout guarantees contiguity, this is a single memcpy of up to num\_taps elements; no wrap-around logic is needed. The Python binding returns an independent NumPy array per call, so an earlier snapshot is never overwritten by a later one; pass_`out=` _to fill a caller-owned buffer instead of allocating._
```C++
size_t dp_delay_ptr (
    dp_delay_state_t * state,
    size_t n,
    double _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Number of samples to copy; clamped to num\_taps. 
* `out` Output buffer; must hold at least max\_out elements. 
* `max_out` Capacity of `out` in samples. Normally num\_taps (what [**dp\_delay\_ptr\_max\_out()**](delay__core_8h.md#function-dp_delay_ptr_max_out) reports); a smaller value truncates the snapshot instead of overrunning the buffer. 



**Returns:**

min(n, num\_taps, max\_out) samples. 
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



### function dp\_delay\_ptr\_max\_out 

_Maximum samples_ [_**dp\_delay\_ptr()**_](delay__core_8h.md#function-dp_delay_ptr) _writes for a request of n. Returns min(n, num\_taps) — the tight per-call bound (gh-607)._
```C++
size_t dp_delay_ptr_max_out (
    dp_delay_state_t * state,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Number of samples the matching [**dp\_delay\_ptr()**](delay__core_8h.md#function-dp_delay_ptr) call requests. 



**Returns:**

min(n, num\_taps). 





        

<hr>



### function dp\_delay\_push 

_Advance the write pointer and insert a new sample. The head pointer decrements (mod capacity) before the write so that_ `buf[head]` _always holds the most recent sample. The same value is simultaneously written at_`buf[head + capacity]` _to keep the mirror half in sync; this ensures any num\_taps-length window starting at head is contiguous without an extra copy._
```C++
void dp_delay_push (
    dp_delay_state_t * state,
    double _Complex x
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



### function dp\_delay\_push\_ptr 

_Atomically push a sample and snapshot the current window. Equivalent to calling_ [_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _then dp\_delay\_ptr(num\_taps), but avoids the overhead of a second function call. Always writes exactly num\_taps samples to out. The Python binding returns an independent NumPy array per call; pass_`out=` _to reuse one buffer across pushes._
```C++
size_t dp_delay_push_ptr (
    dp_delay_state_t * state,
    double _Complex x,
    double _Complex * out,
    size_t max_out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` New complex sample to insert. 
* `out` Output buffer; must hold at least max\_out elements. 
* `max_out` Capacity of `out` in samples. Normally num\_taps. The push happens either way  the ring is a running window and cannot be left un-advanced  but a smaller capacity truncates the snapshot that is handed back. 



**Returns:**

min(num\_taps, max\_out) samples. 
```C++
>>> from doppler.delay import DelayCf64
>>> d = DelayCf64(num_taps=3)
>>> d.push_ptr(1+0j).tolist()
[(1+0j), 0j, 0j]
>>> d.push_ptr(2+0j).tolist()
[(2+0j), (1+0j), 0j]
```
 





        

<hr>



### function dp\_delay\_push\_ptr\_max\_out 

_Return the maximum output capacity for_ [_**dp\_delay\_push\_ptr()**_](delay__core_8h.md#function-dp_delay_push_ptr) _. Returns num\_taps; the Python binding sizes each call's output array with it, and checks a caller's_`out=` _buffer against it._
```C++
size_t dp_delay_push_ptr_max_out (
    dp_delay_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

num\_taps (number of samples [**dp\_delay\_push\_ptr()**](delay__core_8h.md#function-dp_delay_push_ptr) will write). 





        

<hr>



### function dp\_delay\_reset 

_Reset the delay line to its post-create state. Zeroes the entire dual buffer and resets the write pointer to 0, discarding all previously pushed samples. The num\_taps and capacity are preserved; only the sample history is cleared._ 
```C++
void dp_delay_reset (
    dp_delay_state_t * state
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



### function dp\_delay\_set\_state 

```C++
int dp_delay_set_state (
    dp_delay_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_delay\_state\_bytes 

```C++
size_t dp_delay_state_bytes (
    const dp_delay_state_t * state
) 
```




<hr>



### function dp\_delay\_write 

_Alias for_ [_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _; insert a sample without reading back. Provided for API symmetry with write-then-read patterns where the caller wants to decouple sample ingestion from window inspection. Internally delegates to_[_**dp\_delay\_push()**_](delay__core_8h.md#function-dp_delay_push) _with no additional overhead._
```C++
void dp_delay_write (
    dp_delay_state_t * state,
    double _Complex x
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
## Macro Definition Documentation





### define DELAY\_STATE\_MAGIC 

```C++
#define DELAY_STATE_MAGIC `DP_FOURCC ('D','L','A','Y')`
```




<hr>



### define DELAY\_STATE\_VERSION 

```C++
#define DELAY_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/delay/delay_core.h`

