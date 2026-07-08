

# File acc\_f32\_core.h



[**FileList**](files.md) **>** [**acc\_f32**](dir_0465294bf3f41af7dbdebf91d81a0c4a.md) **>** [**acc\_f32\_core.h**](acc__f32__core_8h.md)

[Go to the source code of this file](acc__f32__core_8h_source.md)

_AccF32 component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_f32\_state\_t**](structacc__f32__state__t.md) <br>_AccF32 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acc\_f32\_add2d**](#function-acc_f32_add2d) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len) <br>_Sum all elements of a (logically) 2-D float array into the accumulator. The array is treated as a flat C-order buffer of_ `x_len` _floats regardless of the original shape; the caller is responsible for passing the total element count._ |
|  [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* | [**acc\_f32\_create**](#function-acc_f32_create) (float acc) <br>_Single-precision floating-point scalar accumulator. Maintains one running sum (_ `acc` _) that persists across calls to_`step` _,_`steps` _,_`madd` _,_`add2d` _, and_`madd2d` _. Use_`get` _to read without side-effects or_`dump` _to read and atomically zero in a single call._ |
|  void | [**acc\_f32\_destroy**](#function-acc_f32_destroy) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Release all memory owned by an AccF32 instance. Passing NULL is safe; the function is a no-op in that case. After this call the pointer must not be used._  |
|  float | [**acc\_f32\_dump**](#function-acc_f32_dump) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Return the accumulated sum and atomically reset it to zero. This is the canonical "drain" primitive: read the period total, then start a fresh accumulation interval without a separate_ `reset` _call. The zero-reset is unconditional and always writes 0.0f._ |
|  float | [**acc\_f32\_get**](#function-acc_f32_get) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Return the current accumulated sum without resetting state. Identical to reading the_ `acc` _property directly; retained as an explicit method so call sites that need the value can be uniform with_`dump` _without a conditional._ |
|  float | [**acc\_f32\_get\_acc**](#function-acc_f32_get_acc) (const [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Return the current accumulator value without modifying state. Use this when you need to read the running sum mid-accumulation without disturbing it. For a read-and-reset in one call use_ `acc_f32_dump` _._ |
|  void | [**acc\_f32\_get\_state**](#function-acc_f32_get_state) (const [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, void \* blob) <br> |
|  void | [**acc\_f32\_madd**](#function-acc_f32_madd) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_Dot-product accumulate:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. The shorter of the two arrays limits the iteration count; no out-of-bounds access occurs. Typical use: apply a short FIR weight vector to one block of signal samples and fold the result into a running total._ |
|  void | [**acc\_f32\_madd2d**](#function-acc_f32_madd2d) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_Dot-product accumulate over a flat 2-D buffer:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. Combines_`add2d` _and_`madd` _semantics — a 2-D signal array is weighted element-wise by a coefficient buffer and the scalar total is folded into the running sum._ |
|  void | [**acc\_f32\_reset**](#function-acc_f32_reset) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br>_Zero the accumulator, restoring the same state as a fresh_ `AccF32(0.0)` _— regardless of the value supplied to_`acc_f32_create` _. Subsequent_`get` _/_`dump` _calls return_`0.0` _until new samples are processed._ |
|  void | [**acc\_f32\_set\_acc**](#function-acc_f32_set_acc) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, float acc) <br>_Overwrite the accumulator with a new value. Useful for seeding the accumulator to a known baseline before processing a new segment without a full_ `reset` _._ |
|  int | [**acc\_f32\_set\_state**](#function-acc_f32_set_state) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**acc\_f32\_state\_bytes**](#function-acc_f32_state_bytes) (const [**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_f32\_step**](#function-acc_f32_step) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, float x) <br>_Add one sample to the running sum (_ `acc += x` _). This is the hot-path entry point for sample-by-sample processing. For block inputs prefer_`acc_f32_steps` _to amortise call overhead and allow auto-vectorisation._ |
|  void | [**acc\_f32\_steps**](#function-acc_f32_steps) ([**acc\_f32\_state\_t**](structacc__f32__state__t.md) \* state, const float \* input, size\_t n) <br>_Add all samples in_ `input` _to the running sum. Equivalent to calling_`acc_f32_step` _for each element, but SIMD-vectorised on platforms that provide it (AVX-512 / AVX2 / SSE2). The loop uses JM\_RESTRICT so the compiler can assume no aliasing between_`state` _and_`input` _._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACC\_F32\_STATE\_MAGIC**](acc__f32__core_8h.md#define-acc_f32_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', 'C', 'F')`<br> |
| define  | [**ACC\_F32\_STATE\_VERSION**](acc__f32__core_8h.md#define-acc_f32_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
acc_f32_state_t *obj = acc_f32_create(0.0f);
acc_f32_step(obj, 1.0f);
float v = acc_f32_get(obj);   // v == 1.0
acc_f32_destroy(obj);
```
 


    
## Public Functions Documentation




### function acc\_f32\_add2d 

_Sum all elements of a (logically) 2-D float array into the accumulator. The array is treated as a flat C-order buffer of_ `x_len` _floats regardless of the original shape; the caller is responsible for passing the total element count._
```C++
void acc_f32_add2d (
    acc_f32_state_t * state,
    const float * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input array (float32, any shape — passed as flat buffer). 
* `x_len` Number of elements in `x`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> grid = np.array([[1.0, 2.0], [3.0, 4.0]], dtype=np.float32)
>>> obj.add2d(grid)
>>> obj.get()
10.0
```
 




        

<hr>



### function acc\_f32\_create 

_Single-precision floating-point scalar accumulator. Maintains one running sum (_ `acc` _) that persists across calls to_`step` _,_`steps` _,_`madd` _,_`add2d` _, and_`madd2d` _. Use_`get` _to read without side-effects or_`dump` _to read and atomically zero in a single call._
```C++
acc_f32_state_t * acc_f32_create (
    float acc
) 
```





**Parameters:**


* `acc` Initial accumulator value (default: 0.0). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_f32\_destroy()**](acc__f32__core_8h.md#function-acc_f32_destroy) when done. 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.get_acc()
0.0
>>> obj.set_acc(5.0)
>>> obj.get_acc()
5.0
>>> obj.reset()
>>> obj.get_acc()
0.0
```
 





        

<hr>



### function acc\_f32\_destroy 

_Release all memory owned by an AccF32 instance. Passing NULL is safe; the function is a no-op in that case. After this call the pointer must not be used._ 
```C++
void acc_f32_destroy (
    acc_f32_state_t * state
) 
```




<hr>



### function acc\_f32\_dump 

_Return the accumulated sum and atomically reset it to zero. This is the canonical "drain" primitive: read the period total, then start a fresh accumulation interval without a separate_ `reset` _call. The zero-reset is unconditional and always writes 0.0f._
```C++
float acc_f32_dump (
    acc_f32_state_t * state
) 
```





**Returns:**

Value of `acc` just before the reset (float). 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.step(3.0)
>>> obj.step(4.0)
>>> obj.dump()
7.0
>>> obj.get()
0.0
```
 





        

<hr>



### function acc\_f32\_get 

_Return the current accumulated sum without resetting state. Identical to reading the_ `acc` _property directly; retained as an explicit method so call sites that need the value can be uniform with_`dump` _without a conditional._
```C++
float acc_f32_get (
    acc_f32_state_t * state
) 
```





**Returns:**

Current value of `acc` (float). 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.step(2.0)
>>> obj.step(3.0)
>>> obj.get()
5.0
```
 





        

<hr>



### function acc\_f32\_get\_acc 

_Return the current accumulator value without modifying state. Use this when you need to read the running sum mid-accumulation without disturbing it. For a read-and-reset in one call use_ `acc_f32_dump` _._
```C++
float acc_f32_get_acc (
    const acc_f32_state_t * state
) 
```





**Returns:**

Current value of `acc` (float). 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.step(4.0)
>>> obj.get_acc()
4.0
```
 





        

<hr>



### function acc\_f32\_get\_state 

```C++
void acc_f32_get_state (
    const acc_f32_state_t * state,
    void * blob
) 
```




<hr>



### function acc\_f32\_madd 

_Dot-product accumulate:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. The shorter of the two arrays limits the iteration count; no out-of-bounds access occurs. Typical use: apply a short FIR weight vector to one block of signal samples and fold the result into a running total._
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
* `x` Signal samples (float32 array). 
* `x_len` Number of elements in `x`. 
* `h` Coefficient / weight array (float32 array). 
* `h_len` Number of elements in `h`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
>>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
>>> obj.madd(x, h)
>>> obj.get()
5.0
```
 




        

<hr>



### function acc\_f32\_madd2d 

_Dot-product accumulate over a flat 2-D buffer:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. Combines_`add2d` _and_`madd` _semantics — a 2-D signal array is weighted element-wise by a coefficient buffer and the scalar total is folded into the running sum._
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
* `x` Signal samples (float32, flat buffer of the 2-D array). 
* `x_len` Number of elements in `x`. 
* `h` Coefficient / weight array (float32). 
* `h_len` Number of elements in `h`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> x = np.array([1.0, 2.0, 3.0, 4.0], dtype=np.float32)
>>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
>>> obj.madd2d(x, h)
>>> obj.get()
5.0
```
 




        

<hr>



### function acc\_f32\_reset 

_Zero the accumulator, restoring the same state as a fresh_ `AccF32(0.0)` _— regardless of the value supplied to_`acc_f32_create` _. Subsequent_`get` _/_`dump` _calls return_`0.0` _until new samples are processed._
```C++
void acc_f32_reset (
    acc_f32_state_t * state
) 
```




```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.step(7.0)
>>> obj.reset()
>>> obj.get_acc()
0.0
```
 


        

<hr>



### function acc\_f32\_set\_acc 

_Overwrite the accumulator with a new value. Useful for seeding the accumulator to a known baseline before processing a new segment without a full_ `reset` _._
```C++
void acc_f32_set_acc (
    acc_f32_state_t * state,
    float acc
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `acc` New accumulator value. 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.set_acc(10.0)
>>> obj.get_acc()
10.0
```
 




        

<hr>



### function acc\_f32\_set\_state 

```C++
int acc_f32_set_state (
    acc_f32_state_t * state,
    const void * blob
) 
```




<hr>



### function acc\_f32\_state\_bytes 

```C++
size_t acc_f32_state_bytes (
    const acc_f32_state_t * state
) 
```




<hr>



### function acc\_f32\_step 

_Add one sample to the running sum (_ `acc += x` _). This is the hot-path entry point for sample-by-sample processing. For block inputs prefer_`acc_f32_steps` _to amortise call overhead and allow auto-vectorisation._
```C++
JM_FORCEINLINE  JM_HOT void acc_f32_step (
    acc_f32_state_t * state,
    float x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (float). 
```C++
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.step(3.0)
>>> obj.get()
3.0
```
 




        

<hr>



### function acc\_f32\_steps 

_Add all samples in_ `input` _to the running sum. Equivalent to calling_`acc_f32_step` _for each element, but SIMD-vectorised on platforms that provide it (AVX-512 / AVX2 / SSE2). The loop uses JM\_RESTRICT so the compiler can assume no aliasing between_`state` _and_`input` _._
```C++
void acc_f32_steps (
    acc_f32_state_t * state,
    const float * input,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `input` Input samples (float32 array). 
* `n` Number of elements in `input`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccF32
>>> obj = AccF32(0.0)
>>> obj.steps(np.array([1.0, 2.0, 3.0], dtype=np.float32))
>>> obj.get()
6.0
```
 




        

<hr>
## Macro Definition Documentation





### define ACC\_F32\_STATE\_MAGIC 

```C++
#define ACC_F32_STATE_MAGIC `DP_FOURCC ('A', 'C', 'C', 'F')`
```




<hr>



### define ACC\_F32\_STATE\_VERSION 

```C++
#define ACC_F32_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_f32/acc_f32_core.h`

