

# File corr2d\_core.h



[**FileList**](files.md) **>** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md) **>** [**corr2d\_core.h**](corr2d__core_8h.md)

[Go to the source code of this file](corr2d__core_8h_source.md)

_2-D FFT-based cross-correlator with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "fft2d/fft2d_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**corr2d\_state\_t**](structcorr2d__state__t.md) <br>_2-D FFT correlator state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr2d\_create**](#function-corr2d_create) (const float complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**corr\_create()**_](corr__core_8h.md#function-corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  void | [**corr2d\_destroy**](#function-corr2d_destroy) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Destroy and free a corr2d instance._  |
|  size\_t | [**corr2d\_execute**](#function-corr2d_execute) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → IFFT2 → normalise (÷ ny\*nx) → accumulate → conditional dump. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._  |
|  size\_t | [**corr2d\_execute\_max\_out**](#function-corr2d_execute_max_out) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Maximum output samples per execute call (always == ny\*nx)._  |
|  void | [**corr2d\_reset**](#function-corr2d_reset) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._  |
|  void | [**corr2d\_set\_ref**](#function-corr2d_set_ref) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference and recompute conj(FFT2(ref))._  |




























## Detailed Description


Two-dimensional extension of corr\_core: all buffers are ny×nx row-major flat arrays of length ny\*nx. The correlation theorem extends naturally:


`R_xh[i,j] = IFFT2( FFT2(x) · conj(FFT2(h)) ) / (ny*nx)`


The reference spectrum is pre-computed at create time. The int-dump semantics are identical to the 1-D case: coherently sum `dwell` frames, then dump.


Lifecycle: 
```C++
float complex ref[NY * NX] = { ... };    // row-major 2-D reference
corr2d_state_t *c = corr2d_create(ref, NY, NX, 4, 1);
float complex out[NY * NX];
for (int i = 0; i < 4; i++) {
    size_t n_out = corr2d_execute(c, frame[i], NY*NX, out);
    if (n_out) process_2d(out, NY, NX);   // fires once, on i == 3
}
corr2d_destroy(c);
```
 


    
## Public Functions Documentation




### function corr2d\_create 

_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**corr\_create()**_](corr__core_8h.md#function-corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._
```C++
corr2d_state_t * corr2d_create (
    const float complex * ref,
    size_t ny,
    size_t nx,
    size_t dwell,
    int nthreads
) 
```





**Parameters:**


* `ref` Reference image, 2-D (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Integration depth; must be &gt;= 1. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on failure. 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((4, 4), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=1, nthreads=1)
>>> c.ny, c.nx, c.dwell, c.count
(4, 4, 1, 0)
```
 





        

<hr>



### function corr2d\_destroy 

_Destroy and free a corr2d instance._ 
```C++
void corr2d_destroy (
    corr2d_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function corr2d\_execute 

_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → IFFT2 → normalise (÷ ny\*nx) → accumulate → conditional dump. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._ 
```C++
size_t corr2d_execute (
    corr2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated 2-D correlator (non-NULL). 
* `in` Input frame, flat row-major CF32, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx. 
* `out` Output buffer for the correlation map (CF32, length ny\*nx); written only on a dump call. 



**Returns:**

ny\*nx on a dump, 0 otherwise (None in Python). 
```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=2)
>>> x = np.ones((2, 2), dtype=np.complex64)
>>> c.execute(x) is None   # frame 1 — no dump
True
>>> c.execute(x).tolist()  # frame 2 — dump
[(2+0j), (2+0j), (2+0j), (2+0j)]
```
 





        

<hr>



### function corr2d\_execute\_max\_out 

_Maximum output samples per execute call (always == ny\*nx)._ 
```C++
size_t corr2d_execute_max_out (
    corr2d_state_t * state
) 
```




<hr>



### function corr2d\_reset 

_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._ 
```C++
void corr2d_reset (
    corr2d_state_t * state
) 
```




```C++
>>> from doppler.spectral import Corr2D
>>> import numpy as np
>>> ref = np.zeros((2, 2), dtype=np.complex64); ref[0, 0] = 1.0
>>> c = Corr2D(ref=ref, dwell=3)
>>> _ = c.execute(np.ones((2, 2), dtype=np.complex64))
>>> c.count
1
>>> c.reset()
>>> c.count
0
```
 


        

<hr>



### function corr2d\_set\_ref 

_Replace the reference and recompute conj(FFT2(ref))._ 
```C++
void corr2d_set_ref (
    corr2d_state_t * state,
    const float complex * ref
) 
```



Also resets accumulator and counter.




**Parameters:**


* `state` Must be non-NULL. 
* `ref` New reference, flat row-major CF32, length ny\*nx. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr2d/corr2d_core.h`

