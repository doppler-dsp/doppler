

# File acc\_trace\_core.h



[**FileList**](files.md) **>** [**acc\_trace**](dir_51e33d48c4bde6f60a2f27e75677a784.md) **>** [**acc\_trace\_core.h**](acc__trace__core_8h.md)

[Go to the source code of this file](acc__trace__core_8h_source.md)

_AccTrace — per-bin vector trace accumulator._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "jm_perf.h"`
* `#include "dp_state.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**acc\_trace\_state\_t**](structacc__trace__state__t.md) <br>_AccTrace state. Allocate with_ [_**acc\_trace\_create()**_](acc__trace__core_8h.md#function-acc_trace_create) _._ |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**acc\_trace\_mode\_t**](#enum-acc_trace_mode_t)  <br>_Trace reduction mode. Values match the Python string-enum index order ("mean", "exp", "maxhold", "minhold")._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**acc\_trace\_accumulate**](#function-acc_trace_accumulate) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state, const float \* p, size\_t p\_len) <br>_Fold one length-n frame into the running trace. Frames shorter than_ `n` _are ignored; if_`p_len` _exceeds_`n` _only the first_`n` _samples are used. The first accumulated frame seeds the trace directly (every mode), so a single frame followed by value() returns that frame unchanged._ |
|  [**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* | [**acc\_trace\_create**](#function-acc_trace_create) (size\_t n, int mode, double alpha) <br>_Create a length-_ `n` _trace accumulator._ |
|  void | [**acc\_trace\_destroy**](#function-acc_trace_destroy) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state) <br>_Destroy an AccTrace instance and release all memory._  |
|  void | [**acc\_trace\_get\_state**](#function-acc_trace_get_state) (const [**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state, void \* blob) <br> |
|  void | [**acc\_trace\_reset**](#function-acc_trace_reset) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state) <br>_Discard the running trace; the next accumulate re-seeds it. The mode, alpha, and length are preserved._  |
|  int | [**acc\_trace\_set\_state**](#function-acc_trace_set_state) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**acc\_trace\_state\_bytes**](#function-acc_trace_state_bytes) (const [**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state) <br> |
|  size\_t | [**acc\_trace\_value**](#function-acc_trace_value) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state, size\_t n, float \* out) <br>_Copy the current averaged trace into_ `out` _. Writes the full length-n trace and returns n. Returns 0 (which the Python wrapper renders as None) before any frame has been accumulated._ |
|  size\_t | [**acc\_trace\_value\_max\_out**](#function-acc_trace_value_max_out) ([**acc\_trace\_state\_t**](structacc__trace__state__t.md) \* state) <br>_Output capacity hint for value(); equals the trace length n._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**ACC\_TRACE\_STATE\_MAGIC**](acc__trace__core_8h.md#define-acc_trace_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('A','T','R','C')`<br> |
| define  | [**ACC\_TRACE\_STATE\_VERSION**](acc__trace__core_8h.md#define-acc_trace_state_version)  `1u`<br> |

## Detailed Description


Folds a stream of equal-length frames into a single running trace using one of four reduction modes: linear mean, exponential moving average (EMA), max-hold, or min-hold. Where the scalar `acc_f32` / `acc_cf64` accumulators reduce a frame to one running sum, AccTrace keeps a value _per bin_, which is what spectrum averaging, waterfalls/spectrograms, and video-averaged displays need. Accumulation is done in double precision regardless of the float32 input/output to keep the running mean numerically stable over long captures.


The first frame seeds the trace in every mode; subsequent frames update it:
* mean : `acc += (p - acc) / count` (Welford running mean)
* exp : `acc = alpha*p + (1-alpha)*acc` (EMA)
* maxhold : `acc = max(acc, p)` per bin
* minhold : `acc = min(acc, p)` per bin




Lifecycle: create -&gt; (accumulate / reset)\* -&gt; value -&gt; destroy 


    
## Public Types Documentation




### enum acc\_trace\_mode\_t 

_Trace reduction mode. Values match the Python string-enum index order ("mean", "exp", "maxhold", "minhold")._ 
```C++
enum acc_trace_mode_t {
    ACC_TRACE_MEAN = 0,
    ACC_TRACE_EXP = 1,
    ACC_TRACE_MAXHOLD = 2,
    ACC_TRACE_MINHOLD = 3
};
```




<hr>
## Public Functions Documentation




### function acc\_trace\_accumulate 

_Fold one length-n frame into the running trace. Frames shorter than_ `n` _are ignored; if_`p_len` _exceeds_`n` _only the first_`n` _samples are used. The first accumulated frame seeds the trace directly (every mode), so a single frame followed by value() returns that frame unchanged._
```C++
void acc_trace_accumulate (
    acc_trace_state_t * state,
    const float * p,
    size_t p_len
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `p` Input frame (float32). 
* `p_len` Number of samples in `p`; must be &gt;= n to take effect.


```C++
>>> import numpy as np
>>> from doppler.accumulator import AccTrace
>>> acc = AccTrace(n=4, mode="mean")
>>> acc.accumulate(np.array([1, 3, 5, 7], dtype=np.float32))
>>> acc.accumulate(np.array([3, 5, 7, 9], dtype=np.float32))
>>> acc.value().tolist()
[2.0, 4.0, 6.0, 8.0]
```
 


        

<hr>



### function acc\_trace\_create 

_Create a length-_ `n` _trace accumulator._
```C++
acc_trace_state_t * acc_trace_create (
    size_t n,
    int mode,
    double alpha
) 
```





**Parameters:**


* `n` Trace length in bins. Must be &gt; 0; returns NULL otherwise. 
* `mode` Reduction mode index (0=mean, 1=exp, 2=maxhold, 3=minhold). 
* `alpha` EMA smoothing factor used only by `exp` mode (0 &lt; alpha &lt;= 1). 



**Returns:**

Heap-allocated state, or NULL on invalid argument or OOM. 




**Note:**

Caller must call [**acc\_trace\_destroy()**](acc__trace__core_8h.md#function-acc_trace_destroy) when done.



```C++
>>> from doppler.accumulator import AccTrace
>>> acc = AccTrace(n=8, mode="mean")
>>> acc.n, acc.count
(8, 0)
```
 


        

<hr>



### function acc\_trace\_destroy 

_Destroy an AccTrace instance and release all memory._ 
```C++
void acc_trace_destroy (
    acc_trace_state_t * state
) 
```





**Parameters:**


* `state` May be NULL (no-op). 




        

<hr>



### function acc\_trace\_get\_state 

```C++
void acc_trace_get_state (
    const acc_trace_state_t * state,
    void * blob
) 
```




<hr>



### function acc\_trace\_reset 

_Discard the running trace; the next accumulate re-seeds it. The mode, alpha, and length are preserved._ 
```C++
void acc_trace_reset (
    acc_trace_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


```C++
>>> import numpy as np
>>> from doppler.accumulator import AccTrace
>>> acc = AccTrace(n=4, mode="mean")
>>> acc.accumulate(np.ones(4, dtype=np.float32))
>>> acc.reset()
>>> acc.count
0
```
 


        

<hr>



### function acc\_trace\_set\_state 

```C++
int acc_trace_set_state (
    acc_trace_state_t * state,
    const void * blob
) 
```




<hr>



### function acc\_trace\_state\_bytes 

```C++
size_t acc_trace_state_bytes (
    const acc_trace_state_t * state
) 
```




<hr>



### function acc\_trace\_value 

_Copy the current averaged trace into_ `out` _. Writes the full length-n trace and returns n. Returns 0 (which the Python wrapper renders as None) before any frame has been accumulated._
```C++
size_t acc_trace_value (
    acc_trace_state_t * state,
    size_t n,
    float * out
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `n` Caller buffer capacity (ignored; buffer is pre-sized to n). 
* `out` Destination, at least n float32 elements. 



**Returns:**

Number of samples written (n, or 0 if empty).



```C++
>>> import numpy as np
>>> from doppler.accumulator import AccTrace
>>> acc = AccTrace(n=3, mode="maxhold")
>>> acc.accumulate(np.array([1, 5, 2], dtype=np.float32))
>>> acc.accumulate(np.array([4, 3, 6], dtype=np.float32))
>>> acc.value().tolist()
[4.0, 5.0, 6.0]
```
 


        

<hr>



### function acc\_trace\_value\_max\_out 

_Output capacity hint for value(); equals the trace length n._ 
```C++
size_t acc_trace_value_max_out (
    acc_trace_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define ACC\_TRACE\_STATE\_MAGIC 

```C++
#define ACC_TRACE_STATE_MAGIC `DP_FOURCC ('A','T','R','C')`
```




<hr>



### define ACC\_TRACE\_STATE\_VERSION 

```C++
#define ACC_TRACE_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acc_trace/acc_trace_core.h`

