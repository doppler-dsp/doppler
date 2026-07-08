

# File corr2d\_core.h



[**FileList**](files.md) **>** [**corr2d**](dir_55247951d314f4b4a6db9bf46862b830.md) **>** [**corr2d\_core.h**](corr2d__core_8h.md)

[Go to the source code of this file](corr2d__core_8h_source.md)

_2-D FFT-based cross-correlator with coherent integrate-and-dump._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp_state.h"`
* `#include "fft2d/fft2d_core.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**corr2d\_state\_t**](structcorr2d__state__t.md) <br>_2-D FFT correlator state._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr2d\_create**](#function-corr2d_create) (const float complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads, size\_t ny\_out, size\_t nx\_out) <br>_Allocate a 2-D FFT correlator with coherent integrate-and-dump. Two-dimensional extension of_ [_**corr\_create()**_](corr__core_8h.md#function-corr_create) _. The reference is a flat row-major ny×nx CF32 array; its conjugate spectrum is pre-computed once so each execute() call costs two 2-D FFTs plus ny\*nx complex multiplies. The Python wrapper requires_`ref` _to be a 2-D ndarray with shape (ny, nx); it passes a flat view to C._ |
|  void | [**corr2d\_destroy**](#function-corr2d_destroy) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Destroy and free a corr2d instance._  |
|  size\_t | [**corr2d\_execute**](#function-corr2d_execute) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._ |
|  size\_t | [**corr2d\_execute\_max\_out**](#function-corr2d_execute_max_out) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Maximum output samples per execute call (always == ny\*nx)._  |
|  void | [**corr2d\_get\_state**](#function-corr2d_get_state) (const [**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, void \* blob) <br> |
|  void | [**corr2d\_reset**](#function-corr2d_reset) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0. Equivalent to starting a fresh dwell cycle without rebuilding FFT plans or recomputing ref\_spec._  |
|  void | [**corr2d\_set\_ref**](#function-corr2d_set_ref) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const float complex \* ref) <br>_Replace the reference and recompute conj(FFT2(ref))._  |
|  int | [**corr2d\_set\_state**](#function-corr2d_set_state) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const void \* blob) <br> |
|  size\_t | [**corr2d\_state\_bytes**](#function-corr2d_state_bytes) (const [**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br> |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CORR2D\_STATE\_MAGIC**](corr2d__core_8h.md#define-corr2d_state_magic)  `[**DP\_FOURCC**](dp__state_8h.md#define-dp_fourcc) ('C','R','2','D')`<br> |
| define  | [**CORR2D\_STATE\_VERSION**](corr2d__core_8h.md#define-corr2d_state_version)  `1u`<br> |

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
    int nthreads,
    size_t ny_out,
    size_t nx_out
) 
```





**Parameters:**


* `ref` Reference image, 2-D (ny, nx) CF32 ndarray in Python. 
* `ny` Number of rows in the reference and input frames. 
* `nx` Number of columns in the reference and input frames. 
* `dwell` Integration depth; must be &gt;= 1. 
* `nthreads` Accepted for API compatibility; ignored. 
* `ny_out` Inverse/output rows; 0 =&gt; native (ny). Must be &gt;= ny. A larger output zero-pads the cross-spectrum before the inverse, returning the band-limited (Dirichlet) interpolation of the correlation on a finer (ny\_out, nx\_out) grid — same peak, sub-bin resolution. Native is bit-exact and allocates no extra buffers. 
* `nx_out` Inverse/output columns; 0 =&gt; native (nx). Must be &gt;= nx. 



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

_Correlate one 2-D frame and optionally dump the coherent accumulator. Runs the 2-D pipeline: FFT2 → pointwise multiply with ref\_spec → accumulate the cross-spectrum; on dump, IFFT2 → normalise (÷ ny\*nx). Accumulating in the frequency domain and inverting once is exactly the per-frame inverse summed, by linearity of the IFFT — valid because the dwell is_ **coherent** _(a complex sum); a non-coherent (magnitude) integration could not defer the inverse. The Python wrapper accepts a (ny, nx) CF32 ndarray; a dump returns a flat length-ny\*nx ndarray, a no-dump returns None._
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



### function corr2d\_get\_state 

```C++
void corr2d_get_state (
    const corr2d_state_t * state,
    void * blob
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



### function corr2d\_set\_state 

```C++
int corr2d_set_state (
    corr2d_state_t * state,
    const void * blob
) 
```




<hr>



### function corr2d\_state\_bytes 

```C++
size_t corr2d_state_bytes (
    const corr2d_state_t * state
) 
```




<hr>
## Macro Definition Documentation





### define CORR2D\_STATE\_MAGIC 

```C++
#define CORR2D_STATE_MAGIC `DP_FOURCC ('C','R','2','D')`
```




<hr>



### define CORR2D\_STATE\_VERSION 

```C++
#define CORR2D_STATE_VERSION `1u`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/corr2d/corr2d_core.h`

