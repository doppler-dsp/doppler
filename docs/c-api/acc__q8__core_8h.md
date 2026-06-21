

# File acc\_q8\_core.h



[**FileList**](files.md) **>** [**acc\_q8**](dir_af45fd7415a1bcf5c13e14c3d63a83bf.md) **>** [**acc\_q8\_core.h**](acc__q8__core_8h.md)

[Go to the source code of this file](acc__q8__core_8h_source.md)

_AccQ8 — a running 32-bit integer accumulator for Q8 (int8\_t) samples. Internally sums each sample into a 32-bit accumulator, which can hold up to 2^24 maximum-magnitude Q8 samples before overflow. Use get() for a non-destructive read, or dump() to read-and-reset in one atomic call._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_q8\_state\_t**](structacc__q8__state__t.md) <br>_AccQ8 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* | [**acc\_q8\_create**](#function-acc_q8_create) (int32\_t acc) <br>_Allocate and initialise an AccQ8 accumulator. The accumulator starts at the supplied initial value and accepts Q8 (int8\_t) samples via step(), steps(), or madd(). The 32-bit internal register handles up to roughly 16 million max-magnitude samples before wrap — sufficient for all standard DSP block sizes._  |
|  void | [**acc\_q8\_destroy**](#function-acc_q8_destroy) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state) <br>_Destroy an AccQ8 instance and release all memory. Safe to call with NULL._  |
|  int32\_t | [**acc\_q8\_dump**](#function-acc_q8_dump) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state) <br>_Return the accumulated value and atomically reset it to zero. Avoids the need for a separate reset() call when processing a stream of non-overlapping blocks._  |
|  int32\_t | [**acc\_q8\_get**](#function-acc_q8_get) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state) <br>_Return the current accumulated value without resetting it. Mirrors get\_acc() but exposed under the name used consistently across all Acc-family objects in the Python API._  |
|  int32\_t | [**acc\_q8\_get\_acc**](#function-acc_q8_get_acc) (const [**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state) <br>_Read the current accumulator value without modifying it. Permits repeated snapshots of the running sum mid-stream._  |
|  void | [**acc\_q8\_madd**](#function-acc_q8_madd) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state, const int8\_t \* a, size\_t a\_len, const int8\_t \* b, size\_t b\_len) <br>_Multiply-accumulate over the shorter of the two arrays. Computes acc += sum(_ `a[i]` _\*_`b[i]` _), widening int8\_t inputs to int32\_t before accumulation to prevent intermediate overflow._ |
|  void | [**acc\_q8\_reset**](#function-acc_q8_reset) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state) <br>_Reset the accumulator to zero, mirroring the post-create state. Always resets to zero regardless of the original constructor value, so it is safe to call at the start of any new accumulation window._  |
|  void | [**acc\_q8\_set\_acc**](#function-acc_q8_set_acc) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state, int32\_t val) <br>_Overwrite the accumulator with a new value. Useful for applying a bias before a new accumulation window, or for restoring a checkpointed accumulator state._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_q8\_step**](#function-acc_q8_step) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state, int8\_t x) <br>_Accumulate one Q8 sample into the running total. The sample is sign-extended to 32 bits before addition so negative samples correctly subtract from the accumulator._  |
|  void | [**acc\_q8\_steps**](#function-acc_q8_steps) ([**acc\_q8\_state\_t**](structacc__q8__state__t.md) \* state, const int8\_t \* input, size\_t n) <br>_Accumulate a contiguous block of Q8 samples. Equivalent to calling step() n times; the single loop is more amenable to auto-vectorisation than repeated method calls._  |




























## Detailed Description


Lifecycle: create -&gt; `[step / steps / madd / reset]*` -&gt; `[get / dump]*` -&gt; destroy



```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.get()
0
```
 


    
## Public Functions Documentation




### function acc\_q8\_create 

_Allocate and initialise an AccQ8 accumulator. The accumulator starts at the supplied initial value and accepts Q8 (int8\_t) samples via step(), steps(), or madd(). The 32-bit internal register handles up to roughly 16 million max-magnitude samples before wrap — sufficient for all standard DSP block sizes._ 
```C++
acc_q8_state_t * acc_q8_create (
    int32_t acc
) 
```





**Parameters:**


* `acc` Initial accumulator value (default: 0). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_q8\_destroy()**](acc__q8__core_8h.md#function-acc_q8_destroy) when done.



```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(10)
>>> obj.get_acc()
10
```
 


        

<hr>



### function acc\_q8\_destroy 

_Destroy an AccQ8 instance and release all memory. Safe to call with NULL._ 
```C++
void acc_q8_destroy (
    acc_q8_state_t * state
) 
```





**Parameters:**


* `state` May be NULL.


```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.destroy()
```
 


        

<hr>



### function acc\_q8\_dump 

_Return the accumulated value and atomically reset it to zero. Avoids the need for a separate reset() call when processing a stream of non-overlapping blocks._ 
```C++
int32_t acc_q8_dump (
    acc_q8_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Accumulator value before the reset (int32\_t).



```C++
>>> from doppler.arith import AccQ8
>>> import numpy as np
>>> obj = AccQ8(0)
>>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
>>> obj.dump()
15
>>> obj.get()
0
```
 


        

<hr>



### function acc\_q8\_get 

_Return the current accumulated value without resetting it. Mirrors get\_acc() but exposed under the name used consistently across all Acc-family objects in the Python API._ 
```C++
int32_t acc_q8_get (
    acc_q8_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current accumulator value (int32\_t).



```C++
>>> from doppler.arith import AccQ8
>>> import numpy as np
>>> obj = AccQ8(0)
>>> obj.steps(np.array([10, 20, 30], dtype=np.int8))
>>> obj.get()
60
```
 


        

<hr>



### function acc\_q8\_get\_acc 

_Read the current accumulator value without modifying it. Permits repeated snapshots of the running sum mid-stream._ 
```C++
int32_t acc_q8_get_acc (
    const acc_q8_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.step(10)
>>> obj.get_acc()
10
```
 


        

<hr>



### function acc\_q8\_madd 

_Multiply-accumulate over the shorter of the two arrays. Computes acc += sum(_ `a[i]` _\*_`b[i]` _), widening int8\_t inputs to int32\_t before accumulation to prevent intermediate overflow._
```C++
void acc_q8_madd (
    acc_q8_state_t * state,
    const int8_t * a,
    size_t a_len,
    const int8_t * b,
    size_t b_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `a` First input array (int8\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int8\_t), same length as a. 
* `b_len` Number of elements in b.


```C++
>>> from doppler.arith import AccQ8
>>> import numpy as np
>>> obj = AccQ8(0)
>>> a = np.array([10, 20, 30], dtype=np.int8)
>>> b = np.array([1, 2, 3], dtype=np.int8)
>>> obj.madd(a, b)
>>> obj.get()
140
```
 


        

<hr>



### function acc\_q8\_reset 

_Reset the accumulator to zero, mirroring the post-create state. Always resets to zero regardless of the original constructor value, so it is safe to call at the start of any new accumulation window._ 
```C++
void acc_q8_reset (
    acc_q8_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.step(42)
>>> obj.reset()
>>> obj.get()
0
```
 


        

<hr>



### function acc\_q8\_set\_acc 

_Overwrite the accumulator with a new value. Useful for applying a bias before a new accumulation window, or for restoring a checkpointed accumulator state._ 
```C++
void acc_q8_set_acc (
    acc_q8_state_t * state,
    int32_t val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` Replacement accumulator value.


```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.set_acc(50)
>>> obj.get_acc()
50
```
 


        

<hr>



### function acc\_q8\_step 

_Accumulate one Q8 sample into the running total. The sample is sign-extended to 32 bits before addition so negative samples correctly subtract from the accumulator._ 
```C++
JM_FORCEINLINE  JM_HOT void acc_q8_step (
    acc_q8_state_t * state,
    int8_t x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Q8 input sample (int8\_t, range `[-128, 127]`).


```C++
>>> from doppler.arith import AccQ8
>>> obj = AccQ8(0)
>>> obj.step(10)
>>> obj.step(20)
>>> obj.get()
30
```
 


        

<hr>



### function acc\_q8\_steps 

_Accumulate a contiguous block of Q8 samples. Equivalent to calling step() n times; the single loop is more amenable to auto-vectorisation than repeated method calls._ 
```C++
void acc_q8_steps (
    acc_q8_state_t * state,
    const int8_t * input,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `input` Input array of int8\_t samples. 
* `n` Number of samples in input.


```C++
>>> from doppler.arith import AccQ8
>>> import numpy as np
>>> obj = AccQ8(0)
>>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int8))
>>> obj.get()
15
```
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_q8/acc_q8_core.h`

