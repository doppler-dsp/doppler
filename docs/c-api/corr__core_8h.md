

# File corr\_core.h



[**FileList**](files.md) **>** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md) **>** [**corr\_core.h**](corr__core_8h.md)

[Go to the source code of this file](corr__core_8h_source.md)

_1-D FFT-based cross-correlator with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "fft/fft_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**corr\_state\_t**](structcorr__state__t.md) <br>_1-D FFT correlator state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**corr\_state\_t**](structcorr__state__t.md) \* | [**corr\_create**](#function-corr_create) (const float complex \* ref, size\_t n, size\_t dwell, int nthreads) <br>_Create a 1-D FFT correlator._  |
|  void | [**corr\_destroy**](#function-corr_destroy) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Destroy and free a corr instance._  |
|  size\_t | [**corr\_execute**](#function-corr_execute) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one frame and optionally dump the accumulator._  |
|  size\_t | [**corr\_execute\_max\_out**](#function-corr_execute_max_out) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Maximum output samples per execute call (always == n)._  |
|  void | [**corr\_reset**](#function-corr_reset) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0._  |
|  void | [**corr\_set\_ref**](#function-corr_set_ref) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference signal and recompute conj(FFT(ref))._  |




























## Detailed Description


Implements the correlation theorem: cross-correlation in the lag domain is equivalent to pointwise multiplication of the forward spectrum with the conjugate reference spectrum, followed by an inverse FFT.


`R_xh[τ] = IFFT( FFT(x) · conj(FFT(h)) ) / n`


The reference spectrum `conj(FFT(h))` is pre-computed at create time and stored in `ref_spec`, so each execute call costs two FFTs (forward + inverse) plus n complex multiplies — O(n log n).


Integrate-and-dump (int-dump) coherently sums `dwell` successive correlation maps into an accumulator. On the `dwell`-th call execute() copies the accumulator to the caller's output buffer, zeroes the accumulator, resets the counter, and returns `n`. All other calls return 0 (no output produced). With `dwell = 1` the object is a pure, zero- latency correlator.


Lifecycle: 
```C++
float complex ref[N] = { ... };
corr_state_t *c = corr_create(ref, N, 8, 1);   // 8-frame coherent dwell
float complex out[N];
for (int i = 0; i < 8; i++) {
    size_t n_out = corr_execute(c, frame[i], N, out);
    if (n_out) process(out, N);   // fires once, on i == 7
}
corr_destroy(c);
```



Thread safety: a single state must not be used concurrently from multiple threads; create separate instances per thread. 


    
## Public Functions Documentation




### function corr\_create 

_Create a 1-D FFT correlator._ 
```C++
corr_state_t * corr_create (
    const float complex * ref,
    size_t n,
    size_t dwell,
    int nthreads
) 
```



Allocates two FFT plans, computes `conj(FFT(ref))` once, and zeroes the accumulator. The caller may free or reuse `ref` after this call returns.




**Parameters:**


* `ref` Reference signal of length `n` (CF32, row-major). 
* `n` Transform / reference length in samples. 
* `dwell` Integration depth. Must be &gt;= 1. Pass 1 for pure correlation (no accumulation). 
* `nthreads` Accepted for API compatibility; ignored (pocketfft is single-threaded). 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 





        

<hr>



### function corr\_destroy 

_Destroy and free a corr instance._ 
```C++
void corr_destroy (
    corr_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function corr\_execute 

_Correlate one frame and optionally dump the accumulator._ 
```C++
size_t corr_execute (
    corr_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```



Steps:
* FFT(in) → work\_fft
* `work_fft[k] *= ref_spec[k]` (frequency-domain multiplication)
* IFFT(work\_fft) → work\_ifft (unnormalized; divide by n)
* `accum[k] += work_ifft[k] / n`
* count++
* If count == dwell: copy accum → out, zero accum, reset count, return n.
* Otherwise: return 0 (no output this call).






**Parameters:**


* `state` Must be non-NULL. 
* `in` Input frame of length n\_in (must equal state-&gt;n). 
* `n_in` Number of input samples; ignored beyond state-&gt;n. 
* `out` Output buffer of length &gt;= state-&gt;n. Only written when the function returns n (dump). 



**Returns:**

n on a dump, 0 otherwise. 





        

<hr>



### function corr\_execute\_max\_out 

_Maximum output samples per execute call (always == n)._ 
```C++
size_t corr_execute_max_out (
    corr_state_t * state
) 
```




<hr>



### function corr\_reset 

_Zero the accumulator and reset the integration counter to 0._ 
```C++
void corr_reset (
    corr_state_t * state
) 
```



Equivalent to starting a fresh dwell cycle. Does NOT recompute ref\_spec; use [**corr\_set\_ref()**](corr__core_8h.md#function-corr_set_ref) for that.




**Parameters:**


* `state` Must be non-NULL. 




        

<hr>



### function corr\_set\_ref 

_Replace the reference signal and recompute conj(FFT(ref))._ 
```C++
void corr_set_ref (
    corr_state_t * state,
    const float complex * ref
) 
```



Also resets the accumulator and counter (as if [**corr\_reset()**](corr__core_8h.md#function-corr_reset) were called). Useful when the reference must change between dwells without tearing down the FFT plans.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference signal of length state-&gt;n. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr/corr_core.h`

