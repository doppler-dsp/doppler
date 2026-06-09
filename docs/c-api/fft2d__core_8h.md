

# File fft2d\_core.h



[**FileList**](files.md) **>** [**fft2d**](dir_9009a3f6624dc57956402cd0407c056b.md) **>** [**fft2d\_core.h**](fft2d__core_8h.md)

[Go to the source code of this file](fft2d__core_8h_source.md)

_Per-instance 2-D FFT using pocketfft directly._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "pocketfft/pocketfft.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**fft2d\_state\_t**](structfft2d__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fft2d\_state\_t**](structfft2d__state__t.md) \* | [**fft2d\_create**](#function-fft2d_create) (size\_t ny, size\_t nx, int sign, int nthreads) <br>_Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two pocketfft 2-D plans are built at construction time — one CF64, one CF32. All execute calls accept and return flat row-major arrays of length ny\*nx; the Python layer may reshape them with .reshape(ny, nx)._ `nthreads` _is accepted for API parity but ignored._ |
|  void | [**fft2d\_destroy**](#function-fft2d_destroy) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Destroy and free an fft2d instance._  |
|  size\_t | [**fft2d\_execute\_cf32**](#function-fft2d_execute_cf32) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Compute an out-of-place 2-D DFT on a single-precision complex grid. Single-precision variant of_ [_**fft2d\_execute\_cf64()**_](fft2d__core_8h.md#function-fft2d_execute_cf64) _. Accepts and returns flat row-major CF32 arrays of length ny\*nx. Output is unnormalised;_`in` _and_`out` _must not alias._ |
|  size\_t | [**fft2d\_execute\_cf32\_max\_out**](#function-fft2d_execute_cf32_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples for CF32 execute (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_cf64**](#function-fft2d_execute_cf64) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Compute an out-of-place 2-D DFT on a double-precision complex grid._ `in` _is a flat row-major CF64 array of length ny\*nx. The output is written to the caller-supplied_`out` _buffer (also ny\*nx); the two must not alias. The transform is unnormalised._ |
|  size\_t | [**fft2d\_execute\_cf64\_max\_out**](#function-fft2d_execute_cf64_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples per execute call (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_inplace\_cf32**](#function-fft2d_execute_inplace_cf32) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF32 2-D). Single-precision variant of_[_**fft2d\_execute\_inplace\_cf64()**_](fft2d__core_8h.md#function-fft2d_execute_inplace_cf64) _. Copies ny\*nx CF32 samples then applies the CF32 2-D pocketfft plan to_`out` _._ |
|  size\_t | [**fft2d\_execute\_inplace\_cf32\_max\_out**](#function-fft2d_execute_inplace_cf32_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples for inplace CF32 execute (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_inplace\_cf64**](#function-fft2d_execute_inplace_cf64) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF64 2-D). The ny\*nx CF64 samples from_`in` _are first memcpy'd to_`out` _; the 2-D DFT is then applied to_`out` _in-place._`in` _is left unmodified. Useful when the caller owns_`out` _and wants to preserve_`in` _._ |
|  size\_t | [**fft2d\_execute\_inplace\_cf64\_max\_out**](#function-fft2d_execute_inplace_cf64_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples for inplace CF64 execute (ny \* nx)._  |
|  void | [**fft2d\_reset**](#function-fft2d_reset) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_No-op reset (plans are immutable after creation)._  |




























## Detailed Description


Holds two pocketfft plans — one CF64, one CF32 — for an ny × nx row-major transform. Input and output arrays are flat buffers of length ny\*nx; the Python wrapper class reshapes them. nthreads is accepted for API compatibility but ignored.


Lifecycle: 
```C++
fft2d_state_t *fft = fft2d_create(64, 64, -1, 1);
double complex out[64 * 64];
fft2d_execute_cf64(fft, in, 64 * 64, out);
fft2d_destroy(fft);
```
 


    
## Public Functions Documentation




### function fft2d\_create 

_Allocate a reusable 2-D FFT engine for a fixed ny×nx grid. Two pocketfft 2-D plans are built at construction time — one CF64, one CF32. All execute calls accept and return flat row-major arrays of length ny\*nx; the Python layer may reshape them with .reshape(ny, nx)._ `nthreads` _is accepted for API parity but ignored._
```C++
fft2d_state_t * fft2d_create (
    size_t ny,
    size_t nx,
    int sign,
    int nthreads
) 
```





**Parameters:**


* `ny` Number of rows (outer dimension). 
* `nx` Number of columns (inner dimension). 
* `sign` -1 for the forward DFT, +1 for the inverse DFT. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on failure. 
```C++
>>> from doppler.spectral import FFT2D
>>> import numpy as np
>>> fft2d = FFT2D(ny=4, nx=4, sign=-1, nthreads=1)
>>> fft2d.ny, fft2d.nx, fft2d.sign
(4, 4, -1)
>>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
>>> out = fft2d.execute_cf32(x)
>>> out.shape, out.dtype
((16,), dtype('complex64'))
>>> bool(np.allclose(out, 1.0))
True
```
 





        

<hr>



### function fft2d\_destroy 

_Destroy and free an fft2d instance._ 
```C++
void fft2d_destroy (
    fft2d_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function fft2d\_execute\_cf32 

_Compute an out-of-place 2-D DFT on a single-precision complex grid. Single-precision variant of_ [_**fft2d\_execute\_cf64()**_](fft2d__core_8h.md#function-fft2d_execute_cf64) _. Accepts and returns flat row-major CF32 arrays of length ny\*nx. Output is unnormalised;_`in` _and_`out` _must not alias._
```C++
size_t fft2d_execute_cf32 (
    fft2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated FFT2D engine (non-NULL). 
* `in` Flat row-major CF32 input, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx. 
* `out` Flat row-major CF32 output, length &gt;= ny\*nx (caller-allocated). 



**Returns:**

ny\*nx (number of samples written). 
```C++
>>> from doppler.spectral import FFT2D
>>> import numpy as np
>>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
>>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
>>> out = fft2d.execute_cf32(x)
>>> out.shape, out.dtype
((16,), dtype('complex64'))
>>> bool(np.allclose(out, 1.0))
True
```
 





        

<hr>



### function fft2d\_execute\_cf32\_max\_out 

_Maximum output samples for CF32 execute (ny \* nx)._ 
```C++
size_t fft2d_execute_cf32_max_out (
    fft2d_state_t * state
) 
```




<hr>



### function fft2d\_execute\_cf64 

_Compute an out-of-place 2-D DFT on a double-precision complex grid._ `in` _is a flat row-major CF64 array of length ny\*nx. The output is written to the caller-supplied_`out` _buffer (also ny\*nx); the two must not alias. The transform is unnormalised._
```C++
size_t fft2d_execute_cf64 (
    fft2d_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
) 
```





**Parameters:**


* `state` Allocated FFT2D engine (non-NULL). 
* `in` Flat row-major CF64 input, length ny\*nx. 
* `n_in` Number of input samples; must equal ny\*nx. 
* `out` Flat row-major CF64 output, length &gt;= ny\*nx (caller-allocated). 



**Returns:**

ny\*nx (number of samples written). 
```C++
>>> from doppler.spectral import FFT2D
>>> import numpy as np
>>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
>>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
>>> out = fft2d.execute_cf64(x)
>>> out.shape, out.dtype
((16,), dtype('complex128'))
>>> bool(np.allclose(out, 1.0))
True
```
 





        

<hr>



### function fft2d\_execute\_cf64\_max\_out 

_Maximum output samples per execute call (ny \* nx)._ 
```C++
size_t fft2d_execute_cf64_max_out (
    fft2d_state_t * state
) 
```




<hr>



### function fft2d\_execute\_inplace\_cf32 

_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF32 2-D). Single-precision variant of_[_**fft2d\_execute\_inplace\_cf64()**_](fft2d__core_8h.md#function-fft2d_execute_inplace_cf64) _. Copies ny\*nx CF32 samples then applies the CF32 2-D pocketfft plan to_`out` _._
```C++
size_t fft2d_execute_inplace_cf32 (
    fft2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated FFT2D engine (non-NULL). 
* `in` Source, ny\*nx CF32 flat row-major; not modified. 
* `n_in` Number of input samples; must equal ny\*nx. 
* `out` Destination, length &gt;= ny\*nx; must not alias in. 



**Returns:**

ny\*nx (number of samples written). 
```C++
>>> from doppler.spectral import FFT2D
>>> import numpy as np
>>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
>>> x = np.zeros(16, dtype=np.complex64); x[0] = 1.0
>>> out = fft2d.execute_inplace_cf32(x)
>>> bool(np.allclose(out, 1.0))
True
```
 





        

<hr>



### function fft2d\_execute\_inplace\_cf32\_max\_out 

_Maximum output samples for inplace CF32 execute (ny \* nx)._ 
```C++
size_t fft2d_execute_inplace_cf32_max_out (
    fft2d_state_t * state
) 
```




<hr>



### function fft2d\_execute\_inplace\_cf64 

_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF64 2-D). The ny\*nx CF64 samples from_`in` _are first memcpy'd to_`out` _; the 2-D DFT is then applied to_`out` _in-place._`in` _is left unmodified. Useful when the caller owns_`out` _and wants to preserve_`in` _._
```C++
size_t fft2d_execute_inplace_cf64 (
    fft2d_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
) 
```





**Parameters:**


* `state` Allocated FFT2D engine (non-NULL). 
* `in` Source, ny\*nx CF64 flat row-major; not modified. 
* `n_in` Number of input samples; must equal ny\*nx. 
* `out` Destination, length &gt;= ny\*nx; must not alias in. 



**Returns:**

ny\*nx (number of samples written). 
```C++
>>> from doppler.spectral import FFT2D
>>> import numpy as np
>>> fft2d = FFT2D(ny=4, nx=4, sign=-1)
>>> x = np.zeros(16, dtype=np.complex128); x[0] = 1.0
>>> out = fft2d.execute_inplace_cf64(x)
>>> bool(np.allclose(out, 1.0))
True
```
 





        

<hr>



### function fft2d\_execute\_inplace\_cf64\_max\_out 

_Maximum output samples for inplace CF64 execute (ny \* nx)._ 
```C++
size_t fft2d_execute_inplace_cf64_max_out (
    fft2d_state_t * state
) 
```




<hr>



### function fft2d\_reset 

_No-op reset (plans are immutable after creation)._ 
```C++
void fft2d_reset (
    fft2d_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fft2d/fft2d_core.h`

