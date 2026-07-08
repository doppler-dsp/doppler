

# File acc\_q15\_core.h



[**FileList**](files.md) **>** [**acc\_q15**](dir_df770d8a485da99b359af14931eaacf8.md) **>** [**acc\_q15\_core.h**](acc__q15__core_8h.md)

[Go to the source code of this file](acc__q15__core_8h_source.md)

_AccQ15 — a running 64-bit integer accumulator for Q15 (int16\_t) samples. Internally sums each sample into a 64-bit accumulator, which prevents overflow even for very long block lengths. Use get() to read the running total non-destructively, or dump() to read-and-reset in one call._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_q15\_state\_t**](structacc__q15__state__t.md) <br>_AccQ15 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* | [**acc\_q15\_create**](#function-acc_q15_create) (int64\_t acc) <br>_Allocate and initialise an AccQ15 accumulator. The accumulator starts at the supplied initial value and may be driven sample-by-sample (step), in bulk (steps), or via multiply-accumulate (madd). The internal register is a 64-bit signed integer so it will not overflow in any realistic DSP workload._  |
|  void | [**acc\_q15\_destroy**](#function-acc_q15_destroy) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br>_Destroy an AccQ15 instance and release all memory. Safe to call with NULL._  |
|  int64\_t | [**acc\_q15\_dump**](#function-acc_q15_dump) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br>_Return the accumulated value and atomically reset it to zero. Ideal for block-based processing where each block hands off its sum and then starts fresh, avoiding a separate reset() call._  |
|  int64\_t | [**acc\_q15\_get**](#function-acc_q15_get) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br>_Return the current accumulated value without resetting it. Identical to reading the acc field directly; exists as a named method so the Python binding can expose it consistently with dump()._  |
|  int64\_t | [**acc\_q15\_get\_acc**](#function-acc_q15_get_acc) (const [**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br>_Read the current accumulator value without modifying it. Use this when you need to snapshot the running total mid-stream and continue accumulating afterward._  |
|  void | [**acc\_q15\_get\_state**](#function-acc_q15_get_state) (const [**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, void \* blob) <br> |
|  void | [**acc\_q15\_madd**](#function-acc_q15_madd) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, const int16\_t \* a, size\_t a\_len, const int16\_t \* b, size\_t b\_len) <br>_Multiply-accumulate over the shorter of the two arrays. Computes acc += sum(_ `a[i]` _\*_`b[i]` _), using SIMD (AVX2 when available) to process multiple products per cycle, making this efficient for FIR filter energy computation and dot-product accumulation across blocks._ |
|  void | [**acc\_q15\_reset**](#function-acc_q15_reset) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br>_Reset the accumulator to zero, mirroring the post-create state. Does not re-initialise to the constructor's acc value — always resets to zero, matching the default initial state for a clean sweep._  |
|  void | [**acc\_q15\_set\_acc**](#function-acc_q15_set_acc) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, int64\_t val) <br>_Overwrite the accumulator with a new value. Useful for setting a bias before a new accumulation window, or for restoring a previously checkpointed value._  |
|  int | [**acc\_q15\_set\_state**](#function-acc_q15_set_state) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**acc\_q15\_state\_bytes**](#function-acc_q15_state_bytes) (const [**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_q15\_step**](#function-acc_q15_step) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, int16\_t x) <br>_Accumulate one Q15 sample into the running total. The sample is sign-extended to 64 bits before addition, ensuring that negative samples subtract correctly from the accumulator without wrap._  |
|  void | [**acc\_q15\_steps**](#function-acc_q15_steps) ([**acc\_q15\_state\_t**](structacc__q15__state__t.md) \* state, const int16\_t \* input, size\_t n) <br>_Accumulate a contiguous block of Q15 samples. Equivalent to calling step() n times but faster for large arrays because the loop can be auto-vectorised by the compiler._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACC\_Q15\_STATE\_MAGIC**](acc__q15__core_8h.md#define-acc_q15_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', '1', '5')`<br> |
| define  | [**ACC\_Q15\_STATE\_VERSION**](acc__q15__core_8h.md#define-acc_q15_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; `[step / steps / madd / reset]*` -&gt; `[get / dump]*` -&gt; destroy



```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.get()
0
```
 


    
## Public Functions Documentation




### function acc\_q15\_create 

_Allocate and initialise an AccQ15 accumulator. The accumulator starts at the supplied initial value and may be driven sample-by-sample (step), in bulk (steps), or via multiply-accumulate (madd). The internal register is a 64-bit signed integer so it will not overflow in any realistic DSP workload._ 
```C++
acc_q15_state_t * acc_q15_create (
    int64_t acc
) 
```





**Parameters:**


* `acc` Initial accumulator value (default: 0). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_q15\_destroy()**](acc__q15__core_8h.md#function-acc_q15_destroy) when done.



```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(100)
>>> obj.get_acc()
100
```
 


        

<hr>



### function acc\_q15\_destroy 

_Destroy an AccQ15 instance and release all memory. Safe to call with NULL._ 
```C++
void acc_q15_destroy (
    acc_q15_state_t * state
) 
```





**Parameters:**


* `state` May be NULL.


```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.destroy()
```
 


        

<hr>



### function acc\_q15\_dump 

_Return the accumulated value and atomically reset it to zero. Ideal for block-based processing where each block hands off its sum and then starts fresh, avoiding a separate reset() call._ 
```C++
int64_t acc_q15_dump (
    acc_q15_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Accumulator value before the reset (int64\_t).



```C++
>>> from doppler.arith import AccQ15
>>> import numpy as np
>>> obj = AccQ15(0)
>>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
>>> obj.dump()
15
>>> obj.get()
0
```
 


        

<hr>



### function acc\_q15\_get 

_Return the current accumulated value without resetting it. Identical to reading the acc field directly; exists as a named method so the Python binding can expose it consistently with dump()._ 
```C++
int64_t acc_q15_get (
    acc_q15_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Current accumulator value (int64\_t).



```C++
>>> from doppler.arith import AccQ15
>>> import numpy as np
>>> obj = AccQ15(0)
>>> obj.steps(np.array([10, 20, 30], dtype=np.int16))
>>> obj.get()
60
```
 


        

<hr>



### function acc\_q15\_get\_acc 

_Read the current accumulator value without modifying it. Use this when you need to snapshot the running total mid-stream and continue accumulating afterward._ 
```C++
int64_t acc_q15_get_acc (
    const acc_q15_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.step(100)
>>> obj.get()
100
>>> obj.step(200)
>>> obj.get()
300
```
 


        

<hr>



### function acc\_q15\_get\_state 

```C++
void acc_q15_get_state (
    const acc_q15_state_t * state,
    void * blob
) 
```




<hr>



### function acc\_q15\_madd 

_Multiply-accumulate over the shorter of the two arrays. Computes acc += sum(_ `a[i]` _\*_`b[i]` _), using SIMD (AVX2 when available) to process multiple products per cycle, making this efficient for FIR filter energy computation and dot-product accumulation across blocks._
```C++
void acc_q15_madd (
    acc_q15_state_t * state,
    const int16_t * a,
    size_t a_len,
    const int16_t * b,
    size_t b_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `a` First input array (int16\_t). 
* `a_len` Number of elements in a. 
* `b` Second input array (int16\_t), same length as a. 
* `b_len` Number of elements in b.


```C++
>>> from doppler.arith import AccQ15
>>> import numpy as np
>>> obj = AccQ15(0)
>>> a = np.array([100, 200, 300], dtype=np.int16)
>>> b = np.array([10, 20, 30], dtype=np.int16)
>>> obj.madd(a, b)
>>> obj.get()
14000
```
 


        

<hr>



### function acc\_q15\_reset 

_Reset the accumulator to zero, mirroring the post-create state. Does not re-initialise to the constructor's acc value — always resets to zero, matching the default initial state for a clean sweep._ 
```C++
void acc_q15_reset (
    acc_q15_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.step(42)
>>> obj.reset()
>>> obj.get()
0
```
 


        

<hr>



### function acc\_q15\_set\_acc 

_Overwrite the accumulator with a new value. Useful for setting a bias before a new accumulation window, or for restoring a previously checkpointed value._ 
```C++
void acc_q15_set_acc (
    acc_q15_state_t * state,
    int64_t val
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `val` Replacement accumulator value.


```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.set_acc(1000)
>>> obj.get_acc()
1000
```
 


        

<hr>



### function acc\_q15\_set\_state 

```C++
int acc_q15_set_state (
    acc_q15_state_t * state,
    const void * blob
) 
```




<hr>



### function acc\_q15\_state\_bytes 

```C++
size_t acc_q15_state_bytes (
    const acc_q15_state_t * state
) 
```




<hr>



### function acc\_q15\_step 

_Accumulate one Q15 sample into the running total. The sample is sign-extended to 64 bits before addition, ensuring that negative samples subtract correctly from the accumulator without wrap._ 
```C++
JM_FORCEINLINE  JM_HOT void acc_q15_step (
    acc_q15_state_t * state,
    int16_t x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Q15 input sample (int16\_t, range `[-32768, 32767]`).


```C++
>>> from doppler.arith import AccQ15
>>> obj = AccQ15(0)
>>> obj.step(100)
>>> obj.step(200)
>>> obj.get()
300
```
 


        

<hr>



### function acc\_q15\_steps 

_Accumulate a contiguous block of Q15 samples. Equivalent to calling step() n times but faster for large arrays because the loop can be auto-vectorised by the compiler._ 
```C++
void acc_q15_steps (
    acc_q15_state_t * state,
    const int16_t * input,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `input` Input array of int16\_t samples. 
* `n` Number of samples in input.


```C++
>>> from doppler.arith import AccQ15
>>> import numpy as np
>>> obj = AccQ15(0)
>>> obj.steps(np.array([1, 2, 3, 4, 5], dtype=np.int16))
>>> obj.get()
15
```
 


        

<hr>
## Macro Definition Documentation





### define ACC\_Q15\_STATE\_MAGIC 

```C++
#define ACC_Q15_STATE_MAGIC `DP_FOURCC ('A', 'C', '1', '5')`
```




<hr>



### define ACC\_Q15\_STATE\_VERSION 

```C++
#define ACC_Q15_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_q15/acc_q15_core.h`

