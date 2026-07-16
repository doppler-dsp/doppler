

# File detector2d\_core.h



[**FileList**](files.md) **>** [**detector2d**](dir_bd7354e9665bd912180ec22b3c69b55c.md) **>** [**detector2d\_core.h**](detector2d__core_8h.md)

[Go to the source code of this file](detector2d__core_8h_source.md)

_2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._ [More...](#detailed-description)

* `#include "buffer/buffer.h"`
* `#include "corr2d/corr2d_core.h"`
* `#include "dp_state.h"`















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
|  [**detector2d\_state\_t**](structdetector2d__state__t.md) \* | [**detector2d\_create**](#function-detector2d_create) (const float complex \* ref, size\_t ny, size\_t nx, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of_ [_**detector\_create()**_](detector__core_8h.md#function-detector_create) _. Input frames are flat row-major CF32 arrays of length ny\*nx streamed through a ring buffer. On every int-dump the peak flat index is decomposed into (row, col) and a_[_**det\_result2d\_t**_](structdet__result2d__t.md) _is emitted when test\_stat &gt; threshold. The Python wrapper accepts a (ny, nx) CF32 ndarray for both_`ref` _and the push input._ |
|  void | [**detector2d\_destroy**](#function-detector2d_destroy) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state) <br>_Destroy and free._  |
|  void | [**detector2d\_get\_state**](#function-detector2d_get_state) (const [**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, void \* blob) <br> |
|  size\_t | [**detector2d\_push**](#function-detector2d_push) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, [**det\_result2d\_t**](structdet__result2d__t.md) \* result, size\_t max\_results) <br>_Stream an arbitrary-length CF32 chunk through the 2-D detector. Identical to_ [_**detector\_push()**_](detector__core_8h.md#function-detector_push) _except frames are ny\*nx complex samples and each detection event carries (row, col) for the peak location instead of a single lag index. In Python the result is always a list of (row, col, peak\_mag, noise\_est, test\_stat) tuples._ |
|  void | [**detector2d\_reset**](#function-detector2d_reset) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state) <br>_Reset the 2-D correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator. The reference spectrum and FFT plans are preserved._  |
|  int | [**detector2d\_set\_ref**](#function-detector2d_set_ref) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference image and recompute its spectrum._  |
|  int | [**detector2d\_set\_state**](#function-detector2d_set_state) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, const void \* blob) <br> |
|  void | [**detector2d\_set\_threshold**](#function-detector2d_set_threshold) ([**detector2d\_state\_t**](structdetector2d__state__t.md) \* state, float threshold) <br>_Change threshold without rebuilding._  |
|  size\_t | [**detector2d\_state\_bytes**](#function-detector2d_state_bytes) (const [**detector2d\_state\_t**](structdetector2d__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DETECTOR2D\_STATE\_MAGIC**](detector2d__core_8h.md#define-detector2d_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','E','T','2')`<br> |
| define  | [**DETECTOR2D\_STATE\_VERSION**](detector2d__core_8h.md#define-detector2d_state_version)  `1u`<br> |
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

_Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of_ [_**detector\_create()**_](detector__core_8h.md#function-detector_create) _. Input frames are flat row-major CF32 arrays of length ny\*nx streamed through a ring buffer. On every int-dump the peak flat index is decomposed into (row, col) and a_[_**det\_result2d\_t**_](structdet__result2d__t.md) _is emitted when test\_stat &gt; threshold. The Python wrapper accepts a (ny, nx) CF32 ndarray for both_`ref` _and the push input._
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


* `ref` 2-D reference image, (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Int-dump depth; must be &gt;= 1. 
* `noise_lo` Lower flat-index noise bin (inclusive, 0-based). 
* `noise_hi` Upper flat-index noise bin (inclusive, &lt; ny\*nx). 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.spectral import CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> det.ny, det.nx, det.n, det.dwell
(4, 4, 16, 1)
```
 





        

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



### function detector2d\_get\_state 

```C++
void detector2d_get_state (
    const detector2d_state_t * state,
    void * blob
) 
```




<hr>



### function detector2d\_push 

_Stream an arbitrary-length CF32 chunk through the 2-D detector. Identical to_ [_**detector\_push()**_](detector__core_8h.md#function-detector_push) _except frames are ny\*nx complex samples and each detection event carries (row, col) for the peak location instead of a single lag index. In Python the result is always a list of (row, col, peak\_mag, noise\_est, test\_stat) tuples._
```C++
size_t detector2d_push (
    detector2d_state_t * state,
    const float complex * in,
    size_t n_in,
    det_result2d_t * result,
    size_t max_results
) 
```





**Parameters:**


* `state` Allocated 2-D detector (non-NULL). 
* `in` CF32 input chunk of arbitrary length. 
* `n_in` Number of input samples in `in`. 
* `result` Caller-supplied array of at least `max_results` [**det\_result2d\_t**](structdet__result2d__t.md) structs; filled on return. 
* `max_results` Capacity of `result` (maximum detections to emit). 



**Returns:**

Number of [**det\_result2d\_t**](structdet__result2d__t.md) entries written to `result`. 
```C++
>>> from doppler.spectral import CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> results = det.push(np.ones((4, 4), dtype=np.complex64))
>>> len(results)
1
>>> row, col, peak, noise, stat = results[0]
>>> row, col, round(peak, 4), round(noise, 4), round(stat, 4)
(0, 0, 1.0, 1.0, 1.0)
```
 





        

<hr>



### function detector2d\_reset 

_Reset the 2-D correlator, ring buffer, and last-corr flag. Discards any partial frame buffered in the ring and zeroes the coherent accumulator. The reference spectrum and FFT plans are preserved._ 
```C++
void detector2d_reset (
    detector2d_state_t * state
) 
```




```C++
>>> from doppler.spectral import CorrDetector2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> det = CorrDetector2D(ref=ref, dwell=1, noise_lo=1, noise_hi=15,
...                  noise_mode="mean", threshold=0.0)
>>> _ = det.push(np.ones((4, 4), dtype=np.complex64))
>>> det.reset()
>>> det.count
0
```
 


        

<hr>



### function detector2d\_set\_ref 

_Replace the reference image and recompute its spectrum._ 
```C++
int detector2d_set_ref (
    detector2d_state_t * state,
    const float complex * ref
) 
```



Always resets (ring, corr2d accumulator, last-dump bookkeeping), even if the new reference is subsequently rejected. The new reference must have the same ny\*nx total size; see [**corr2d\_set\_ref()**](corr2d__core_8h.md#function-corr2d_set_ref) for the single-row-fast- path rejection rule this forwards.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 



**Returns:**

0 on success, -1 if rejected by [**corr2d\_set\_ref()**](corr2d__core_8h.md#function-corr2d_set_ref). 





        

<hr>



### function detector2d\_set\_state 

```C++
int detector2d_set_state (
    detector2d_state_t * state,
    const void * blob
) 
```




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



### function detector2d\_state\_bytes 

```C++
size_t detector2d_state_bytes (
    const detector2d_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define DETECTOR2D\_STATE\_MAGIC 

```C++
#define DETECTOR2D_STATE_MAGIC `DP_FOURCC ('D','E','T','2')`
```




<hr>



### define DETECTOR2D\_STATE\_VERSION 

```C++
#define DETECTOR2D_STATE_VERSION `1u`
```




<hr>



### define DET\_NOISE\_MODE\_T\_DEFINED 

```C++
#define DET_NOISE_MODE_T_DEFINED 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/detector2d/detector2d_core.h`

