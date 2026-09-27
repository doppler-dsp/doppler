

# File boxcar\_core.h



[**FileList**](files.md) **>** [**boxcar**](dir_5b2ea30dc12e54f23750507f860119fd.md) **>** [**boxcar\_core.h**](boxcar__core_8h.md)

[Go to the source code of this file](boxcar__core_8h_source.md)

_Boxcar (rectangular) moving-average filter — cf32, fixed window._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/jm_perf.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) <br>_Boxcar moving-average state (cf32)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**boxcar\_init**](#function-boxcar_init) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, size\_t len, double gain) <br>_Initialise a boxcar in place (no allocation)._  |
|  [**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* | [**dp\_boxcar\_create**](#function-dp_boxcar_create) (size\_t len, double gain) <br>_Create a boxcar instance._  |
|  void | [**dp\_boxcar\_destroy**](#function-dp_boxcar_destroy) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s) <br>_Destroy a boxcar instance._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) double | [**dp\_boxcar\_get\_gain**](#function-dp_boxcar_get_gain) (const [**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s) <br>_Current output gain._  |
|  void | [**dp\_boxcar\_get\_state**](#function-dp_boxcar_get_state) (const [**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, void \* blob) <br>_Serialize the full state into_ `blob` _._ |
|  void | [**dp\_boxcar\_reset**](#function-dp_boxcar_reset) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s) <br>_Clear the window (zero the ring and the running sum); keep the configured length and gain._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) void | [**dp\_boxcar\_set\_gain**](#function-dp_boxcar_set_gain) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, double gain) <br>_Set the output gain; refresh the cached scale._  |
|  int | [**dp\_boxcar\_set\_state**](#function-dp_boxcar_set_state) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, const void \* blob) <br>_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._  |
|  size\_t | [**dp\_boxcar\_state\_bytes**](#function-dp_boxcar_state_bytes) (const [**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s) <br>_Serialized-state byte size._  |
|  [**JM\_FORCEINLINE**](jm__perf_8h.md#define-jm_forceinline) [**JM\_HOT**](jm__perf_8h.md#define-jm_hot) float \_Complex | [**dp\_boxcar\_step**](#function-dp_boxcar_step) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, float \_Complex x) <br>_Slide the window by one sample; return the gained moving average._  |
|  void | [**dp\_boxcar\_steps**](#function-dp_boxcar_steps) ([**dp\_boxcar\_state\_t**](structdp__boxcar__state__t.md) \* s, const float \_Complex \* x, float \_Complex \* out, size\_t n) <br>_Filter a block: write the gained moving average of each sample._  |



























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
>>> ma = MovingAverage(2)              # 2-sample window, unit gain
>>> ma.steps(np.ones(3, np.complex64)).real.tolist()
[0.5, 1.0, 1.0]
>>> ma2 = MovingAverage(2, gain=2.0)   # gain folded into the mean
>>> ma2.steps(np.ones(3, np.complex64)).real.tolist()
[1.0, 2.0, 2.0]
```
 


    
## Public Functions Documentation




### function boxcar\_init 

_Initialise a boxcar in place (no allocation)._ 
```C++
void boxcar_init (
    dp_boxcar_state_t * s,
    size_t len,
    double gain
) 
```





**Parameters:**


* `s` State to initialise. Must be non-NULL. 
* `len` Window length; clamped to `[1, BOXCAR_MAX_LEN]`. 
* `gain` Output gain (folded into the averaging scale). 




        

<hr>



### function dp\_boxcar\_create 

_Create a boxcar instance._ 
```C++
dp_boxcar_state_t * dp_boxcar_create (
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

Caller must call [**dp\_boxcar\_destroy()**](boxcar__core_8h.md#function-dp_boxcar_destroy) when done. 





        

<hr>



### function dp\_boxcar\_destroy 

_Destroy a boxcar instance._ 
```C++
void dp_boxcar_destroy (
    dp_boxcar_state_t * s
) 
```





**Parameters:**


* `s` May be NULL. 




        

<hr>



### function dp\_boxcar\_get\_gain 

_Current output gain._ 
```C++
JM_FORCEINLINE double dp_boxcar_get_gain (
    const dp_boxcar_state_t * s
) 
```




<hr>



### function dp\_boxcar\_get\_state 

_Serialize the full state into_ `blob` _._
```C++
void dp_boxcar_get_state (
    const dp_boxcar_state_t * s,
    void * blob
) 
```




<hr>



### function dp\_boxcar\_reset 

_Clear the window (zero the ring and the running sum); keep the configured length and gain._ 
```C++
void dp_boxcar_reset (
    dp_boxcar_state_t * s
) 
```



Returns the filter to its just-constructed state: the delay ring and the running window sum are zeroed while `len` and `gain` are preserved, so the next `len-1` outputs ramp in over a partial window exactly as they did on a fresh instance. Call it at a segment boundary so samples from one capture do not average into an unrelated next one.




**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
```C++
>>> import numpy as np
>>> from doppler.filter import MovingAverage
>>> ma = MovingAverage(2)                         # 2-sample window
>>> _ = ma.steps(np.ones(4, np.complex64))        # fill the window
>>> ma.reset()                                    # clear it
>>> round(ma.step(1 + 0j).real, 4)                # ramps in from empty
0.5
```
 




        

<hr>



### function dp\_boxcar\_set\_gain 

_Set the output gain; refresh the cached scale._ 
```C++
JM_FORCEINLINE void dp_boxcar_set_gain (
    dp_boxcar_state_t * s,
    double gain
) 
```





**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `gain` New output gain (folded into `scale = gain / len`). 




        

<hr>



### function dp\_boxcar\_set\_state 

_Restore state; DP\_OK, or DP\_ERR\_INVALID if the envelope rejects._ 
```C++
int dp_boxcar_set_state (
    dp_boxcar_state_t * s,
    const void * blob
) 
```




<hr>



### function dp\_boxcar\_state\_bytes 

_Serialized-state byte size._ 
```C++
size_t dp_boxcar_state_bytes (
    const dp_boxcar_state_t * s
) 
```




<hr>



### function dp\_boxcar\_step 

_Slide the window by one sample; return the gained moving average._ 
```C++
JM_FORCEINLINE  JM_HOT float _Complex dp_boxcar_step (
    dp_boxcar_state_t * s,
    float _Complex x
) 
```



O(1): add `x`, drop the sample leaving the window, return `acc · scale` (= `gain · acc / len`) — one multiply.




**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `x` One input sample. 



**Returns:**

The gained window mean after admitting `x`. 
```C++
>>> from doppler.filter import MovingAverage
>>> ma = MovingAverage(2)   # 2-sample sliding window, unit gain
>>> [round(ma.step(v).real, 4) for v in (1 + 0j, 3 + 0j, 3 + 0j)]
[0.5, 2.0, 3.0]
```
 





        

<hr>



### function dp\_boxcar\_steps 

_Filter a block: write the gained moving average of each sample._ 
```C++
void dp_boxcar_steps (
    dp_boxcar_state_t * s,
    const float _Complex * x,
    float _Complex * out,
    size_t n
) 
```



Applies [**dp\_boxcar\_step()**](boxcar__core_8h.md#function-dp_boxcar_step) to each input sample in turn, so the window sum and ring carry across the block exactly as they would sample by sample — a stream can be processed in frames of any size with no seam. Immediately after a reset the first `len-1` outputs average over a partial (still filling) window and ramp in.




**Parameters:**


* `s` Boxcar state. Must be non-NULL. 
* `x` Input samples. 
* `out` Output (gained window means); may alias `x`. 
* `n` Number of samples. 
```C++
>>> import numpy as np
>>> from doppler.filter import MovingAverage
>>> ma = MovingAverage(3)                          # 3-sample window
>>> x = np.ones(5, np.complex64)                   # unit step input
>>> [round(v, 4) for v in ma.steps(x).real.tolist()]
[0.3333, 0.6667, 1.0, 1.0, 1.0]
```
 




        

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
The documentation for this class was generated from the following file `native/inc/doppler/boxcar/boxcar_core.h`

