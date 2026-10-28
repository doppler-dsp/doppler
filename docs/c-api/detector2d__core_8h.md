

# File detector2d\_core.h



[**FileList**](files.md) **>** [**detector2d**](dir_8496bb9d19545edf0ab852f69ada11c8.md) **>** [**detector2d\_core.h**](detector2d__core_8h.md)

[Go to the source code of this file](detector2d__core_8h_source.md)

_2-D streaming signal detector with FFT2D-based correlation, integrate-and-dump, and configurable noise-referenced threshold._ [More...](#detailed-description)

* `#include "doppler/corr2d/corr2d_core.h"`
* `#include "doppler/dp_state.h"`
* `#include "doppler/f32_buffer/f32_buffer_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**det\_peak\_t**](structdet__peak__t.md) <br>_One listed peak of a surface: its cell and value._  |
| struct | [**det\_result2d\_t**](structdet__result2d__t.md) <br>_Detection event returned by_ [_**dp\_detector2d\_push()**_](detector2d__core_8h.md#function-dp_detector2d_push) _._ |
| struct | [**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) <br>_2-D signal detector state._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**det\_noise\_mode\_t**](#enum-det_noise_mode_t)  <br>_How the noise floor is aggregated from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive (as in_ [_**detector\_core.h**_](detector__core_8h.md) _)._ |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_detector2d\_consumed**](#function-dp_detector2d_consumed) (const [**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state) <br>_Input samples the last_ [_**dp\_detector2d\_push()**_](detector2d__core_8h.md#function-dp_detector2d_push) _took._ |
|  [**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* | [**dp\_detector2d\_create**](#function-dp_detector2d_create) (const float \_Complex \* ref, size\_t ny, size\_t nx, size\_t dwell, size\_t noise\_lo, size\_t noise\_hi, [**det\_noise\_mode\_t**](detector__core_8h.md#enum-det_noise_mode_t) noise\_mode, float threshold, int nthreads) <br>_Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of_ [_**dp\_detector\_create()**_](detector__core_8h.md#function-dp_detector_create) _. Input frames are flat row-major CF32 arrays of length ny\*nx, cut from the stream by the ring's framer. On every int-dump the peak flat index is decomposed into (row, col) and a_[_**det\_result2d\_t**_](structdet__result2d__t.md) _is emitted when test\_stat &gt; threshold. The Python wrapper accepts a (ny, nx) CF32 ndarray for both_`ref` _and the push input._ |
|  void | [**dp\_detector2d\_destroy**](#function-dp_detector2d_destroy) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state) <br>_Destroy and free._  |
|  void | [**dp\_detector2d\_get\_state**](#function-dp_detector2d_get_state) (const [**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state, void \* blob) <br>_Serialize the correlator, carry and last-dump result into_ `blob` _._ |
|  size\_t | [**dp\_detector2d\_push**](#function-dp_detector2d_push) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state, const float \_Complex \* in, size\_t n\_in, [**det\_result2d\_t**](structdet__result2d__t.md) \* result, size\_t max\_results) <br>_Stream an arbitrary-length CF32 chunk through the 2-D detector. The same pipeline as_ [_**dp\_detector\_push()**_](detector__core_8h.md#function-dp_detector_push) _, except that frames are ny\*nx complex samples and each detection event carries (row, col) for the peak location instead of a single lag index. In Python the result is always a list of (row, col, peak\_mag, noise\_est, test\_stat) tuples._ |
|  void | [**dp\_detector2d\_reset**](#function-dp_detector2d_reset) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state) <br>_Reset the 2-D correlator, the carry, and last-corr flag. Discards any partial frame carried between pushes and zeroes the coherent accumulator. The reference spectrum and FFT plans are preserved. The last-dump fields (peak\_row, peak\_col, peak\_mag, noise\_est, test\_stat) read 0 until the next dump, so the state blob after a reset matches a fresh object's (doppler#2642)._  |
|  int | [**dp\_detector2d\_set\_ref**](#function-dp_detector2d_set_ref) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state, const float \_Complex \* ref) <br>_Replace the reference image and recompute its spectrum._  |
|  int | [**dp\_detector2d\_set\_state**](#function-dp_detector2d_set_state) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state, const void \* blob) <br>_Restore a detector from a blob made by_ [_**dp\_detector2d\_get\_state()**_](detector2d__core_8h.md#function-dp_detector2d_get_state) _._ |
|  void | [**dp\_detector2d\_set\_threshold**](#function-dp_detector2d_set_threshold) ([**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state, float threshold) <br>_Change threshold without rebuilding._  |
|  size\_t | [**dp\_detector2d\_state\_bytes**](#function-dp_detector2d_state_bytes) (const [**dp\_detector2d\_state\_t**](structdp__detector2d__state__t.md) \* state) <br>_Size in bytes of this detector's state blob._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DETECTOR2D\_STATE\_MAGIC**](detector2d__core_8h.md#define-detector2d_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('D','E','T','2')`<br>_State-blob magic ('DET2'); set\_state() rejects any other._  |
| define  | [**DETECTOR2D\_STATE\_VERSION**](detector2d__core_8h.md#define-detector2d_state_version)  `2u`<br>_State-blob layout version (2; version-1 blobs are refused)._  |
| define  | [**DET\_NOISE\_MODE\_T\_DEFINED**](detector2d__core_8h.md#define-det_noise_mode_t_defined)  <br>_Include guard: det\_noise\_mode\_t is defined once across the detector headers that share it._  |
| define  | [**DET\_PEAK\_T\_DEFINED**](detector2d__core_8h.md#define-det_peak_t_defined)  <br>_Include guard:_ [_**det\_peak\_t**_](structdet__peak__t.md) _is defined once across headers._ |

## Detailed Description


Two-dimensional extension of detector\_core. The input stream is chunked into ny×nx frames (flat row-major CF32) by the ring's framed face (DECLARE\_DP\_BUFFER\_FRAMES), so the detections are a function of the input stream, not of how it was split into calls. The test statistic and threshold semantics are identical to the 1-D variant; the only difference is that the peak index maps to a (row, col) pair instead of a single lag.


Detection events: [**det\_result2d\_t**](structdet__result2d__t.md) = { row, col, peak\_mag, noise\_est, test\_stat }


Lifecycle: 
```C++
float _Complex ref[NY * NX] = { ... };
dp_detector2d_state_t *det = dp_detector2d_create(ref, NY, NX, 1,
    0, NY*NX-1, DET_NOISE_MEAN, 0.0f, 1);
det_result2d_t results[64];
// a full results[] stops a push, and the input it did not take is
// offered again -- dp_detector2d_consumed(det) says where it stopped
while (recv(chunk, CHUNK_SZ)) {
    for (size_t off = 0; off < CHUNK_SZ;
         off += dp_detector2d_consumed(det)) {
        size_t n = dp_detector2d_push(det, chunk + off, CHUNK_SZ - off,
                                      results, 64);
        for (size_t i = 0; i < n; i++)
            printf("row=%zu col=%zu stat=%.2f\n",
                   results[i].row, results[i].col, results[i].test_stat);
    }
}
dp_detector2d_destroy(det);
```
 


    
## Public Types Documentation




### enum det\_noise\_mode\_t 

_How the noise floor is aggregated from the correlation magnitudes over the flat bins noise\_lo to noise\_hi, inclusive (as in_ [_**detector\_core.h**_](detector__core_8h.md) _)._
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




### function dp\_detector2d\_consumed 

_Input samples the last_ [_**dp\_detector2d\_push()**_](detector2d__core_8h.md#function-dp_detector2d_push) _took._
```C++
size_t dp_detector2d_consumed (
    const dp_detector2d_state_t * state
) 
```



Equal to its `n_in` unless `result` filled up; then the caller resumes at in + consumed. 0 after create, reset, set\_ref and set\_state.




**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Samples of the last push's input consumed (including any kept as the carry). 





        

<hr>



### function dp\_detector2d\_create 

_Allocate a 2-D streaming signal detector backed by a 2-D correlator. Two-dimensional extension of_ [_**dp\_detector\_create()**_](detector__core_8h.md#function-dp_detector_create) _. Input frames are flat row-major CF32 arrays of length ny\*nx, cut from the stream by the ring's framer. On every int-dump the peak flat index is decomposed into (row, col) and a_[_**det\_result2d\_t**_](structdet__result2d__t.md) _is emitted when test\_stat &gt; threshold. The Python wrapper accepts a (ny, nx) CF32 ndarray for both_`ref` _and the push input._
```C++
dp_detector2d_state_t * dp_detector2d_create (
    const float _Complex * ref,
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
* `noise_hi` Upper flat-index noise bin (inclusive, &lt; ny\*nx). A value at or beyond the window clamps to ny\*nx - 1, so the default sentinel selects the full window. 
* `noise_mode` Noise aggregation: "mean", "median", "min", or "max". 
* `threshold` Test-stat gate; 0.0 = always emit. 
* `nthreads` Forwarded to [**dp\_corr2d\_create()**](corr2d__core_8h.md#function-dp_corr2d_create): workers for the single-row-reference fast path (1 serial, &lt;= 0 every online core); the general 2-D path ignores it. 



**Returns:**

Heap-allocated state, or NULL when [**dp\_corr2d\_create()**](corr2d__core_8h.md#function-dp_corr2d_create) refuses the arguments (e.g. `dwell` 0) or allocation fails. 
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



### function dp\_detector2d\_destroy 

_Destroy and free._ 
```C++
void dp_detector2d_destroy (
    dp_detector2d_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function dp\_detector2d\_get\_state 

_Serialize the correlator, carry and last-dump result into_ `blob` _._
```C++
void dp_detector2d_get_state (
    const dp_detector2d_state_t * state,
    void * blob
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `blob` Destination; at least [**dp\_detector2d\_state\_bytes()**](detector2d__core_8h.md#function-dp_detector2d_state_bytes) bytes. 




        

<hr>



### function dp\_detector2d\_push 

_Stream an arbitrary-length CF32 chunk through the 2-D detector. The same pipeline as_ [_**dp\_detector\_push()**_](detector__core_8h.md#function-dp_detector_push) _, except that frames are ny\*nx complex samples and each detection event carries (row, col) for the peak location instead of a single lag index. In Python the result is always a list of (row, col, peak\_mag, noise\_est, test\_stat) tuples._
```C++
size_t dp_detector2d_push (
    dp_detector2d_state_t * state,
    const float _Complex * in,
    size_t n_in,
    det_result2d_t * result,
    size_t max_results
) 
```



Python's push() has room for 1024 detections a call, so a push that completes at most 1024 frames, counting the carry, loses nothing. One that fills the room loses the rest of its input, every later frame whether or not it would have made a detection, unless the rest is shorter than a frame: that is kept as the carry. The next push stays on the frame grid, its peaks at their own (row, col), only when what was lost is a whole number of frames; otherwise they are off by the remainder. Before v0.66 the room was 64, and a push past it kept up to ring\_cap/n - 1 of those frames for the next call and dropped the rest. #1992 and just-buildit/just-makeit#2184 track sizing the list to the call.




**Parameters:**


* `state` Allocated 2-D detector (non-NULL). 
* `in` CF32 input chunk of arbitrary length. 
* `n_in` Number of input samples in `in`. 
* `result` Caller-supplied array of at least `max_results` [**det\_result2d\_t**](structdet__result2d__t.md) structs; filled on return. 
* `max_results` Capacity of `result` (maximum detections to emit). A full `result` never loses input: a frame yields at most one detection, and once `result` is full the push takes the rest of its input only if the rest completes no frame (it is then the carry); otherwise it takes nothing more, stopping on the boundary of its last frame. [**dp\_detector2d\_consumed()**](detector2d__core_8h.md#function-dp_detector2d_consumed) says how many samples it took, and the caller offers the rest again. Input that runs out mid-frame is the carry, held inside (fewer than ny\*nx samples). With room 0 the push is full from the start, so a resume loop needs room for at least one; with that, a push of any input takes at least one sample. 



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



### function dp\_detector2d\_reset 

_Reset the 2-D correlator, the carry, and last-corr flag. Discards any partial frame carried between pushes and zeroes the coherent accumulator. The reference spectrum and FFT plans are preserved. The last-dump fields (peak\_row, peak\_col, peak\_mag, noise\_est, test\_stat) read 0 until the next dump, so the state blob after a reset matches a fresh object's (doppler#2642)._ 
```C++
void dp_detector2d_reset (
    dp_detector2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL.


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



### function dp\_detector2d\_set\_ref 

_Replace the reference image and recompute its spectrum._ 
```C++
int dp_detector2d_set_ref (
    dp_detector2d_state_t * state,
    const float _Complex * ref
) 
```



Always resets (the carry, corr2d accumulator, last-dump bookkeeping), even if the new reference is subsequently rejected. The new reference must have the same ny\*nx total size; see [**dp\_corr2d\_set\_ref()**](corr2d__core_8h.md#function-dp_corr2d_set_ref) for the single-row-fast- path rejection rule this forwards.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 



**Returns:**

0 on success, -1 if rejected by [**dp\_corr2d\_set\_ref()**](corr2d__core_8h.md#function-dp_corr2d_set_ref). 





        

<hr>



### function dp\_detector2d\_set\_state 

_Restore a detector from a blob made by_ [_**dp\_detector2d\_get\_state()**_](detector2d__core_8h.md#function-dp_detector2d_get_state) _._
```C++
int dp_detector2d_set_state (
    dp_detector2d_state_t * state,
    const void * blob
) 
```



Clears the last-surface flag (out\_buf is not in the blob) and the consumed count.




**Parameters:**


* `state` Must be non-NULL, created with the same configuration. 
* `blob` Source blob of [**dp\_detector2d\_state\_bytes()**](detector2d__core_8h.md#function-dp_detector2d_state_bytes) bytes. 



**Returns:**

`DP_OK`, or `DP_ERR_INVALID` for a header or child-blob mismatch (a child refused after the corr2d child was read leaves that child restored). 





        

<hr>



### function dp\_detector2d\_set\_threshold 

_Change threshold without rebuilding._ 
```C++
void dp_detector2d_set_threshold (
    dp_detector2d_state_t * state,
    float threshold
) 
```





**Parameters:**


* `state` Must be non-NULL. 
* `threshold` New threshold; 0.0 = always fire. 




        

<hr>



### function dp\_detector2d\_state\_bytes 

_Size in bytes of this detector's state blob._ 
```C++
size_t dp_detector2d_state_bytes (
    const dp_detector2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 



**Returns:**

Header + corr2d and framer child blobs + 2 u64 + 3 f32. 





        

<hr>
## Macro Definition Documentation





### define DETECTOR2D\_STATE\_MAGIC 

_State-blob magic ('DET2'); set\_state() rejects any other._ 
```C++
#define DETECTOR2D_STATE_MAGIC `DP_FOURCC ('D','E','T','2')`
```




<hr>



### define DETECTOR2D\_STATE\_VERSION 

_State-blob layout version (2; version-1 blobs are refused)._ 
```C++
#define DETECTOR2D_STATE_VERSION `2u`
```




<hr>



### define DET\_NOISE\_MODE\_T\_DEFINED 

_Include guard: det\_noise\_mode\_t is defined once across the detector headers that share it._ 
```C++
#define DET_NOISE_MODE_T_DEFINED 
```




<hr>



### define DET\_PEAK\_T\_DEFINED 

_Include guard:_ [_**det\_peak\_t**_](structdet__peak__t.md) _is defined once across headers._
```C++
#define DET_PEAK_T_DEFINED 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/detector2d/detector2d_core.h`

