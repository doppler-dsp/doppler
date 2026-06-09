

# File fft\_core.h



[**FileList**](files.md) **>** [**fft**](dir_5dc24668fb1cbe963321608da9e9d4ca.md) **>** [**fft\_core.h**](fft__core_8h.md)

[Go to the source code of this file](fft__core_8h_source.md)

_Per-instance 1-D FFT using pocketfft directly._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "pocketfft/pocketfft.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**fft\_state\_t**](structfft__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft\_create**](#function-fft_create) (size\_t n, int sign, int nthreads) <br>_Allocate a reusable 1-D FFT engine for a fixed length and sign. Two pocketfft plans are created at construction time — one for CF64 and one for CF32 — so execute calls carry no plan-setup overhead. The same instance may be called repeatedly for independent input vectors of the same length._ `nthreads` _is accepted for API parity but is ignored; pocketfft plans are single-threaded._ |
|  void | [**fft\_destroy**](#function-fft_destroy) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Destroy and free an fft instance._  |
|  size\_t | [**fft\_execute\_cf32**](#function-fft_execute_cf32) ([**fft\_state\_t**](structfft__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Compute an out-of-place 1-D DFT on a single-precision complex input. Identical to_ [_**fft\_execute\_cf64()**_](fft__core_8h.md#function-fft_execute_cf64) _but operates on float complex (CF32) buffers, halving memory bandwidth relative to the double-precision variant. Output is unnormalised;_`in` _and_`out` _must not alias._ |
|  size\_t | [**fft\_execute\_cf32\_max\_out**](#function-fft_execute_cf32_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples for CF32 execute (always == n)._  |
|  size\_t | [**fft\_execute\_cf64**](#function-fft_execute_cf64) ([**fft\_state\_t**](structfft__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Compute an out-of-place 1-D DFT on a double-precision complex input. The output is written to a fresh caller-supplied buffer;_ `in` _and_`out` _must not alias. The transform is unnormalised: the inverse DFT (sign=+1) does NOT divide by n. Both buffers must be exactly state-&gt;n elements long._ |
|  size\_t | [**fft\_execute\_cf64\_max\_out**](#function-fft_execute_cf64_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples per execute call (always == n)._  |
|  size\_t | [**fft\_execute\_inplace\_cf32**](#function-fft_execute_inplace_cf32) ([**fft\_state\_t**](structfft__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF32). Single-precision variant of_[_**fft\_execute\_inplace\_cf64()**_](fft__core_8h.md#function-fft_execute_inplace_cf64) _. Copies state-&gt;n CF32 samples from_`in` _to_`out` _, then transforms_`out` _with the CF32 pocketfft plan._`in` _is left unmodified._ |
|  size\_t | [**fft\_execute\_inplace\_cf32\_max\_out**](#function-fft_execute_inplace_cf32_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples for inplace CF32 (always == n)._  |
|  size\_t | [**fft\_execute\_inplace\_cf64**](#function-fft_execute_inplace_cf64) ([**fft\_state\_t**](structfft__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF64). The copy step lets callers preserve their input while keeping the output buffer hot in cache. Semantically identical to_[_**fft\_execute\_cf64()**_](fft__core_8h.md#function-fft_execute_cf64) _for separate_`in` _/_`out` _pointers; use this variant when the caller already owns_`out` _and wants the result there without a second allocation._ |
|  size\_t | [**fft\_execute\_inplace\_cf64\_max\_out**](#function-fft_execute_inplace_cf64_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples for inplace CF64 (always == n)._  |
|  void | [**fft\_reset**](#function-fft_reset) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_No-op reset (plans are immutable after creation)._  |




























## Detailed Description


Holds two pocketfft plans — one for CF64, one for CF32 — allocated at create time for the requested transform length and sign. nthreads is accepted for API compatibility but ignored; pocketfft is single-threaded.


Lifecycle: 
```C++
fft_state_t *fft = fft_create(1024, -1, 1);
double complex out[1024];
fft_execute_cf64(fft, in, 1024, out);
fft_destroy(fft);
```
 


    
## Public Functions Documentation




### function fft\_create 

_Allocate a reusable 1-D FFT engine for a fixed length and sign. Two pocketfft plans are created at construction time — one for CF64 and one for CF32 — so execute calls carry no plan-setup overhead. The same instance may be called repeatedly for independent input vectors of the same length._ `nthreads` _is accepted for API parity but is ignored; pocketfft plans are single-threaded._
```C++
fft_state_t * fft_create (
    size_t n,
    int sign,
    int nthreads
) 
```





**Parameters:**


* `n` Transform length in samples (power of two recommended). 
* `sign` -1 for the forward DFT, +1 for the inverse DFT. 
* `nthreads` Accepted for API compatibility; ignored. 



**Returns:**

Heap-allocated state, or NULL on allocation failure. 
```C++
>>> from doppler.spectral import FFT
>>> import numpy as np
>>> fft = FFT(n=4, sign=-1, nthreads=1)
>>> fft.n, fft.sign
(4, -1)
>>> x = np.array([1, 0, 0, 0], dtype=np.complex64)
>>> fft.execute_cf32(x).tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function fft\_destroy 

_Destroy and free an fft instance._ 
```C++
void fft_destroy (
    fft_state_t * state
) 
```





**Parameters:**


* `state` May be NULL. 




        

<hr>



### function fft\_execute\_cf32 

_Compute an out-of-place 1-D DFT on a single-precision complex input. Identical to_ [_**fft\_execute\_cf64()**_](fft__core_8h.md#function-fft_execute_cf64) _but operates on float complex (CF32) buffers, halving memory bandwidth relative to the double-precision variant. Output is unnormalised;_`in` _and_`out` _must not alias._
```C++
size_t fft_execute_cf32 (
    fft_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated FFT engine (non-NULL). 
* `in` Input buffer of length state-&gt;n (CF32, row-major). 
* `n_in` Number of input samples; must equal state-&gt;n. 
* `out` Output buffer of length &gt;= state-&gt;n (CF32, caller-allocated). 



**Returns:**

n (number of samples written). 
```C++
>>> from doppler.spectral import FFT
>>> import numpy as np
>>> fft = FFT(n=4, sign=-1)
>>> x = np.ones(4, dtype=np.complex64)
>>> fft.execute_cf32(x).tolist()
[(4+0j), 0j, 0j, 0j]
```
 





        

<hr>



### function fft\_execute\_cf32\_max\_out 

_Maximum output samples for CF32 execute (always == n)._ 
```C++
size_t fft_execute_cf32_max_out (
    fft_state_t * state
) 
```




<hr>



### function fft\_execute\_cf64 

_Compute an out-of-place 1-D DFT on a double-precision complex input. The output is written to a fresh caller-supplied buffer;_ `in` _and_`out` _must not alias. The transform is unnormalised: the inverse DFT (sign=+1) does NOT divide by n. Both buffers must be exactly state-&gt;n elements long._
```C++
size_t fft_execute_cf64 (
    fft_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
) 
```





**Parameters:**


* `state` Allocated FFT engine (non-NULL). 
* `in` Input buffer of length state-&gt;n (CF64, row-major). 
* `n_in` Number of input samples; must equal state-&gt;n. 
* `out` Output buffer of length &gt;= state-&gt;n (CF64, caller-allocated). 



**Returns:**

n (number of samples written). 
```C++
>>> from doppler.spectral import FFT
>>> import numpy as np
>>> fft = FFT(n=4, sign=-1)
>>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
>>> fft.execute_cf64(x).tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function fft\_execute\_cf64\_max\_out 

_Maximum output samples per execute call (always == n)._ 
```C++
size_t fft_execute_cf64_max_out (
    fft_state_t * state
) 
```




<hr>



### function fft\_execute\_inplace\_cf32 

_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF32). Single-precision variant of_[_**fft\_execute\_inplace\_cf64()**_](fft__core_8h.md#function-fft_execute_inplace_cf64) _. Copies state-&gt;n CF32 samples from_`in` _to_`out` _, then transforms_`out` _with the CF32 pocketfft plan._`in` _is left unmodified._
```C++
size_t fft_execute_inplace_cf32 (
    fft_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
) 
```





**Parameters:**


* `state` Allocated FFT engine (non-NULL). 
* `in` Source buffer, state-&gt;n CF32 samples; not modified. 
* `n_in` Number of input samples; must equal state-&gt;n. 
* `out` Destination buffer, length &gt;= state-&gt;n; must not alias in. 



**Returns:**

n (number of samples written). 
```C++
>>> from doppler.spectral import FFT
>>> import numpy as np
>>> fft = FFT(n=4, sign=-1)
>>> x = np.array([1, 0, 0, 0], dtype=np.complex64)
>>> fft.execute_inplace_cf32(x).tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function fft\_execute\_inplace\_cf32\_max\_out 

_Maximum output samples for inplace CF32 (always == n)._ 
```C++
size_t fft_execute_inplace_cf32_max_out (
    fft_state_t * state
) 
```




<hr>



### function fft\_execute\_inplace\_cf64 

_Copy_ `in` _into_`out` _, then transform_`out` _in-place (CF64). The copy step lets callers preserve their input while keeping the output buffer hot in cache. Semantically identical to_[_**fft\_execute\_cf64()**_](fft__core_8h.md#function-fft_execute_cf64) _for separate_`in` _/_`out` _pointers; use this variant when the caller already owns_`out` _and wants the result there without a second allocation._
```C++
size_t fft_execute_inplace_cf64 (
    fft_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
) 
```





**Parameters:**


* `state` Allocated FFT engine (non-NULL). 
* `in` Source buffer, state-&gt;n CF64 samples; not modified. 
* `n_in` Number of input samples; must equal state-&gt;n. 
* `out` Destination buffer, length &gt;= state-&gt;n; must not alias in. 



**Returns:**

n (number of samples written). 
```C++
>>> from doppler.spectral import FFT
>>> import numpy as np
>>> fft = FFT(n=4, sign=-1)
>>> x = np.array([1, 0, 0, 0], dtype=np.complex128)
>>> fft.execute_inplace_cf64(x).tolist()
[(1+0j), (1+0j), (1+0j), (1+0j)]
```
 





        

<hr>



### function fft\_execute\_inplace\_cf64\_max\_out 

_Maximum output samples for inplace CF64 (always == n)._ 
```C++
size_t fft_execute_inplace_cf64_max_out (
    fft_state_t * state
) 
```




<hr>



### function fft\_reset 

_No-op reset (plans are immutable after creation)._ 
```C++
void fft_reset (
    fft_state_t * state
) 
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fft/fft_core.h`

