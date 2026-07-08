

# File acc\_cf64\_core.h



[**FileList**](files.md) **>** [**acc\_cf64**](dir_a31d3897e2036bab462df07bf5a3b557.md) **>** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md)

[Go to the source code of this file](acc__cf64__core_8h_source.md)

_AccCf64 component API._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) <br>_AccCf64 state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acc\_cf64\_add2d**](#function-acc_cf64_add2d) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len) <br>_Sum all elements of a (logically) 2-D complex array into the accumulator. The array is treated as a flat C-order buffer of_ `x_len` _complex128 samples regardless of the original shape; the caller is responsible for passing the total element count._ |
|  [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* | [**acc\_cf64\_create**](#function-acc_cf64_create) (double \_Complex acc) <br>_Double-precision complex scalar accumulator. Maintains one running complex sum (_ `acc` _) across calls to_`step` _,_`steps` _,_`madd` _,_`add2d` _, and_`madd2d` _. The signal path is double-precision complex (128-bit per sample); coefficient arrays for_`madd` _/_`madd2d` _are single-precision float to match typical FIR weight storage. Use_`get` _to read without side-effects or_`dump` _to read and zero atomically._ |
|  void | [**acc\_cf64\_destroy**](#function-acc_cf64_destroy) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Release all memory owned by an AccCf64 instance. Passing NULL is safe; the function is a no-op in that case. After this call the pointer must not be used._  |
|  double complex | [**acc\_cf64\_dump**](#function-acc_cf64_dump) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Return the accumulated sum and atomically reset it to zero. This is the canonical "drain" primitive: read the period total, then start a fresh accumulation interval without a separate_ `reset` _call. Both real and imaginary parts are zeroed unconditionally._ |
|  double complex | [**acc\_cf64\_get**](#function-acc_cf64_get) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Return the current accumulated sum without resetting state. Identical to reading the_ `acc` _property directly; retained as an explicit method so call sites that need the value can be uniform with_`dump` _without a conditional._ |
|  double \_Complex | [**acc\_cf64\_get\_acc**](#function-acc_cf64_get_acc) (const [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Return the current accumulator value without modifying state. Use this when you need to read the running sum mid-accumulation without disturbing it. For a read-and-reset in one call use_ `acc_cf64_dump` _._ |
|  void | [**acc\_cf64\_get\_state**](#function-acc_cf64_get_state) (const [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, void \* blob) <br> |
|  void | [**acc\_cf64\_madd**](#function-acc_cf64_madd) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_Dot-product accumulate with complex signal and float weights:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. The signal array_`x` _is double-precision complex; the coefficient array_`h` _is single-precision float (widened to double before multiplication). The shorter of the two arrays limits iteration._ |
|  void | [**acc\_cf64\_madd2d**](#function-acc_cf64_madd2d) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* x, size\_t x\_len, const float \* h, size\_t h\_len) <br>_Dot-product accumulate over a flat 2-D complex buffer:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. Combines_`add2d` _and_`madd` _semantics for 2-D data — a complex signal grid is weighted element-wise by a real coefficient buffer and folded into the running sum._ |
|  void | [**acc\_cf64\_reset**](#function-acc_cf64_reset) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br>_Zero the accumulator, restoring the same state as a fresh_ `AccCf64(0j)` _— regardless of the value supplied to_`acc_cf64_create` _. Both the real and imaginary parts are set to 0.0. Subsequent_`get` _/_`dump` _calls return_`0j` _until new samples are processed._ |
|  void | [**acc\_cf64\_set\_acc**](#function-acc_cf64_set_acc) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, double \_Complex acc) <br>_Overwrite the accumulator with a new complex value. Useful for seeding the accumulator to a known baseline before processing a new segment without a full_ `reset` _._ |
|  int | [**acc\_cf64\_set\_state**](#function-acc_cf64_set_state) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**acc\_cf64\_state\_bytes**](#function-acc_cf64_state_bytes) (const [**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state) <br> |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) void | [**acc\_cf64\_step**](#function-acc_cf64_step) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, double complex x) <br>_Add one complex sample to the running sum (_ `acc += x` _). This is the hot-path entry for sample-by-sample processing. For block inputs prefer_`acc_cf64_steps` _to amortise call overhead._ |
|  void | [**acc\_cf64\_steps**](#function-acc_cf64_steps) ([**acc\_cf64\_state\_t**](structacc__cf64__state__t.md) \* state, const double complex \* input, size\_t n) <br>_Add all samples in_ `input` _to the running sum. Equivalent to calling_`acc_cf64_step` _for each element; iterates element-by-element over double-precision complex samples._ |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACC\_CF64\_STATE\_MAGIC**](acc__cf64__core_8h.md#define-acc_cf64_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A', 'C', 'C', 'C')`<br> |
| define  | [**ACC\_CF64\_STATE\_VERSION**](acc__cf64__core_8h.md#define-acc_cf64_state_version)  `1u`<br> |

## Detailed Description


Lifecycle: create -&gt; (step / steps / reset)\* -&gt; destroy


Example: 
```C++
acc_cf64_state_t *obj = acc_cf64_create(0.0 + 0.0 * I);
acc_cf64_step(obj, 1.0 + 0.5 * I);
double complex v = acc_cf64_get(obj);  // v == 1.0 + 0.5 * I
acc_cf64_destroy(obj);
```
 


    
## Public Functions Documentation




### function acc\_cf64\_add2d 

_Sum all elements of a (logically) 2-D complex array into the accumulator. The array is treated as a flat C-order buffer of_ `x_len` _complex128 samples regardless of the original shape; the caller is responsible for passing the total element count._
```C++
void acc_cf64_add2d (
    acc_cf64_state_t * state,
    const double complex * x,
    size_t x_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input array (complex128, any shape — passed as flat buffer). 
* `x_len` Number of elements in `x`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> grid = np.array([[1+0j, 2+0j], [3+0j, 4+0j]], dtype=np.complex128)
>>> obj.add2d(grid)
>>> obj.get()
(10+0j)
```
 




        

<hr>



### function acc\_cf64\_create 

_Double-precision complex scalar accumulator. Maintains one running complex sum (_ `acc` _) across calls to_`step` _,_`steps` _,_`madd` _,_`add2d` _, and_`madd2d` _. The signal path is double-precision complex (128-bit per sample); coefficient arrays for_`madd` _/_`madd2d` _are single-precision float to match typical FIR weight storage. Use_`get` _to read without side-effects or_`dump` _to read and zero atomically._
```C++
acc_cf64_state_t * acc_cf64_create (
    double _Complex acc
) 
```





**Parameters:**


* `acc` Initial accumulator value (default: 0j). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 




**Note:**

Caller must call [**acc\_cf64\_destroy()**](acc__cf64__core_8h.md#function-acc_cf64_destroy) when done. 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.get_acc()
0j
>>> obj.set_acc(3+4j)
>>> obj.get_acc()
(3+4j)
>>> obj.reset()
>>> obj.get_acc()
0j
```
 





        

<hr>



### function acc\_cf64\_destroy 

_Release all memory owned by an AccCf64 instance. Passing NULL is safe; the function is a no-op in that case. After this call the pointer must not be used._ 
```C++
void acc_cf64_destroy (
    acc_cf64_state_t * state
) 
```




<hr>



### function acc\_cf64\_dump 

_Return the accumulated sum and atomically reset it to zero. This is the canonical "drain" primitive: read the period total, then start a fresh accumulation interval without a separate_ `reset` _call. Both real and imaginary parts are zeroed unconditionally._
```C++
double complex acc_cf64_dump (
    acc_cf64_state_t * state
) 
```





**Returns:**

Value of `acc` just before the reset (complex). 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.step(3+2j)
>>> obj.step(1+1j)
>>> obj.dump()
(4+3j)
>>> obj.get()
0j
```
 





        

<hr>



### function acc\_cf64\_get 

_Return the current accumulated sum without resetting state. Identical to reading the_ `acc` _property directly; retained as an explicit method so call sites that need the value can be uniform with_`dump` _without a conditional._
```C++
double complex acc_cf64_get (
    acc_cf64_state_t * state
) 
```





**Returns:**

Current value of `acc` (complex). 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.step(2+0j)
>>> obj.step(0+3j)
>>> obj.get()
(2+3j)
```
 





        

<hr>



### function acc\_cf64\_get\_acc 

_Return the current accumulator value without modifying state. Use this when you need to read the running sum mid-accumulation without disturbing it. For a read-and-reset in one call use_ `acc_cf64_dump` _._
```C++
double _Complex acc_cf64_get_acc (
    const acc_cf64_state_t * state
) 
```





**Returns:**

Current value of `acc` (complex). 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.step(1+2j)
>>> obj.get_acc()
(1+2j)
```
 





        

<hr>



### function acc\_cf64\_get\_state 

```C++
void acc_cf64_get_state (
    const acc_cf64_state_t * state,
    void * blob
) 
```




<hr>



### function acc\_cf64\_madd 

_Dot-product accumulate with complex signal and float weights:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. The signal array_`x` _is double-precision complex; the coefficient array_`h` _is single-precision float (widened to double before multiplication). The shorter of the two arrays limits iteration._
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
* `x` Complex signal samples (complex128 array). 
* `x_len` Number of elements in `x`. 
* `h` Real coefficient / weight array (float32 array). 
* `h_len` Number of elements in `h`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
>>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
>>> obj.madd(x, h)
>>> obj.get()
(5+0j)
```
 




        

<hr>



### function acc\_cf64\_madd2d 

_Dot-product accumulate over a flat 2-D complex buffer:_ `acc += sum(x[i] * h[i])` _for_`i` _in_`0 .. min(x_len, h_len) - 1` _. Combines_`add2d` _and_`madd` _semantics for 2-D data — a complex signal grid is weighted element-wise by a real coefficient buffer and folded into the running sum._
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
* `x` Complex signal samples (complex128, flat buffer). 
* `x_len` Number of elements in `x`. 
* `h` Real coefficient / weight array (float32). 
* `h_len` Number of elements in `h`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> x = np.array([1+0j, 2+0j, 3+0j, 4+0j], dtype=np.complex128)
>>> h = np.array([0.5, 0.5, 0.5, 0.5], dtype=np.float32)
>>> obj.madd2d(x, h)
>>> obj.get()
(5+0j)
```
 




        

<hr>



### function acc\_cf64\_reset 

_Zero the accumulator, restoring the same state as a fresh_ `AccCf64(0j)` _— regardless of the value supplied to_`acc_cf64_create` _. Both the real and imaginary parts are set to 0.0. Subsequent_`get` _/_`dump` _calls return_`0j` _until new samples are processed._
```C++
void acc_cf64_reset (
    acc_cf64_state_t * state
) 
```




```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.step(3+2j)
>>> obj.reset()
>>> obj.get_acc()
0j
```
 


        

<hr>



### function acc\_cf64\_set\_acc 

_Overwrite the accumulator with a new complex value. Useful for seeding the accumulator to a known baseline before processing a new segment without a full_ `reset` _._
```C++
void acc_cf64_set_acc (
    acc_cf64_state_t * state,
    double _Complex acc
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `acc` New accumulator value (complex). 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.set_acc(5+6j)
>>> obj.get_acc()
(5+6j)
```
 




        

<hr>



### function acc\_cf64\_set\_state 

```C++
int acc_cf64_set_state (
    acc_cf64_state_t * state,
    const void * blob
) 
```




<hr>



### function acc\_cf64\_state\_bytes 

```C++
size_t acc_cf64_state_bytes (
    const acc_cf64_state_t * state
) 
```




<hr>



### function acc\_cf64\_step 

_Add one complex sample to the running sum (_ `acc += x` _). This is the hot-path entry for sample-by-sample processing. For block inputs prefer_`acc_cf64_steps` _to amortise call overhead._
```C++
JM_FORCEINLINE  JM_HOT void acc_cf64_step (
    acc_cf64_state_t * state,
    double complex x
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `x` Input sample (complex). 
```C++
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.step(3+2j)
>>> obj.get()
(3+2j)
```
 




        

<hr>



### function acc\_cf64\_steps 

_Add all samples in_ `input` _to the running sum. Equivalent to calling_`acc_cf64_step` _for each element; iterates element-by-element over double-precision complex samples._
```C++
void acc_cf64_steps (
    acc_cf64_state_t * state,
    const double complex * input,
    size_t n
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `input` Input samples (complex128 array). 
* `n` Number of elements in `input`. 
```C++
>>> import numpy as np
>>> from doppler.accumulator import AccCf64
>>> obj = AccCf64(0j)
>>> obj.steps(np.array([1+0j, 2+1j, 3+2j], dtype=np.complex128))
>>> obj.get()
(6+3j)
```
 




        

<hr>
## Macro Definition Documentation





### define ACC\_CF64\_STATE\_MAGIC 

```C++
#define ACC_CF64_STATE_MAGIC `DP_FOURCC ('A', 'C', 'C', 'C')`
```




<hr>



### define ACC\_CF64\_STATE\_VERSION 

```C++
#define ACC_CF64_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_cf64/acc_cf64_core.h`

