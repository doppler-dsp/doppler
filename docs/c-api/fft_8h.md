

# File fft.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**fft.h**](fft_8h.md)

[Go to the source code of this file](fft_8h_source.md)

_Fast Fourier Transform — setup and execution._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_fft2d\_s | [**dp\_fft2d\_t**](#typedef-dp_fft2d_t)  <br>_Opaque per-instance 2-D FFT plan._  |
| typedef struct dp\_fft\_s | [**dp\_fft\_t**](#typedef-dp_fft_t)  <br>_Opaque per-instance 1-D FFT plan._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_fft1d\_execute**](#function-dp_fft1d_execute) (const double complex \* input, double complex \* output) <br>_Execute an out-of-place 1-D FFT._  |
|  void | [**dp\_fft1d\_execute\_cf32**](#function-dp_fft1d_execute_cf32) (const float complex \* input, float complex \* output) <br>_Execute an out-of-place 1-D FFT on single-precision complex data._  |
|  void | [**dp\_fft1d\_execute\_inplace**](#function-dp_fft1d_execute_inplace) (double complex \* data) <br>_Execute an in-place 1-D FFT._  |
|  void | [**dp\_fft1d\_execute\_inplace\_cf32**](#function-dp_fft1d_execute_inplace_cf32) (float complex \* data) <br>_Execute an in-place 1-D FFT on single-precision complex data._  |
|  [**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* | [**dp\_fft2d\_create**](#function-dp_fft2d_create) (size\_t ny, size\_t nx, int sign, int nthreads) <br>_Create a 2-D FFT plan._  |
|  void | [**dp\_fft2d\_destroy**](#function-dp_fft2d_destroy) ([**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* plan) <br>_Destroy a 2-D plan and free all memory._  |
|  void | [**dp\_fft2d\_execute**](#function-dp_fft2d_execute) (const double complex \* input, double complex \* output) <br>_Execute an out-of-place 2-D FFT._  |
|  void | [**dp\_fft2d\_execute\_cf32**](#function-dp_fft2d_execute_cf32) (const float complex \* input, float complex \* output) <br>_Execute an out-of-place 2-D FFT on single-precision complex data._  |
|  void | [**dp\_fft2d\_execute\_inplace**](#function-dp_fft2d_execute_inplace) (double complex \* data) <br>_Execute an in-place 2-D FFT._  |
|  void | [**dp\_fft2d\_execute\_inplace\_cf32**](#function-dp_fft2d_execute_inplace_cf32) (float complex \* data) <br>_Execute an in-place 2-D FFT on single-precision complex data._  |
|  void | [**dp\_fft2d\_run\_cf32**](#function-dp_fft2d_run_cf32) (const [**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* plan, const float complex \* in, float complex \* out) <br> |
|  void | [**dp\_fft2d\_run\_cf64**](#function-dp_fft2d_run_cf64) (const [**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* plan, const double complex \* in, double complex \* out) <br> |
|  void | [**dp\_fft2d\_run\_inplace\_cf32**](#function-dp_fft2d_run_inplace_cf32) (const [**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* plan, float complex \* data) <br> |
|  void | [**dp\_fft2d\_run\_inplace\_cf64**](#function-dp_fft2d_run_inplace_cf64) (const [**dp\_fft2d\_t**](fft_8h.md#typedef-dp_fft2d_t) \* plan, double complex \* data) <br> |
|  [**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* | [**dp\_fft\_create**](#function-dp_fft_create) (size\_t n, int sign, int nthreads) <br>_Create a 1-D FFT plan._  |
|  void | [**dp\_fft\_destroy**](#function-dp_fft_destroy) ([**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* plan) <br>_Destroy a 1-D plan and free all memory._  |
|  void | [**dp\_fft\_global\_setup**](#function-dp_fft_global_setup) (const size\_t \* shape, size\_t ndim, int sign, int nthreads, const char \* planner, const char \* wisdom\_path) <br>_Set up the global FFT plan for a given shape._  |
|  void | [**dp\_fft\_run\_cf32**](#function-dp_fft_run_cf32) (const [**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* plan, const float complex \* in, float complex \* out) <br> |
|  void | [**dp\_fft\_run\_cf64**](#function-dp_fft_run_cf64) (const [**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* plan, const double complex \* in, double complex \* out) <br> |
|  void | [**dp\_fft\_run\_inplace\_cf32**](#function-dp_fft_run_inplace_cf32) (const [**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* plan, float complex \* data) <br> |
|  void | [**dp\_fft\_run\_inplace\_cf64**](#function-dp_fft_run_inplace_cf64) (const [**dp\_fft\_t**](fft_8h.md#typedef-dp_fft_t) \* plan, double complex \* data) <br> |




























## Detailed Description


## Public Types Documentation




### typedef dp\_fft2d\_t

_Opaque per-instance 2-D FFT plan._
```C++
typedef struct dp_fft2d_s dp_fft2d_t;
```




<hr>



### typedef dp\_fft\_t

_Opaque per-instance 1-D FFT plan._
```C++
typedef struct dp_fft_s dp_fft_t;
```




<hr>
## Public Functions Documentation




### function dp\_fft1d\_execute

_Execute an out-of-place 1-D FFT._
```C++
void dp_fft1d_execute (
    const double complex * input,
    double complex * output
)
```



Uses the plan established by the most recent [**dp\_fft\_global\_setup()**](fft_8h.md#function-dp_fft_global_setup) call with ndim=1. `input` and `output` must each be at least `shape[0]` elements.




**Parameters:**


* `input` Read-only input array of complex doubles.
* `output` Output array (must not alias `input`).






<hr>



### function dp\_fft1d\_execute\_cf32

_Execute an out-of-place 1-D FFT on single-precision complex data._
```C++
void dp_fft1d_execute_cf32 (
    const float complex * input,
    float complex * output
)
```





**Parameters:**


* `input` Read-only input array of `shape[0]` float complex elements.
* `output` Output array (must not alias `input`).






<hr>



### function dp\_fft1d\_execute\_inplace

_Execute an in-place 1-D FFT._
```C++
void dp_fft1d_execute_inplace (
    double complex * data
)
```





**Parameters:**


* `data` Input/output array of `shape[0]` complex doubles.






<hr>



### function dp\_fft1d\_execute\_inplace\_cf32

_Execute an in-place 1-D FFT on single-precision complex data._
```C++
void dp_fft1d_execute_inplace_cf32 (
    float complex * data
)
```





**Parameters:**


* `data` Input/output array of `shape[0]` float complex elements.






<hr>



### function dp\_fft2d\_create

_Create a 2-D FFT plan._
```C++
dp_fft2d_t * dp_fft2d_create (
    size_t ny,
    size_t nx,
    int sign,
    int nthreads
)
```





**Parameters:**


* `ny` Row count (outer dimension).
* `nx` Column count (inner dimension).
* `sign` -1 forward, +1 inverse.
* `nthreads` Thread count (FFTW only).



**Returns:**

Heap-allocated plan, or NULL on failure.







<hr>



### function dp\_fft2d\_destroy

_Destroy a 2-D plan and free all memory._
```C++
void dp_fft2d_destroy (
    dp_fft2d_t * plan
)
```





**Parameters:**


* `plan` May be NULL (no-op).






<hr>



### function dp\_fft2d\_execute

_Execute an out-of-place 2-D FFT._
```C++
void dp_fft2d_execute (
    const double complex * input,
    double complex * output
)
```



Uses the plan established by the most recent [**dp\_fft\_global\_setup()**](fft_8h.md#function-dp_fft_global_setup) call with ndim=2. Arrays must each hold `shape[0] * shape[1]` elements in row-major order.




**Parameters:**


* `input` Read-only input array.
* `output` Output array (must not alias `input`).






<hr>



### function dp\_fft2d\_execute\_cf32

_Execute an out-of-place 2-D FFT on single-precision complex data._
```C++
void dp_fft2d_execute_cf32 (
    const float complex * input,
    float complex * output
)
```





**Parameters:**


* `input` Read-only input array of `shape[0] * shape[1]` elements.
* `output` Output array (must not alias `input`).






<hr>



### function dp\_fft2d\_execute\_inplace

_Execute an in-place 2-D FFT._
```C++
void dp_fft2d_execute_inplace (
    double complex * data
)
```





**Parameters:**


* `data` Input/output array of `shape[0] * shape[1]` complex doubles.






<hr>



### function dp\_fft2d\_execute\_inplace\_cf32

_Execute an in-place 2-D FFT on single-precision complex data._
```C++
void dp_fft2d_execute_inplace_cf32 (
    float complex * data
)
```





**Parameters:**


* `data` Input/output array of `shape[0] * shape[1]` float complex elements.






<hr>



### function dp\_fft2d\_run\_cf32

```C++
void dp_fft2d_run_cf32 (
    const dp_fft2d_t * plan,
    const float complex * in,
    float complex * out
)
```




<hr>



### function dp\_fft2d\_run\_cf64

```C++
void dp_fft2d_run_cf64 (
    const dp_fft2d_t * plan,
    const double complex * in,
    double complex * out
)
```




<hr>



### function dp\_fft2d\_run\_inplace\_cf32

```C++
void dp_fft2d_run_inplace_cf32 (
    const dp_fft2d_t * plan,
    float complex * data
)
```




<hr>



### function dp\_fft2d\_run\_inplace\_cf64

```C++
void dp_fft2d_run_inplace_cf64 (
    const dp_fft2d_t * plan,
    double complex * data
)
```




<hr>



### function dp\_fft\_create

_Create a 1-D FFT plan._
```C++
dp_fft_t * dp_fft_create (
    size_t n,
    int sign,
    int nthreads
)
```





**Parameters:**


* `n` Transform length in samples.
* `sign` -1 for forward (DFT), +1 for inverse (IDFT).
* `nthreads` Thread count (FFTW only; PocketFFT is always single-threaded per call but fully re-entrant).



**Returns:**

Heap-allocated plan, or NULL on failure.







<hr>



### function dp\_fft\_destroy

_Destroy a 1-D plan and free all memory._
```C++
void dp_fft_destroy (
    dp_fft_t * plan
)
```





**Parameters:**


* `plan` May be NULL (no-op).






<hr>



### function dp\_fft\_global\_setup

_Set up the global FFT plan for a given shape._
```C++
void dp_fft_global_setup (
    const size_t * shape,
    size_t ndim,
    int sign,
    int nthreads,
    const char * planner,
    const char * wisdom_path
)
```



Plans are cached by shape and parameters; subsequent calls with the same shape return immediately. When FFTW is the backend, `planner` and `wisdom_path` control FFTW plan creation.




**Parameters:**


* `shape` Array of `ndim` dimension sizes (e.g. `{1024}` for 1-D).
* `ndim` Number of dimensions: 1 or 2.
* `sign` Transform direction: -1 for forward FFT, +1 for inverse.
* `nthreads` Number of threads (FFTW only; ignored by PocketFFT).
* `planner` Planner effort: `"estimate"`, `"measure"`, `"patient"`, or `"exhaustive"` (FFTW only).
* `wisdom_path` Path to load/save FFTW wisdom, or NULL to skip.



**Note:**

**IMPORTANT (FFTW only)**: Heavier planners (`"measure"`, `"patient"`, `"exhaustive"`) will **ERASE** the contents of the input and output arrays during the `execution` phase of the _first_ transform (when the plan is created). If using these planners, ensure you run the first transform with dummy data or call setup _before_ filling your buffers. The `"estimate"` planner (default) is safe to use on live data.







<hr>



### function dp\_fft\_run\_cf32

```C++
void dp_fft_run_cf32 (
    const dp_fft_t * plan,
    const float complex * in,
    float complex * out
)
```




<hr>



### function dp\_fft\_run\_cf64

```C++
void dp_fft_run_cf64 (
    const dp_fft_t * plan,
    const double complex * in,
    double complex * out
)
```




<hr>



### function dp\_fft\_run\_inplace\_cf32

```C++
void dp_fft_run_inplace_cf32 (
    const dp_fft_t * plan,
    float complex * data
)
```




<hr>



### function dp\_fft\_run\_inplace\_cf64

```C++
void dp_fft_run_inplace_cf64 (
    const dp_fft_t * plan,
    double complex * data
)
```




<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/fft.h`
