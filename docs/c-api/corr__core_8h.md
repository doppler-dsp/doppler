

# File corr\_core.h



[**FileList**](files.md) **>** [**corr**](dir_17ecfb211582dadfc5fc9d22d4d97fbd.md) **>** [**corr\_core.h**](corr__core_8h.md)

[Go to the source code of this file](corr__core_8h_source.md)

_1-D FFT-based cross-correlator with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "fft/fft_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**corr\_state\_t**](structcorr__state__t.md) <br>_1-D FFT correlator state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**corr\_state\_t**](structcorr__state__t.md) \* | [**corr\_create**](#function-corr_create) (const float complex \* ref, size\_t n, size\_t dwell, int nthreads, size\_t n\_out) <br>_Allocate a 1-D FFT correlator with coherent integrate-and-dump. Pre-computes conj(FFT(ref)) once at construction so each execute() call costs only two FFTs and n complex multiplies._ `ref` _may be freed after this returns. With_`dwell` _== 1 every call produces output; with larger values the accumulator absorbs_`dwell` _frames before dumping._ |
|  void | [**corr\_destroy**](#function-corr_destroy) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Destroy and free a corr instance._  |
|  size\_t | [**corr\_execute**](#function-corr_execute) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one frame and optionally dump the coherent accumulator. Runs: forward FFT → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, inverse FFT → normalise (÷ n). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. On the_`dwell-th` _call_`out` _is written, the accumulator is zeroed, and the counter resets; the function returns n\_out. All other calls return 0 and leave_`out` _unmodified. In Python, a dump returns an ndarray and a no-dump returns None._ |
|  size\_t | [**corr\_execute\_max\_out**](#function-corr_execute_max_out) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Maximum output samples per execute call (== n\_out)._  |
|  void | [**corr\_get\_state**](#function-corr_get_state) (const [**corr\_state\_t**](structcorr__state__t.md) \* state, void \* blob) <br> |
|  void | [**corr\_reset**](#function-corr_reset) ([**corr\_state\_t**](structcorr__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without tearing down the FFT plans. Does NOT recompute ref\_spec; use_ [_**corr\_set\_ref()**_](corr__core_8h.md#function-corr_set_ref) _to replace the reference._ |
|  void | [**corr\_set\_ref**](#function-corr_set_ref) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference signal and recompute conj(FFT(ref))._  |
|  int | [**corr\_set\_state**](#function-corr_set_state) ([**corr\_state\_t**](structcorr__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**corr\_state\_bytes**](#function-corr_state_bytes) (const [**corr\_state\_t**](structcorr__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CORR\_STATE\_MAGIC**](corr__core_8h.md#define-corr_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','O','R','R')`<br> |
| define  | [**CORR\_STATE\_VERSION**](corr__core_8h.md#define-corr_state_version)  `1u`<br> |

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
    int nthreads,
    size_t n_out
) 
```





**Parameters:**


* `ref` Reference signal, CF32, length `n`. 
* `n` Reference / FFT length in samples. 
* `dwell` Integration depth; must be &gt;= 1. Pass 1 for immediate output on every call. 
* `nthreads` Accepted for API compatibility; ignored. 
* `n_out` Inverse/output length; 0 =&gt; native (n). Must be &gt;= n. A larger value zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer length-n\_out grid — same peak, sub-bin lag resolution. Native is bit-exact and allocates no extra buffer. 



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

_Correlate one frame and optionally dump the coherent accumulator. Runs: forward FFT → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, inverse FFT → normalise (÷ n). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. On the_`dwell-th` _call_`out` _is written, the accumulator is zeroed, and the counter resets; the function returns n\_out. All other calls return 0 and leave_`out` _unmodified. In Python, a dump returns an ndarray and a no-dump returns None._
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
* `out` Output buffer for the correlation map (CF32, length n\_out); written only on a dump call. 



**Returns:**

n\_out on a dump call, 0 otherwise (None in Python). 
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

_Maximum output samples per execute call (== n\_out)._ 
```C++
size_t corr_execute_max_out (
    corr_state_t * state
) 
```




<hr>



### function corr\_get\_state 

```C++
void corr_get_state (
    const corr_state_t * state,
    void * blob
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



### function corr\_set\_state 

```C++
int corr_set_state (
    corr_state_t * state,
    const void * blob
) 
```




<hr>



### function corr\_state\_bytes 

```C++
size_t corr_state_bytes (
    const corr_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define CORR\_STATE\_MAGIC 

```C++
#define CORR_STATE_MAGIC `DP_FOURCC ('C','O','R','R')`
```




<hr>



### define CORR\_STATE\_VERSION 

```C++
#define CORR_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr/corr_core.h`

