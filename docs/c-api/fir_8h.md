

# File fir.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**fir.h**](fir_8h.md)

[Go to the source code of this file](fir_8h_source.md)

_FIR filter with SIMD-accelerated hot loops (real and complex taps)._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_fir | [**dp\_fir\_t**](#typedef-dp_fir_t)  <br>_Opaque FIR filter state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* | [**dp\_fir\_create**](#function-dp_fir_create) (const float \_Complex \* taps, size\_t num\_taps) <br>_Create a complex FIR filter from CF32 tap coefficients._  |
|  [**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* | [**dp\_fir\_create\_real**](#function-dp_fir_create_real) (const float \* taps, size\_t num\_taps) <br>_Create a real-coefficient FIR filter for complex (IQ) signals._  |
|  void | [**dp\_fir\_destroy**](#function-dp_fir_destroy) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f) <br>_Destroy the filter and release all associated memory._  |
|  int | [**dp\_fir\_execute\_cf32**](#function-dp_fir_execute_cf32) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const float \_Complex \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter an array of CF32 complex samples._  |
|  int | [**dp\_fir\_execute\_ci16**](#function-dp_fir_execute_ci16) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int16\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI16 IQ samples, writing CF32 output._  |
|  int | [**dp\_fir\_execute\_ci32**](#function-dp_fir_execute_ci32) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int32\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI32 IQ samples, writing CF32 output._  |
|  int | [**dp\_fir\_execute\_ci8**](#function-dp_fir_execute_ci8) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int8\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI8 IQ samples, writing CF32 output._  |
|  int | [**dp\_fir\_execute\_real\_cf32**](#function-dp_fir_execute_real_cf32) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const float \_Complex \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CF32 IQ samples through a real-tap filter._  |
|  int | [**dp\_fir\_execute\_real\_ci16**](#function-dp_fir_execute_real_ci16) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int16\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI16 IQ samples through a real-tap filter, output CF32._  |
|  int | [**dp\_fir\_execute\_real\_ci32**](#function-dp_fir_execute_real_ci32) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int32\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI32 IQ samples through a real-tap filter, output CF32._  |
|  int | [**dp\_fir\_execute\_real\_ci8**](#function-dp_fir_execute_real_ci8) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f, const int8\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI8 IQ samples through a real-tap filter, output CF32._  |
|  void | [**dp\_fir\_reset**](#function-dp_fir_reset) ([**dp\_fir\_t**](fir_8h.md#typedef-dp_fir_t) \* f) <br>_Zero the delay line without freeing the filter._  |




























## Detailed Description


Implements direct-form FIR filtering for IQ signal streams. Two filter variants share the same lifecycle (reset/destroy) API:


[**dp\_fir\_create()**](fir_8h.md#function-dp_fir_create) — complex CF32 taps (general case) [**dp\_fir\_create\_real()**](fir_8h.md#function-dp_fir_create_real) — real float taps (DDC/DUC common case)


Real-tap filters are the norm in DDC/DUC applications: CIC compensation, root-raised-cosine, channel-select LPF. Using real taps costs 1 FMA per tap instead of 2 FMA + permute + mul, cutting the multiply count in half.


Computation precision is CF32 throughout. Integer inputs (CI8, CI16, CI32) are upcasted to CF32 in the hot loop via AVX-512 integer converts.


SIMD dispatch (compile-time):
* Complex taps: AVX-512F+DQ — 8 complex outputs/iteration
* Real taps: AVX-512F — 8 complex outputs/iteration
* Scalar fallback always available





```C++
#include <dp/fir.h>

// DDC low-pass: real taps, CI16 input (LimeSDR/USRP)
float taps[63] = { ... };   // designed with scipy.signal.firwin
dp_fir_t *fir = dp_fir_create_real(taps, 63);

int16_t raw[2 * 4096];           // 2 int16_t per complex sample
float _Complex out[4096];
dp_fir_execute_real_ci16(fir, raw, out, 4096);

// Complex taps (e.g. frequency-shifted filter, Hilbert transformer)
float _Complex ctaps[63] = { ... };
dp_fir_t *cfir = dp_fir_create(ctaps, 63);
dp_fir_execute_cf32(cfir, out, out, 4096);

dp_fir_destroy(fir);
dp_fir_destroy(cfir);
```




## Public Types Documentation




### typedef dp\_fir\_t

_Opaque FIR filter state._
```C++
typedef struct dp_fir dp_fir_t;
```




<hr>
## Public Functions Documentation




### function dp\_fir\_create

_Create a complex FIR filter from CF32 tap coefficients._
```C++
dp_fir_t * dp_fir_create (
    const float _Complex * taps,
    size_t num_taps
)
```



The filter allocates an internal delay line of `num_taps` − 1 complex samples, initialised to zero. A scratch buffer is allocated lazily on the first call to an execute function and reused on subsequent calls.




**Parameters:**


* `taps` Pointer to `num_taps` complex tap coefficients.
* `num_taps` Number of taps (filter length ≥ 1).



**Returns:**

Heap-allocated filter state, or NULL on failure.







<hr>



### function dp\_fir\_create\_real

_Create a real-coefficient FIR filter for complex (IQ) signals._
```C++
dp_fir_t * dp_fir_create_real (
    const float * taps,
    size_t num_taps
)
```



Real taps are the common case in DDC/DUC: the filter is designed in the real domain (e.g. scipy.signal.firwin) and applied to complex IQ samples. Using real taps costs 1 FMA per tap instead of 2 FMA + permute + mul, halving the multiply count vs [**dp\_fir\_create()**](fir_8h.md#function-dp_fir_create) with zero-imaginary coefficients.


Use dp\_fir\_execute\_real\_\*() to run the filter. [**dp\_fir\_reset()**](fir_8h.md#function-dp_fir_reset) and [**dp\_fir\_destroy()**](fir_8h.md#function-dp_fir_destroy) work identically for both real and complex filters.




**Parameters:**


* `taps` Pointer to `num_taps` real-valued tap coefficients.
* `num_taps` Number of taps (filter length ≥ 1).



**Returns:**

Heap-allocated filter state, or NULL on failure.







<hr>



### function dp\_fir\_destroy

_Destroy the filter and release all associated memory._
```C++
void dp_fir_destroy (
    dp_fir_t * f
)
```





**Parameters:**


* `f` Filter state (may be NULL).






<hr>



### function dp\_fir\_execute\_cf32

_Filter an array of CF32 complex samples._
```C++
int dp_fir_execute_cf32 (
    dp_fir_t * f,
    const float _Complex * in,
    float _Complex * out,
    size_t num_samples
)
```



Dispatches to the widest available SIMD path at compile time (AVX-512 → scalar). Processes 8 output samples per inner iteration on AVX-512-capable hardware.




**Parameters:**


* `f` Filter state.
* `in` Input array of `num_samples` CF32 samples.
* `out` Output array (may alias `in` for in-place use).
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_ci16

_Filter CI16 IQ samples, writing CF32 output._
```C++
int dp_fir_execute_ci16 (
    dp_fir_t * f,
    const int16_t * in,
    float _Complex * out,
    size_t num_samples
)
```



Upcasts int16\_t I/Q to float on the fly before filtering. Designed for LimeSDR, USRP, and PlutoSDR (4 bytes/sample → 16 AVX-512 lanes per register before conversion).




**Parameters:**


* `f` Filter state.
* `in` Interleaved int16\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_ci32

_Filter CI32 IQ samples, writing CF32 output._
```C++
int dp_fir_execute_ci32 (
    dp_fir_t * f,
    const int32_t * in,
    float _Complex * out,
    size_t num_samples
)
```



Upcasts int32\_t I/Q to float on the fly before filtering.




**Parameters:**


* `f` Filter state.
* `in` Interleaved int32\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_ci8

_Filter CI8 IQ samples, writing CF32 output._
```C++
int dp_fir_execute_ci8 (
    dp_fir_t * f,
    const int8_t * in,
    float _Complex * out,
    size_t num_samples
)
```



Upcasts int8\_t I/Q to float on the fly before filtering. Designed for RTL-SDR and HackRF (2 bytes/sample → 32 AVX-512 lanes per register before conversion).




**Parameters:**


* `f` Filter state.
* `in` Interleaved int8\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_real\_cf32

_Filter CF32 IQ samples through a real-tap filter._
```C++
int dp_fir_execute_real_cf32 (
    dp_fir_t * f,
    const float _Complex * in,
    float _Complex * out,
    size_t num_samples
)
```





**Parameters:**


* `f` Real-tap filter (from dp\_fir\_create\_real).
* `in` Input array of `num_samples` float \_Complex samples.
* `out` Output array (may alias `in` for in-place use).
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_real\_ci16

_Filter CI16 IQ samples through a real-tap filter, output CF32._
```C++
int dp_fir_execute_real_ci16 (
    dp_fir_t * f,
    const int16_t * in,
    float _Complex * out,
    size_t num_samples
)
```





**Parameters:**


* `f` Real-tap filter (from dp\_fir\_create\_real).
* `in` Interleaved int16\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_real\_ci32

_Filter CI32 IQ samples through a real-tap filter, output CF32._
```C++
int dp_fir_execute_real_ci32 (
    dp_fir_t * f,
    const int32_t * in,
    float _Complex * out,
    size_t num_samples
)
```





**Parameters:**


* `f` Real-tap filter (from dp\_fir\_create\_real).
* `in` Interleaved int32\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_execute\_real\_ci8

_Filter CI8 IQ samples through a real-tap filter, output CF32._
```C++
int dp_fir_execute_real_ci8 (
    dp_fir_t * f,
    const int8_t * in,
    float _Complex * out,
    size_t num_samples
)
```





**Parameters:**


* `f` Real-tap filter (from dp\_fir\_create\_real).
* `in` Interleaved int8\_t I/Q; length 2×num\_samples.
* `out` Output array of float \_Complex samples.
* `num_samples` Number of complex samples to process.



**Returns:**

DP\_OK on success, DP\_ERR\_MEMORY on alloc fail.







<hr>



### function dp\_fir\_reset

_Zero the delay line without freeing the filter._
```C++
void dp_fir_reset (
    dp_fir_t * f
)
```



Use after a stream discontinuity to prevent history contamination.




**Parameters:**


* `f` Filter state (must be non-NULL).






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/fir.h`
