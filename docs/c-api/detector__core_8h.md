

# File detector\_core.h



[**FileList**](files.md) **>** [**detector**](dir_f93f7a52d403868792375ffc90a1c1d7.md) **>** [**detector\_core.h**](detector__core_8h.md)

[Go to the source code of this file](detector__core_8h_source.md)

_1-D streaming signal detector with FFT-based correlation, integrate-and-dump, and configurable noise-referenced threshold._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "corr/corr_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_result\_t**](structdet__result__t.md) <br>_Detection event returned by_ [_**detector\_push()**_](detector__core_8h.md#function-detector_push) _._ |
| struct | [**detector\_state\_t**](structdetector__state__t.md) <br>_1-D signal detector state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**detector\_state\_t**](structdetector__state__t.md) \* | [**detector\_create**](#function-detector_create) (const float complex \* ref, size\_t n, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Create a 1-D signal detector._  |
|  void | [**detector\_destroy**](#function-detector_destroy) ([**detector\_state\_t**](structdetector__state__t.md) \* state) <br>_Destroy and free a detector instance._  |
|  size\_t | [**detector\_push**](#function-detector_push) ([**detector\_state\_t**](structdetector__state__t.md) \* state, const float complex \* in, size\_t n\_in, [**det\_result\_t**](structdet__result__t.md) \* result, size\_t max\_results) <br>_Push an arbitrary-length CF32 chunk through the detector._  |
|  void | [**detector\_reset**](#function-detector_reset) ([**detector\_state\_t**](structdetector__state__t.md) \* state) <br>_Reset the correlator, ring buffer, and last-corr flag._  |
|  void | [**detector\_set\_ref**](#function-detector_set_ref) ([**detector\_state\_t**](structdetector__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference signal and recompute conj(FFT(ref))._  |
|  void | [**detector\_set\_threshold**](#function-detector_set_threshold) ([**detector\_state\_t**](structdetector__state__t.md) \* state, float threshold) <br>_Change the threshold without rebuilding the object._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DET\_NOISE\_MODE\_T\_DEFINED**](detector__core_8h.md#define-det_noise_mode_t_defined)  <br>_Selects how noise power is estimated from the correlation magnitude vector over bins &#91;noise\_lo, noise\_hi&#93;._  |

## Detailed Description


Wraps a [**corr\_state\_t**](structcorr__state__t.md) (FFT correlator + coherent int-dump) behind a double-mapped ring buffer so that arbitrary-length sample streams can be fed in any chunk size. After every int-dump a test statistic is computed:


test\_stat = peak\_mag / noise\_est


where peak\_mag = max\|R&#91;τ&#93;\| and noise\_est is an aggregate (mean, median, min, or max) of \|R&#91;τ&#93;\| over a user-supplied bin range &#91;noise\_lo, noise\_hi&#93;. A detection event is emitted when test\_stat &gt; threshold (or whenever threshold == 0.0, which means "always fire").


The detector operates as a single-threaded object; do not call [**detector\_push()**](detector__core_8h.md#function-detector_push) concurrently from multiple threads.


Lifecycle: 
```C++
float complex ref[N] = { ... };
detector_state_t *det = detector_create(ref, N, 1,
    1, N-1, DET_NOISE_MEAN, 0.0f, 1);
det_result_t results[64];
// stream loop
while (recv(chunk, CHUNK_SZ)) {
    size_t n = detector_push(det, chunk, CHUNK_SZ, results, 64);
    for (size_t i = 0; i < n; i++)
        printf("lag=%zu stat=%.2f\n", results[i].lag, results[i].test_stat);
}
detector_destroy(det);
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




### function detector\_create 

_Create a 1-D signal detector._ 
```C++
detector_state_t * detector_create (
    const float complex * ref,
    size_t n,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```



Allocates a [**corr\_state\_t**](structcorr__state__t.md), a dp\_f32\_t ring buffer of capacity next\_pow2(max(n, 512)), and all scratch buffers. The ring buffer satisfies the dp\_f32\_create() page-alignment constraint automatically.




**Parameters:**


* `ref` Reference signal, CF32, length `n`. May be freed after this call returns. 
* `n` Frame / FFT length in complex samples. Must be &gt;= 1. 
* `dwell` Int-dump depth. Must be &gt;= 1. 
* `noise_lo` Lower noise bin (inclusive, 0 &lt;= noise\_lo &lt;= noise\_hi). 
* `noise_hi` Upper noise bin (inclusive, noise\_hi &lt; n). 
* `noise_mode` Aggregation mode for noise estimation. 
* `threshold` Test-stat threshold. 0.0 = always emit a detection. 
* `nthreads` Passed through to [**corr\_create()**](corr__core_8h.md#function-corr_create); currently ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function detector\_destroy 

_Destroy and free a detector instance._ 
```C++
void detector_destroy (
    detector_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function detector\_push 

_Push an arbitrary-length CF32 chunk through the detector._ 
```C++
size_t detector_push (
    detector_state_t * state,
    const float complex * in,
    size_t n_in,
    det_result_t * result,
    size_t max_results
) 
```



Writes `n_in` complex samples into the ring buffer in the minimum number of chunks that fit, then drains all complete n-sample frames through the correlator. On every int-dump a test statistic is computed; if it passes the threshold, a [**det\_result\_t**](structdet__result__t.md) is appended to `result`[]. The function returns as soon as `n_in` samples have been consumed or `max_results` detections have been stored, whichever comes first.


The `result` array must be pre-allocated by the caller. A stack array of 64 elements is sufficient for any realistic push size: 
```C++
det_result_t buf[64];
size_t n = detector_push(det, chunk, len, buf, 64);
```





**Parameters:**


* `state` Must be non-NULL. 
* `in` Input CF32 array of length `n_in`. 
* `n_in` Number of complex samples to push. 
* `result` Output array; caller allocates at least `max_results`. 
* `max_results` Maximum detections to store; prevents unbounded output. 



**Returns:**

Number of detections stored in `result`[]. 





        

<hr>



### function detector\_reset 

_Reset the correlator, ring buffer, and last-corr flag._ 
```C++
void detector_reset (
    detector_state_t * state
) 
```



Discards any partial frame buffered in the ring. Equivalent to starting fresh from the same reference.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function detector\_set\_ref 

_Replace the reference signal and recompute conj(FFT(ref))._ 
```C++
void detector_set_ref (
    detector_state_t * state,
    const float complex * ref
) 
```



Also resets (see [**detector\_reset()**](detector__core_8h.md#function-detector_reset)). The new reference must have the same length `n` that was passed to [**detector\_create()**](detector__core_8h.md#function-detector_create).




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, CF32, length state-&gt;n. 




        

<hr>



### function detector\_set\_threshold 

_Change the threshold without rebuilding the object._ 
```C++
void detector_set_threshold (
    detector_state_t * state,
    float threshold
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 




        

<hr>
## Macro Definition Documentation





### define DET\_NOISE\_MODE\_T\_DEFINED 

_Selects how noise power is estimated from the correlation magnitude vector over bins &#91;noise\_lo, noise\_hi&#93;._ 
```C++
#define DET_NOISE_MODE_T_DEFINED 
```



The test statistic is peak\_mag / noise\_est. A zero noise\_est (e.g., when all bins in the range are zero) yields test\_stat = 0. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector/detector_core.h`

