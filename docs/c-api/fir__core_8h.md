

# File fir\_core.h



[**FileList**](files.md) **>** [**fir**](dir_37fd0118bf34c485dd22fe4d261d6eac.md) **>** [**fir\_core.h**](fir__core_8h.md)

[Go to the source code of this file](fir__core_8h_source.md)

_Direct-form FIR filter — real-tap and complex-tap variants._ [More...](#detailed-description)

* `#include "clib_common.h"`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct fir\_state | [**fir\_state\_t**](#typedef-fir_state_t)  <br>_Opaque FIR filter state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* | [**fir\_create**](#function-fir_create) (const float \_Complex \* taps, size\_t num\_taps) <br>_Create a FIR filter from complex CF32 tap coefficients._  |
|  [**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* | [**fir\_create\_real**](#function-fir_create_real) (const float \* taps, size\_t num\_taps) <br>_Create a FIR filter from real float tap coefficients._  |
|  void | [**fir\_destroy**](#function-fir_destroy) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f) <br>_Destroy the filter and release all associated memory._  |
|  int | [**fir\_execute\_cf32**](#function-fir_execute_cf32) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const float \_Complex \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CF32 samples through a complex-tap filter._  |
|  int | [**fir\_execute\_ci16**](#function-fir_execute_ci16) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int16\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI16 (interleaved int16\_t I/Q, len=2×n) → CF32._  |
|  int | [**fir\_execute\_ci32**](#function-fir_execute_ci32) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int32\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI32 (interleaved int32\_t I/Q, len=2×n) → CF32._  |
|  int | [**fir\_execute\_ci8**](#function-fir_execute_ci8) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int8\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI8 (interleaved int8\_t I/Q, len=2×n) → CF32._  |
|  int | [**fir\_execute\_real\_cf32**](#function-fir_execute_real_cf32) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const float \_Complex \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CF32 → CF32 through a real-tap filter. In-place safe._  |
|  int | [**fir\_execute\_real\_ci16**](#function-fir_execute_real_ci16) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int16\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI16 → CF32 through a real-tap filter._  |
|  int | [**fir\_execute\_real\_ci32**](#function-fir_execute_real_ci32) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int32\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI32 → CF32 through a real-tap filter._  |
|  int | [**fir\_execute\_real\_ci8**](#function-fir_execute_real_ci8) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f, const int8\_t \* in, float \_Complex \* out, size\_t num\_samples) <br>_Filter CI8 → CF32 through a real-tap filter._  |
|  int | [**fir\_is\_real**](#function-fir_is_real) (const [**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f) <br>_1 if filter was created with real taps, 0 if complex._  |
|  size\_t | [**fir\_num\_taps**](#function-fir_num_taps) (const [**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f) <br>_Number of tap coefficients._  |
|  void | [**fir\_reset**](#function-fir_reset) ([**fir\_state\_t**](fir__core_8h.md#typedef-fir_state_t) \* f) <br>_Zero the delay line without freeing the filter._  |




























## Detailed Description


Two constructors select the C backend at creation time:


[**fir\_create()**](fir__core_8h.md#function-fir_create) — complex CF32 taps (general case) [**fir\_create\_real()**](fir__core_8h.md#function-fir_create_real) — real float taps (DDC/DUC common case; 1 FMA/tap)


All execute functions accept CF32, CI8, CI16, or CI32 input and write CF32 output. Integer inputs are upcasted to CF32 inside the hot loop. The internal scratch buffer [delay \| input] is allocated lazily on the first execute call and reused on all subsequent calls.


Return value convention: 0 = success, -1 = allocation failure.



```C++
// DDC low-pass — real taps, CI16 input (LimeSDR / USRP)
float taps[63] = { ... };        // designed with scipy.signal.firwin
fir_state_t *fir = fir_create_real(taps, 63);

int16_t raw[2 * 4096];           // 2 int16_t per complex sample
float _Complex out[4096];
fir_execute_real_ci16(fir, raw, out, 4096);

// Complex taps (Hilbert transformer, frequency-shifted filter)
float _Complex ctaps[63] = { ... };
fir_state_t *cfir = fir_create(ctaps, 63);
fir_execute_cf32(cfir, out, out, 4096);  // in-place OK

fir_destroy(fir);
fir_destroy(cfir);
```




## Public Types Documentation




### typedef fir\_state\_t

_Opaque FIR filter state._
```C++
typedef struct fir_state fir_state_t;
```




<hr>
## Public Functions Documentation




### function fir\_create

_Create a FIR filter from complex CF32 tap coefficients._
```C++
fir_state_t * fir_create (
    const float _Complex * taps,
    size_t num_taps
)
```



Allocates an internal delay line of `num_taps` − 1 complex samples, initialised to zero. The scratch buffer is allocated lazily on the first execute call.




**Parameters:**


* `taps` Pointer to `num_taps` complex tap coefficients.
* `num_taps` Filter length (≥ 1).



**Returns:**

Heap-allocated state, or NULL on failure.







<hr>



### function fir\_create\_real

_Create a FIR filter from real float tap coefficients._
```C++
fir_state_t * fir_create_real (
    const float * taps,
    size_t num_taps
)
```



Real taps cost 1 FMA per tap instead of 2 FMA + permute + mul, halving the multiply count for filters designed in the real domain (e.g. scipy.signal.firwin). Use fir\_execute\_real\_\*() to run the filter; [**fir\_reset()**](fir__core_8h.md#function-fir_reset) and [**fir\_destroy()**](fir__core_8h.md#function-fir_destroy) work identically for both real-tap and complex-tap filters.




**Parameters:**


* `taps` Pointer to `num_taps` real-valued tap coefficients.
* `num_taps` Filter length (≥ 1).



**Returns:**

Heap-allocated state, or NULL on failure.







<hr>



### function fir\_destroy

_Destroy the filter and release all associated memory._
```C++
void fir_destroy (
    fir_state_t * f
)
```





**Parameters:**


* `f` Filter state (may be NULL — safe no-op).






<hr>



### function fir\_execute\_cf32

_Filter CF32 samples through a complex-tap filter._
```C++
int fir_execute_cf32 (
    fir_state_t * f,
    const float _Complex * in,
    float _Complex * out,
    size_t num_samples
)
```



Dispatches to AVX-512F+DQ (8 outputs/iter, 2 FMA + permute + mul per tap) or scalar fallback at compile time. In-place (out == in) is safe.




**Parameters:**


* `f` Complex-tap filter (from fir\_create).
* `in` Input array of `num_samples` float \_Complex.
* `out` Output array (may alias in).
* `num_samples` Number of complex samples to filter.



**Returns:**

0 on success, -1 on allocation failure.







<hr>



### function fir\_execute\_ci16

_Filter CI16 (interleaved int16\_t I/Q, len=2×n) → CF32._
```C++
int fir_execute_ci16 (
    fir_state_t * f,
    const int16_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_ci32

_Filter CI32 (interleaved int32\_t I/Q, len=2×n) → CF32._
```C++
int fir_execute_ci32 (
    fir_state_t * f,
    const int32_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_ci8

_Filter CI8 (interleaved int8\_t I/Q, len=2×n) → CF32._
```C++
int fir_execute_ci8 (
    fir_state_t * f,
    const int8_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_real\_cf32

_Filter CF32 → CF32 through a real-tap filter. In-place safe._
```C++
int fir_execute_real_cf32 (
    fir_state_t * f,
    const float _Complex * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_real\_ci16

_Filter CI16 → CF32 through a real-tap filter._
```C++
int fir_execute_real_ci16 (
    fir_state_t * f,
    const int16_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_real\_ci32

_Filter CI32 → CF32 through a real-tap filter._
```C++
int fir_execute_real_ci32 (
    fir_state_t * f,
    const int32_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_execute\_real\_ci8

_Filter CI8 → CF32 through a real-tap filter._
```C++
int fir_execute_real_ci8 (
    fir_state_t * f,
    const int8_t * in,
    float _Complex * out,
    size_t num_samples
)
```




<hr>



### function fir\_is\_real

_1 if filter was created with real taps, 0 if complex._
```C++
int fir_is_real (
    const fir_state_t * f
)
```




<hr>



### function fir\_num\_taps

_Number of tap coefficients._
```C++
size_t fir_num_taps (
    const fir_state_t * f
)
```




<hr>



### function fir\_reset

_Zero the delay line without freeing the filter._
```C++
void fir_reset (
    fir_state_t * f
)
```



Use after a stream discontinuity to prevent history contamination. Tap coefficients and scratch capacity are preserved.




**Parameters:**


* `f` Filter state (must be non-NULL).






<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fir/fir_core.h`
