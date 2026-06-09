

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
|  [**corr\_state\_t**](structcorr__state__t.md) \* | [**corr\_create**](#function-corr_create) (const float complex \* ref, size\_t n, size\_t dwell, int nthreads) <br>_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._ |
|  void | [**corr\_destroy**](#function-corr_destroy) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Destroy and free a corr instance._  |
|  size\_t | [**corr\_execute**](#function-corr_execute) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one frame and optionally dump the coherent accumulator. Runs the six-step pipeline: forward FFT → pointwise multiply with ref\_spec → inverse FFT → normalise (÷ n) → accumulate → conditional dump. On the_ `dwell-th` _call the accumulator is copied to_`out` _, zeroed, and the counter resets; the function returns n. All other calls return 0 and leave_`out` _unmodified. In Python, a dump returns an ndarray and a no-dump returns None._ |
|  size\_t | [**corr\_execute\_max\_out**](#function-corr_execute_max_out) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Maximum output samples per execute call (always == n)._  |
|  void | [**corr\_reset**](#function-corr_reset) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without tearing down the FFT plans. Does NOT recompute ref\_spec; use_ [_**corr\_set\_ref()**_](corr__core_8h.md#function-corr_set_ref) _to replace the reference._ |
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

_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._
```C++
corr_state_t * corr_create (
    const float complex * ref,
    size_t n,
    size_t dwell,
    int nthreads
) 
```





**Parameters:**


* `ref` Reference signal, CF32, length `n`. 
* `n` Reference / FFT length in samples. 
* `dwell` Integration depth; must be &gt;= 1. Pass 1 for immediate output on every call. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.spectral import Corr
>>> import numpy as np
>>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
>>> corr = Corr(ref=ref, dwell=1, nthreads=1)
>>> corr.n, corr.dwell, corr.count
(4, 1, 0)
```
 





        

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

_Correlate one frame and optionally dump the coherent accumulator. Runs the six-step pipeline: forward FFT → pointwise multiply with ref\_spec → inverse FFT → normalise (÷ n) → accumulate → conditional dump. On the_ `dwell-th` _call the accumulator is copied to_`out` _, zeroed, and the counter resets; the function returns n. All other calls return 0 and leave_`out` _unmodified. In Python, a dump returns an ndarray and a no-dump returns None._
```C++
size_t corr_execute (
    corr_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated correlator (non-NULL). 
* `in` Input frame, CF32, length state-&gt;n. 
* `n_in` Number of input samples; must equal state-&gt;n. 
* `out` Output buffer for the correlation map (CF32, length state-&gt;n); written only on a dump call. 



**Returns:**

n on a dump call, 0 otherwise (None in Python). 
```C++
>>> from doppler.spectral import Corr
>>> import numpy as np
>>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
>>> corr = Corr(ref=ref, dwell=2)
>>> x = np.ones(4, dtype=np.complex64)
>>> corr.execute(x) is None   # frame 1 — no dump yet
True
>>> corr.execute(x).tolist()  # frame 2 — dump
[(2+0j), (2+0j), (2+0j), (2+0j)]
```
 





        

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

_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without tearing down the FFT plans. Does NOT recompute ref\_spec; use_ [_**corr\_set\_ref()**_](corr__core_8h.md#function-corr_set_ref) _to replace the reference._
```C++
void corr_reset (
    corr_state_t * state
) 
```




```C++
>>> from doppler.spectral import Corr
>>> import numpy as np
>>> ref = np.zeros(4, dtype=np.complex64); ref[0] = 1.0
>>> corr = Corr(ref=ref, dwell=3)
>>> _ = corr.execute(np.ones(4, dtype=np.complex64))
>>> corr.count
1
>>> corr.reset()
>>> corr.count
0
```
 


        

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

