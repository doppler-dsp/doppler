

# File detector2d\_core.h



[**FileList**](files.md) **>** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md) **>** [**detector2d\_core.h**](detector2d__core_8h.md)

[Go to the source code of this file](detector2d__core_8h_source.md)

_2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "corr2d/corr2d_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_result2d\_t**](structdet__result2d__t.md) <br>_Detection event returned by_ [_**detector2d\_push()**_](detector2d__core_8h.md#function-detector2d_push) _._ |
| struct | [**detector2d\_state\_t**](structdetector2d__state__t.md) <br>_2-D signal detector state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**detector2d\_state\_t**](structdetector2d__state__t.md) \* | [**detector2d\_create**](#function-detector2d_create) (const float complex \* ref, size\_t ny, size\_t nx, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Create a 2-D signal detector._  |
|  void | [**detector2d\_destroy**](#function-detector2d_destroy) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state) <br>_Destroy and free._  |
|  size\_t | [**detector2d\_push**](#function-detector2d_push) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, [**det\_result2d\_t**](structdet__result2d__t.md) \* result, size\_t max\_results) <br>_Push an arbitrary-length CF32 chunk through the 2-D detector._  |
|  void | [**detector2d\_reset**](#function-detector2d_reset) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state) <br>_Reset correlator, ring buffer, and last-corr flag._  |
|  void | [**detector2d\_set\_ref**](#function-detector2d_set_ref) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference image and recompute conj(FFT2(ref))._  |
|  void | [**detector2d\_set\_threshold**](#function-detector2d_set_threshold) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, float threshold) <br>_Change threshold without rebuilding._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DET\_NOISE\_MODE\_T\_DEFINED**](detector2d__core_8h.md#define-det_noise_mode_t_defined)  <br> |

## Detailed Description


Two-dimensional extension of detector\_core. The input stream is chunked into ny×nx frames (flat row-major CF32). The test statistic and threshold semantics are identical to the 1-D variant; the only difference is that the peak index maps to a (row, col) pair instead of a single lag.


Detection events: [**det\_result2d\_t**](structdet__result2d__t.md) = { row, col, peak\_mag, noise\_est, test\_stat }


Lifecycle: 
```C++
float complex ref[NY * NX] = { ... };
detector2d_state_t *det = detector2d_create(ref, NY, NX, 1,
    0, NY*NX-1, DET_NOISE_MEAN, 0.0f, 1);
det_result2d_t results[64];
while (recv(chunk, CHUNK_SZ)) {
    size_t n = detector2d_push(det, chunk, CHUNK_SZ, results, 64);
    for (size_t i = 0; i < n; i++)
        printf("row=%zu col=%zu stat=%.2f\n",
               results[i].row, results[i].col, results[i].test_stat);
}
detector2d_destroy(det);
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




### function detector2d\_create 

_Create a 2-D signal detector._ 
```C++
detector2d_state_t * detector2d_create (
    const float complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    size_t noise_lo,
    size_t noise_hi,
    det_noise_mode_t noise_mode,
    float threshold,
    int nthreads
) 
```





**Parameters:**


* `ref` Reference image, flat row-major CF32, length ny\*nx. 
* `ny` Number of rows. 
* `nx` Number of columns. 
* `dwell` Int-dump depth. Must be &gt;= 1. 
* `noise_lo` Lower flat-index noise bin (inclusive). 
* `noise_hi` Upper flat-index noise bin (inclusive, &lt; ny\*nx). 
* `noise_mode` Noise aggregation mode. 
* `threshold` 0.0 = always emit a detection. 
* `nthreads` Passed to [**corr2d\_create()**](corr2d__core_8h.md#function-corr2d_create); currently ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function detector2d\_destroy 

_Destroy and free._ 
```C++
void detector2d_destroy (
    detector2d_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function detector2d\_push 

_Push an arbitrary-length CF32 chunk through the 2-D detector._ 
```C++
size_t detector2d_push (
    detector2d_state_t * state,
    const float complex * in,
    size_t n_in,
    det_result2d_t * result,
    size_t max_results
) 
```



Behaviour is identical to [**detector\_push()**](detector__core_8h.md#function-detector_push) except frames have length ny\*nx and results carry (row, col) instead of a single lag.




**Parameters:**


* `state` Must be non-NULL. 
* `in` Input CF32 array of length `n_in`. 
* `n_in` Number of complex samples to push. 
* `result` Output array; caller allocates at least `max_results`. 
* `max_results` Maximum detections to store. 



**Returns:**

Number of detections stored in `result`[]. 





        

<hr>



### function detector2d\_reset 

_Reset correlator, ring buffer, and last-corr flag._ 
```C++
void detector2d_reset (
    detector2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function detector2d\_set\_ref 

_Replace the reference image and recompute conj(FFT2(ref))._ 
```C++
void detector2d_set_ref (
    detector2d_state_t * state,
    const float complex * ref
) 
```



Also resets. The new reference must have the same ny\*nx total size.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 




        

<hr>



### function detector2d\_set\_threshold 

_Change threshold without rebuilding._ 
```C++
void detector2d_set_threshold (
    detector2d_state_t * state,
    float threshold
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 




        

<hr>
## Macro Definition Documentation





### define DET\_NOISE\_MODE\_T\_DEFINED 

```C++
#define DET_NOISE_MODE_T_DEFINED 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector2d/detector2d_core.h`

