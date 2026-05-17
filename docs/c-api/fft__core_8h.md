

# File fft\_core.h



[**FileList**](files.md) **>** [**fft**](dir_5dc24668fb1cbe963321608da9e9d4ca.md) **>** [**fft\_core.h**](fft__core_8h.md)

[Go to the source code of this file](fft__core_8h_source.md)

_Per-instance 1-D FFT using pocketfft directly._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include "dp/pocketfft.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**fft\_state\_t**](structfft__state__t.md) <br> |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fft\_state\_t**](structfft__state__t.md) \* | [**fft\_create**](#function-fft_create) (size\_t n, int sign, int nthreads) <br>_Create a 1-D FFT instance._  |
|  void | [**fft\_destroy**](#function-fft_destroy) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Destroy and free an fft instance._  |
|  size\_t | [**fft\_execute\_cf32**](#function-fft_execute_cf32) ([**fft\_state\_t**](structfft__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Out-of-place 1-D CF32 FFT._  |
|  size\_t | [**fft\_execute\_cf32\_max\_out**](#function-fft_execute_cf32_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples for CF32 execute (always == n)._  |
|  size\_t | [**fft\_execute\_cf64**](#function-fft_execute_cf64) ([**fft\_state\_t**](structfft__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_Out-of-place 1-D CF64 FFT._  |
|  size\_t | [**fft\_execute\_cf64\_max\_out**](#function-fft_execute_cf64_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples per execute call (always == n)._  |
|  size\_t | [**fft\_execute\_inplace\_cf32**](#function-fft_execute_inplace_cf32) ([**fft\_state\_t**](structfft__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_In-place 1-D CF32 FFT (copies in→out, then transforms in out)._  |
|  size\_t | [**fft\_execute\_inplace\_cf32\_max\_out**](#function-fft_execute_inplace_cf32_max_out) ([**fft\_state\_t**](structfft__state__t.md) \* state) <br>_Maximum output samples for inplace CF32 (always == n)._  |
|  size\_t | [**fft\_execute\_inplace\_cf64**](#function-fft_execute_inplace_cf64) ([**fft\_state\_t**](structfft__state__t.md) \* state, const double complex \* in, size\_t n\_in, double complex \* out) <br>_In-place 1-D CF64 FFT (copies in→out, then transforms in out)._  |
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

_Create a 1-D FFT instance._
```C++
fft_state_t * fft_create (
    size_t n,
    int sign,
    int nthreads
)
```



Allocates one CF64 and one CF32 pocketfft plan for length `n`. nthreads is accepted for API compatibility but ignored.




**Parameters:**


* `n` Transform length in samples.
* `sign` -1 for forward DFT, +1 for inverse.
* `nthreads` Ignored (pocketfft is single-threaded).



**Returns:**

Heap-allocated state, or NULL on allocation failure.







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

_Out-of-place 1-D CF32 FFT._
```C++
size_t fft_execute_cf32 (
    fft_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
)
```





**Parameters:**


* `state` Must be non-NULL.
* `in` Input buffer of length n\_in.
* `n_in` Number of input samples.
* `out` Output buffer of length &gt;= n.



**Returns:**

n (samples written).







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

_Out-of-place 1-D CF64 FFT._
```C++
size_t fft_execute_cf64 (
    fft_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
)
```





**Parameters:**


* `state` Must be non-NULL.
* `in` Input buffer of length n\_in (must equal state-&gt;n).
* `n_in` Number of input samples.
* `out` Output buffer of length &gt;= n.



**Returns:**

n (samples written).







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

_In-place 1-D CF32 FFT (copies in→out, then transforms in out)._
```C++
size_t fft_execute_inplace_cf32 (
    fft_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
)
```





**Parameters:**


* `state` Must be non-NULL.
* `in` Source; copied into out before the transform.
* `n_in` Number of input samples.
* `out` Buffer of length &gt;= n; must not alias in.



**Returns:**

n (samples written).







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

_In-place 1-D CF64 FFT (copies in→out, then transforms in out)._
```C++
size_t fft_execute_inplace_cf64 (
    fft_state_t * state,
    const double complex * in,
    size_t n_in,
    double complex * out
)
```





**Parameters:**


* `state` Must be non-NULL.
* `in` Source; copied into out before the transform.
* `n_in` Number of input samples.
* `out` Buffer of length &gt;= n; must not alias in.



**Returns:**

n (samples written).







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
