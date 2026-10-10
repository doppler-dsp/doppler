

# File detector\_core.h



[**FileList**](files.md) **>** [**detector**](dir_4cdf6fdfdd426ef1a31e056182554d6b.md) **>** [**detector\_core.h**](detector__core_8h.md)

[Go to the source code of this file](detector__core_8h_source.md)

_1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._ [More...](#detailed-description)

* `#include "doppler/corr/corr_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_result\_t**](structdet__result__t.md) <br>_Detection event returned by_ [_**dp\_detector\_push()**_](detector__core_8h.md#function-dp_detector_push) _._ |
| struct | [**dp\_detector\_state\_t**](structdp__detector__state__t.md) <br>_1-D signal detector state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_detector\_consumed**](#function-dp_detector_consumed) (const [**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state) <br>_Input samples the last_ [_**dp\_detector\_push()**_](detector__core_8h.md#function-dp_detector_push) _took._ |
|  [**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* | [**dp\_detector\_create**](#function-dp_detector_create) (const float \_Complex \* ref, size\_t ref\_len, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 1-D streaming signal detector backed by an FFT correlator. Combines a_ [_**dp\_corr\_state\_t**_](structdp__corr__state__t.md) _with a double-mapped ring buffer so that arbitrary chunk sizes can be pushed. After every int-dump the peak-to-noise test statistic is compared against_`threshold` _; a_[_**det\_result\_t**_](structdet__result__t.md) _is emitted when it passes. Setting_`threshold` _to 0.0 unconditionally fires on every dump. The ring capacity is next\_pow\_two(max(n, 512)) complex samples._ |
|  void | [**dp\_detector\_destroy**](#function-dp_detector_destroy) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state) <br>_Destroy and free a detector instance._  |
|  void | [**dp\_detector\_get\_state**](#function-dp_detector_get_state) (const [**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**dp\_detector\_push**](#function-dp_detector_push) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, [**det\_result\_t**](structdet__result__t.md) \* result, size\_t max\_results) <br>_Stream an arbitrary-length CF32 chunk through the detector pipeline. Takes the input in order, runs each complete n-sample frame through the correlator, and on every int-dump computes the test statistic peak\_mag / noise\_est. Detections that pass the threshold are appended to the Python return list as (lag, peak\_mag, noise\_est, test\_stat) tuples. In Python the result is always a list, even when empty._  |
|  void | [**dp\_detector\_reset**](#function-dp_detector_reset) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state) <br>_Reset the correlator, the carry, and last-corr flag. Discards any partial frame carried between pushes and zeroes the coherent accumulator. Equivalent to starting fresh from the same reference without rebuilding any internal object._  |
|  void | [**dp\_detector\_set\_ref**](#function-dp_detector_set_ref) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state, const float \_Complex \* ref) <br>_Replace the reference signal and recompute conj(FFT(ref))._  |
|  int | [**dp\_detector\_set\_state**](#function-dp_detector_set_state) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state, const void \* blob) <br> |
|  void | [**dp\_detector\_set\_threshold**](#function-dp_detector_set_threshold) ([**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state, float threshold) <br>_Change the threshold without rebuilding the object._  |
|  size\_t | [**dp\_detector\_state\_bytes**](#function-dp_detector_state_bytes) (const [**dp\_detector\_state\_t**](structdp__detector__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DETECTOR\_STATE\_MAGIC**](detector__core_8h.md#define-detector_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','E','T','1')`<br> |
| define  | [**DETECTOR\_STATE\_VERSION**](detector__core_8h.md#define-detector_state_version)  `2u`<br> |
| define  | [**DET\_NOISE\_MODE\_T\_DEFINED**](detector__core_8h.md#define-det_noise_mode_t_defined)  <br>_Selects how noise power is estimated from the correlation magnitude vector over bins &#91;noise\_lo, noise\_hi&#93;._  |

## Detailed Description


Wraps a [**dp\_corr\_state\_t**](structdp__corr__state__t.md) (FFT correlator + coherent int-dump) behind the ring's framed face (DECLARE\_DP\_BUFFER\_FRAMES) so that arbitrary-length sample streams can be fed in any chunk size: the detections are a function of the input stream, not of how it was split into calls. After every int-dump a test statistic is computed:


test\_stat = peak\_mag / noise\_est


where peak\_mag = max\|R&#91;τ&#93;\| and noise\_est is an aggregate (mean, median, min, or max) of \|R&#91;τ&#93;\| over a user-supplied bin range &#91;noise\_lo, noise\_hi&#93;. A detection event is emitted when test\_stat &gt; threshold (or whenever threshold == 0.0, which means "always fire").


The detector operates as a single-threaded object; do not call [**dp\_detector\_push()**](detector__core_8h.md#function-dp_detector_push) concurrently from multiple threads.


Lifecycle: 
```C++
float _Complex ref[N] = { ... };
dp_detector_state_t *det = dp_detector_create(ref, N, 1,
    1, N-1, DET_NOISE_MEAN, 0.0f, 1);
det_result_t results[64];
// stream loop: a full results[] stops a push, and the input it did not
// take is offered again -- dp_detector_consumed(det) says where it stopped
while (recv(chunk, CHUNK_SZ)) {
    for (size_t off = 0; off < CHUNK_SZ; off += dp_detector_consumed(det)) {
        size_t n = dp_detector_push(det, chunk + off, CHUNK_SZ - off,
                                    results, 64);
        for (size_t i = 0; i < n; i++)
            printf("lag=%zu stat=%.2f\n", results[i].lag,
                   results[i].test_stat);
    }
}
dp_detector_destroy(det);
```
 


    
## Public Types Documentation




### enum det\_noise\_mode\_t 

```C++
enum det_noise_mode_t {
    DET_NOISE_MEAN = 0,
    DET_NOISE_MEDIAN = 1,
    DET_NOISE_MIN = 2,
    DET_NOISE_MAX = 3
};
```




<hr>
## Public Functions Documentation




### function dp\_detector\_consumed 

_Input samples the last_ [_**dp\_detector\_push()**_](detector__core_8h.md#function-dp_detector_push) _took._
```C++
size_t dp_detector_consumed (
    const dp_detector_state_t * state
) 
```



Equal to its `n_in` unless `result` filled up; then the caller resumes at in + consumed. 0 after create, reset and set\_state.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function dp\_detector\_create 

_Allocate a 1-D streaming signal detector backed by an FFT correlator. Combines a_ [_**dp\_corr\_state\_t**_](structdp__corr__state__t.md) _with a double-mapped ring buffer so that arbitrary chunk sizes can be pushed. After every int-dump the peak-to-noise test statistic is compared against_`threshold` _; a_[_**det\_result\_t**_](structdet__result__t.md) _is emitted when it passes. Setting_`threshold` _to 0.0 unconditionally fires on every dump. The ring capacity is next\_pow\_two(max(n, 512)) complex samples._
```C++
dp_detector_state_t * dp_detector_create (
    const float _Complex * ref,
    size_t ref_len,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```





**Parameters:**


* `ref` Reference signal, CF32 ndarray of length ref\_len. 
* `ref_len` Reference / FFT length in complex samples. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower noise bin index (inclusive, 0-based). 
* `noise_hi` Upper noise bin index (inclusive, &lt; n). A value at or beyond the window clamps to n - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.spectral import CorrDetector
>>> import numpy as np
>>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
>>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
...                noise_mode="mean", threshold=0.0)
>>> det.n, det.dwell, det.ring_cap
(8, 1, 512)
```
 





        

<hr>



### function dp\_detector\_destroy 

_Destroy and free a detector instance._ 
```C++
void dp_detector_destroy (
    dp_detector_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_detector\_get\_state 

```C++
void dp_detector_get_state (
    const dp_detector_state_t * state,
    void * blob
) 
```




<hr>



### function dp\_detector\_push 

_Stream an arbitrary-length CF32 chunk through the detector pipeline. Takes the input in order, runs each complete n-sample frame through the correlator, and on every int-dump computes the test statistic peak\_mag / noise\_est. Detections that pass the threshold are appended to the Python return list as (lag, peak\_mag, noise\_est, test\_stat) tuples. In Python the result is always a list, even when empty._ 
```C++
size_t dp_detector_push (
    dp_detector_state_t * state,
    const float _Complex * in,
    size_t n_in,
    det_result_t * result,
    size_t max_results
) 
```



Python's push() has room for 1024 detections a call. A push that would make more loses the detections past the 1024th, and the input that would have made them: keep a chunk under 1024 frames. Before v0.66 a push past its room (64 then) kept the whole frames it had already buffered and reported them on the next call; #1992 tracks sizing the list to the call.




**Parameters:**


* `state` Allocated detector (non-NULL). 
* `in` CF32 input chunk of arbitrary length. 
* `n_in` Number of input samples in `in`. 
* `result` Caller-supplied array of at least `max_results` [**det\_result\_t**](structdet__result__t.md) structs; filled on return. 
* `max_results` Capacity of `result` (maximum detections to emit). A full `result` never loses input: a frame yields at most one detection, so a sample is taken unless it would complete a frame when `result` has no room left. The push stops there, [**dp\_detector\_consumed()**](detector__core_8h.md#function-dp_detector_consumed) says how many samples it took, and the caller offers the rest again. Taken input that completes no frame is the carry, held inside (fewer than n samples), so it is taken whole even at 0, and at &gt;= 1 a push of any input takes at least one sample. 



**Returns:**

Number of [**det\_result\_t**](structdet__result__t.md) entries written to `result`. 
```C++
>>> from doppler.spectral import CorrDetector
>>> import numpy as np
>>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
>>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
...                noise_mode="mean", threshold=0.0)
>>> results = det.push(np.ones(8, dtype=np.complex64))
>>> len(results)
1
>>> lag, peak, noise, stat = results[0]
>>> lag, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 1.0, 1.0, 1.0)
```
 





        

<hr>



### function dp\_detector\_reset 

_Reset the correlator, the carry, and last-corr flag. Discards any partial frame carried between pushes and zeroes the coherent accumulator. Equivalent to starting fresh from the same reference without rebuilding any internal object._ 
```C++
void dp_detector_reset (
    dp_detector_state_t * state
) 
```




```C++
>>> from doppler.spectral import CorrDetector
>>> import numpy as np
>>> ref = np.zeros(8, dtype=np.complex64); ref[0] = 1.0
>>> det = CorrDetector(ref=ref, dwell=1, noise_lo=1, noise_hi=7,
...                noise_mode="mean", threshold=0.0)
>>> _ = det.push(np.ones(8, dtype=np.complex64))
>>> det.reset()
>>> det.count
0
```
 


        

<hr>



### function dp\_detector\_set\_ref 

_Replace the reference signal and recompute conj(FFT(ref))._ 
```C++
void dp_detector_set_ref (
    dp_detector_state_t * state,
    const float _Complex * ref
) 
```



Also resets (see [**dp\_detector\_reset()**](detector__core_8h.md#function-dp_detector_reset)). The new reference must have the same length `n` that was passed to [**dp\_detector\_create()**](detector__core_8h.md#function-dp_detector_create).




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, CF32, length state-&gt;n. 




        

<hr>



### function dp\_detector\_set\_state 

```C++
int dp_detector_set_state (
    dp_detector_state_t * state,
    const void * blob
) 
```




<hr>



### function dp\_detector\_set\_threshold 

_Change the threshold without rebuilding the object._ 
```C++
void dp_detector_set_threshold (
    dp_detector_state_t * state,
    float threshold
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 




        

<hr>



### function dp\_detector\_state\_bytes 

```C++
size_t dp_detector_state_bytes (
    const dp_detector_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DETECTOR\_STATE\_MAGIC 

```C++
#define DETECTOR_STATE_MAGIC `DP_FOURCC ('D','E','T','1')`
```




<hr>



### define DETECTOR\_STATE\_VERSION 

```C++
#define DETECTOR_STATE_VERSION `2u`
```




<hr>



### define DET\_NOISE\_MODE\_T\_DEFINED 

_Selects how noise power is estimated from the correlation magnitude vector over bins &#91;noise\_lo, noise\_hi&#93;._ 
```C++
#define DET_NOISE_MODE_T_DEFINED 
```



The test statistic is peak\_mag / noise\_est. A zero noise\_est (e.g., when all bins in the range are zero) yields test\_stat = 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/detector/detector_core.h`

