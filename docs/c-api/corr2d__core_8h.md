

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
|  [**corr2d\_state\_t**](structcorr2d__state__t.md) \* | [**corr2d\_create**](#function-corr2d_create) (const float complex \* ref, size\_t ny, size\_t nx, size\_t dwell, int nthreads) <br>_Create a 2-D FFT correlator._  |
|  void | [**corr2d\_destroy**](#function-corr2d_destroy) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Destroy and free a corr2d instance._  |
|  size\_t | [**corr2d\_execute**](#function-corr2d_execute) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Correlate one 2-D frame and optionally dump the accumulator._  |
|  size\_t | [**corr2d\_execute\_max\_out**](#function-corr2d_execute_max_out) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Maximum output samples per execute call (always == ny\*nx)._  |
|  void | [**corr2d\_reset**](#function-corr2d_reset) ([**corr2d\_state\_t**](structcorr2d__state__t.md) \* state) <br>_Zero the accumulator and reset the integration counter to 0._  |
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

_Create a 2-D FFT correlator._ 
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


* `ref` Reference image, flat row-major CF32, length ny\*nx. 
* `ny` Number of rows. 
* `nx` Number of columns. 
* `dwell` Integration depth; must be &gt;= 1. 
* `nthreads` Ignored (pocketfft is single-threaded). 



**Returns:**

Heap-allocated state, or NULL on failure. 





        

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

_Correlate one 2-D frame and optionally dump the accumulator._ 
```C++
size_t corr2d_execute (
    corr2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```



Steps:
* FFT2(in) → work\_fft
* `work_fft[k] *= ref_spec[k]`
* IFFT2(work\_fft) → work\_ifft (divide by ny\*nx)
* `accum[k] += work_ifft[k] / (ny*nx)`
* If count == dwell: copy accum → out, zero, reset, return ny\*nx.
* Otherwise: return 0.






**Parameters:**


* `state` Must be non-NULL. 
* `in` Flat row-major CF32 frame, length n\_in (must equal ny\*nx). 
* `n_in` Total number of input samples. 
* `out` Output buffer of length &gt;= ny\*nx. Only written on dump. 



**Returns:**

ny\*nx on a dump, 0 otherwise. 





        

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

_Zero the accumulator and reset the integration counter to 0._ 
```C++
void corr2d_reset (
    corr2d_state_t * state
) 
```





**Parameters:**


* `state` Must be non-NULL. 




        

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

