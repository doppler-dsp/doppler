

# File fft2d\_core.h



[**FileList**](files.md) **>** [**fft2d**](dir_9009a3f6624dc57956402cd0407c056b.md) **>** [**fft2d\_core.h**](fft2d__core_8h.md)

[Go to the source code of this file](fft2d__core_8h_source.md)

_Per-instance 2-D FFT using pocketfft directly._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp/pocketfft.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**fft2d\_state\_t**](structfft2d__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fft2d\_state\_t**](structfft2d__state__t.md) \* | [**fft2d\_create**](#function-fft2d_create) (size\_t ny, size\_t nx, int sign, int nthreads) <br>_Create a 2-D FFT instance._  |
|  void | [**fft2d\_destroy**](#function-fft2d_destroy) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Destroy and free an fft2d instance._  |
|  size\_t | [**fft2d\_execute\_cf32**](#function-fft2d_execute_cf32) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Out-of-place 2-D CF32 FFT. Returns ny\*nx._  |
|  size\_t | [**fft2d\_execute\_cf32\_max\_out**](#function-fft2d_execute_cf32_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples for CF32 execute (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_cf64**](#function-fft2d_execute_cf64) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Out-of-place 2-D CF64 FFT. Returns ny\*nx._  |
|  size\_t | [**fft2d\_execute\_cf64\_max\_out**](#function-fft2d_execute_cf64_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples per execute call (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_inplace\_cf32**](#function-fft2d_execute_inplace_cf32) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_In-place 2-D CF32 FFT (copies in→out, then transforms)._  |
|  size\_t | [**fft2d\_execute\_inplace\_cf32\_max\_out**](#function-fft2d_execute_inplace_cf32_max_out) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state) <br>_Maximum output samples for inplace CF32 execute (ny \* nx)._  |
|  size\_t | [**fft2d\_execute\_inplace\_cf64**](#function-fft2d_execute_inplace_cf64) ([**fft2d\_state\_t**](structfft2d__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_In-place 2-D CF64 FFT (copies in→out, then transforms)._  |
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

_Create a 2-D FFT instance._
```C++
fft2d_state_t * fft2d_create (
    size_t ny,
    size_t nx,
    int sign,
    int nthreads
)
```





**Parameters:**


* `ny` Number of rows.
* `nx` Number of columns.
* `sign` -1 for forward DFT, +1 for inverse.
* `nthreads` Ignored (pocketfft is single-threaded).



**Returns:**

Heap-allocated state, or NULL on failure.







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

_Out-of-place 2-D CF32 FFT. Returns ny\*nx._
```C++
size_t fft2d_execute_cf32 (
    fft2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
)
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

_Out-of-place 2-D CF64 FFT. Returns ny\*nx._
```C++
size_t fft2d_execute_cf64 (
    fft2d_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
)
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

_In-place 2-D CF32 FFT (copies in→out, then transforms)._
```C++
size_t fft2d_execute_inplace_cf32 (
    fft2d_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
)
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

_In-place 2-D CF64 FFT (copies in→out, then transforms)._
```C++
size_t fft2d_execute_inplace_cf64 (
    fft2d_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
)
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
