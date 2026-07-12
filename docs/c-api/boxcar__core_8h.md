

# File boxcar\_core.h



[**FileList**](files.md) **>** [**boxcar**](dir_4075e3d5389fc37fde93604059f4dd85.md) **>** [**boxcar\_core.h**](boxcar__core_8h.md)

[Go to the source code of this file](boxcar__core_8h_source.md)

_Boxcar (rectangular) moving-average filter — cf32, fixed window._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**boxcar\_state\_t**](structboxcar__state__t.md) <br>_Boxcar moving-average state (cf32)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**boxcar\_state\_t**](structboxcar__state__t.md) \* | [**boxcar\_create**](#function-boxcar_create) (size\_t len, double gain) <br>_Create a boxcar instance._  |
|  void | [**boxcar\_destroy**](#function-boxcar_destroy) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s) <br>_Destroy a boxcar instance._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**boxcar\_get\_gain**](#function-boxcar_get_gain) (const [**boxcar\_state\_t**](structboxcar__state__t.md) \* s) <br>_Current output gain._  |
|  void | [**boxcar\_get\_state**](#function-boxcar_get_state) (const [**boxcar\_state\_t**](structboxcar__state__t.md) \* s, void \* blob) <br>_Serialize the full state into_ `blob` _._ |
|  void | [**boxcar\_init**](#function-boxcar_init) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s, size\_t len, double gain) <br>_Initialise a boxcar in place (no allocation)._  |
|  void | [**boxcar\_reset**](#function-boxcar_reset) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s) <br>_Clear the window (zero the ring and the running sum); keep config._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**boxcar\_set\_gain**](#function-boxcar_set_gain) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s, double gain) <br>_Set the output gain; refresh the cached scale._  |
|  int | [**boxcar\_set\_state**](#function-boxcar_set_state) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**boxcar\_state\_bytes**](#function-boxcar_state_bytes) (const [**boxcar\_state\_t**](structboxcar__state__t.md) \* s) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float complex | [**boxcar\_step**](#function-boxcar_step) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s, float complex x) <br>_Slide the window by one sample; return the gained moving average._  |
|  void | [**boxcar\_steps**](#function-boxcar_steps) ([**boxcar\_state\_t**](structboxcar__state__t.md) \* s, const float complex \* input, float complex \* output, size\_t n) <br>_Filter a block: write the gained moving average of each sample._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**BOXCAR\_MAX\_LEN**](boxcar__core_8h.md#define-boxcar_max_len)  `64`<br> |
| define  | [**BOXCAR\_STATE\_MAGIC**](boxcar__core_8h.md#define-boxcar_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('B', 'O', 'X', 'C')`<br> |
| define  | [**BOXCAR\_STATE\_VERSION**](boxcar__core_8h.md#define-boxcar_state_version)  `1u`<br> |

## Detailed Description


A sliding-window moving average over the last `len` complex samples: one output per input sample (no rate change). Each step adds the new sample and subtracts the sample leaving the window, so it is O(1) per sample regardless of window length (a running window sum, not a re-summed convolution). The output is the window mean times an optional output `gain`: `gain·(Σ window)/ len`. Because the step must multiply by `1/len` anyway, the gain is folded into a single cached `scale = gain/len`, so applying it is free — a composing loop (e.g. a carrier arm with an AGC) can push its gain into the boxcar and avoid a second multiply.


The delay ring is a **fixed in-struct array** (`BOXCAR_MAX_LEN`), so the state is pointer-free POD: it embeds by value into a composing object (a carrier loop's I/Q arm, a smoother ahead of a detector) and serializes as a whole-struct snapshot. A window longer than `BOXCAR_MAX_LEN` is rejected at create/init time. (A bounded window sum also stays numerically clean — unlike a never-reset CIC integrator-comb, whose integrator drifts in float.)


Until the ring fills (the first `len-1` samples after a reset) the ring holds zeros, so the average is taken over a partial window and the output ramps in.



```C++
>>> import numpy as np
>>> from doppler.filter import MovingAverage
>>> ma = MovingAverage(2)                       # 2-sample window, unit gain
>>> ma.steps(np.ones(3, np.complex64)).real.tolist()
[0.5, 1.0, 1.0]
>>> ma2 = MovingAverage(2, gain=2.0)            # gain folded into the mean
>>> ma2.steps(np.ones(3, np.complex64)).real.tolist()
[1.0, 2.0, 2.0]
```
 


    
## Public Functions Documentation




### function boxcar\_create 

_Create a boxcar instance._ 
```C++
boxcar_state_t * boxcar_create (
    size_t len,
    double gain
) 
```





**Parameters:**


* `len` Window length (1 .. BOXCAR\_MAX\_LEN; default 4). 
* `gain` Output gain (default 1.0). 



**Returns:**

Heap state, or NULL on invalid length / allocation failure. 




**Note:**

Caller must call [**boxcar\_destroy()**](boxcar__core_8h.md#function-boxcar_destroy) when done. 





        

<hr>



### function boxcar\_destroy 

_Destroy a boxcar instance._ 
```C++
void boxcar_destroy (
    boxcar_state_t * s
) 
```





**Parameters:**


* `s` May be NULL. 




        

<hr>



### function boxcar\_get\_gain 

_Current output gain._ 
```C++
JM_FORCEINLINE double boxcar_get_gain (
    const boxcar_state_t * s
) 
```




<hr>



### function boxcar\_get\_state 

_Serialize the full state into_ `blob` _._
```C++
void boxcar_get_state (
    const boxcar_state_t * s,
    void * blob
) 
```




<hr>



### function boxcar\_init 

_Initialise a boxcar in place (no allocation)._ 
```C++
void boxcar_init (
    boxcar_state_t * s,
    size_t len,
    double gain
) 
```





**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `len` Window length; clamped to `[1,  BOXCAR_MAX_LEN ]`. 
* `gain` Output gain (folded into the averaging scale). 




        

<hr>



### function boxcar\_reset 

_Clear the window (zero the ring and the running sum); keep config._ 
```C++
void boxcar_reset (
    boxcar_state_t * s
) 
```




<hr>



### function boxcar\_set\_gain 

_Set the output gain; refresh the cached scale._ 
```C++
JM_FORCEINLINE void boxcar_set_gain (
    boxcar_state_t * s,
    double gain
) 
```





**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `gain` New output gain (folded into `scale = gain / len`). 




        

<hr>



### function boxcar\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int boxcar_set_state (
    boxcar_state_t * s,
    const void * blob
) 
```




<hr>



### function boxcar\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t boxcar_state_bytes (
    const boxcar_state_t * s
) 
```




<hr>



### function boxcar\_step 

_Slide the window by one sample; return the gained moving average._ 
```C++
JM_FORCEINLINE  JM_HOT float complex boxcar_step (
    boxcar_state_t * s,
    float complex x
) 
```



O(1): add `x`, drop the sample leaving the window, return `acc · scale` (= `gain · acc / len`) — one multiply.




**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The gained window mean after admitting `x`. 





        

<hr>



### function boxcar\_steps 

_Filter a block: write the gained moving average of each sample._ 
```C++
void boxcar_steps (
    boxcar_state_t * s,
    const float complex * input,
    float complex * output,
    size_t n
) 
```





**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `input` Input samples. 
* `output` Output (gained window means); may alias `input`. 
* `n` Number of samples. 




        

<hr>
## Macro Definition Documentation





### define BOXCAR\_MAX\_LEN 

```C++
#define BOXCAR_MAX_LEN `64`
```




<hr>



### define BOXCAR\_STATE\_MAGIC 

```C++
#define BOXCAR_STATE_MAGIC `DP_FOURCC ('B', 'O', 'X', 'C')`
```




<hr>



### define BOXCAR\_STATE\_VERSION 

```C++
#define BOXCAR_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/boxcar/boxcar_core.h`

