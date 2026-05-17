

# File ddc.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**ddc.h**](ddc_8h.md)

[Go to the source code of this file](ddc_8h_source.md)

_Digital Down-Converter (DDC)._ [More...](#detailed-description)

* `#include <dp/resamp_dpmfs.h>`
* `#include <dp/stream.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_ddc\_real | [**dp\_ddc\_real\_t**](#typedef-dp_ddc_real_t)  <br>_Opaque Architecture D2 DDC state._  |
| typedef struct dp\_ddc | [**dp\_ddc\_t**](#typedef-dp_ddc_t)  <br>_Opaque DDC state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* | [**dp\_ddc\_create**](#function-dp_ddc_create) (float norm\_freq, size\_t num\_in, double rate) <br>_Create a DDC using built-in filter coefficients._  |
|  [**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* | [**dp\_ddc\_create\_custom**](#function-dp_ddc_create_custom) (float norm\_freq, size\_t num\_in, [**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Create a DDC with a caller-supplied resampler._  |
|  void | [**dp\_ddc\_destroy**](#function-dp_ddc_destroy) ([**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc) <br>_Destroy the DDC and release all resources._  |
|  size\_t | [**dp\_ddc\_execute**](#function-dp_ddc_execute) ([**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Mix and resample a block of CF32 IQ samples._  |
|  float | [**dp\_ddc\_get\_freq**](#function-dp_ddc_get_freq) (const [**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc) <br>_Return the current NCO normalised frequency._  |
|  size\_t | [**dp\_ddc\_max\_out**](#function-dp_ddc_max_out) (const [**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc) <br>_Return the maximum output samples per dp\_ddc\_execute call._  |
|  size\_t | [**dp\_ddc\_nout**](#function-dp_ddc_nout) (const [**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc) <br>_Return the actual output sample count from the last dp\_ddc\_execute call._  |
|  [**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* | [**dp\_ddc\_real\_create**](#function-dp_ddc_real_create) (float norm\_freq, size\_t num\_in, double rate) <br>_Create a D2 DDC using built-in filter coefficients._  |
|  void | [**dp\_ddc\_real\_destroy**](#function-dp_ddc_real_destroy) ([**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc) <br>_Destroy the D2 DDC and free all resources._  |
|  size\_t | [**dp\_ddc\_real\_execute**](#function-dp_ddc_real_execute) ([**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc, const float \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Process a block of real float32 samples._  |
|  float | [**dp\_ddc\_real\_get\_freq**](#function-dp_ddc_real_get_freq) (const [**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc) <br>_Return the current normalised carrier frequency._  |
|  size\_t | [**dp\_ddc\_real\_max\_out**](#function-dp_ddc_real_max_out) (const [**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc) <br>_Return the maximum CF32 output samples per execute call._  |
|  size\_t | [**dp\_ddc\_real\_nout**](#function-dp_ddc_real_nout) (const [**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc) <br>_Return the output sample count from the last execute call._  |
|  void | [**dp\_ddc\_real\_reset**](#function-dp_ddc_real_reset) ([**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc) <br>_Reset halfband, NCO, and resampler history to zero._  |
|  void | [**dp\_ddc\_real\_set\_freq**](#function-dp_ddc_real_set_freq) ([**dp\_ddc\_real\_t**](ddc_8h.md#typedef-dp_ddc_real_t) \* ddc, float norm\_freq) <br>_Change the carrier frequency without resetting state._  |
|  void | [**dp\_ddc\_reset**](#function-dp_ddc_reset) ([**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc) <br>_Reset NCO phase and resampler history to zero._  |
|  void | [**dp\_ddc\_set\_freq**](#function-dp_ddc_set_freq) ([**dp\_ddc\_t**](ddc_8h.md#typedef-dp_ddc_t) \* ddc, float norm\_freq) <br>_Change the NCO tune frequency without resetting phase._  |




























## Detailed Description


## Public Types Documentation




### typedef dp\_ddc\_real\_t

_Opaque Architecture D2 DDC state._
```C++
typedef struct dp_ddc_real dp_ddc_real_t;
```




<hr>



### typedef dp\_ddc\_t

_Opaque DDC state._
```C++
typedef struct dp_ddc dp_ddc_t;
```




<hr>
## Public Functions Documentation




### function dp\_ddc\_create

_Create a DDC using built-in filter coefficients._
```C++
dp_ddc_t * dp_ddc_create (
    float norm_freq,
    size_t num_in,
    double rate
)
```



Uses a built-in M=3, N=19 Kaiser–DPMFS lowpass filter with frequency response normalised to the **output** sample rate fs\_out (where 1.0 = fs\_out, 0.5 = Nyquist\_out):


passband ≤ 0.4 × fs\_out flat response stopband ≥ 0.6 × fs\_out ≥ 60 dB rejection


The alias of the stopband edge folds back to 1.0 − 0.6 = 0.4 × fs\_out, landing exactly at the passband edge — the widest possible transition band that keeps aliases out of the passband.


Because the cutoffs scale with fs\_out, the same coefficient bank works for any decimation rate. In input-rate units:


passband edge = 0.4 × rate × fs\_in stopband edge = 0.6 × rate × fs\_in


This matches the doppler spectrum analyser's 80 % bandwidth convention (fs\_out = span / 0.8).


Passing `rate` = 1.0 (or any value within 1×10⁻⁶ of 1.0) bypasses the resampler; output equals the mixed signal at the input rate.




**Parameters:**


* `norm_freq` NCO normalised frequency f/fs (cycles per sample). Negative values shift a positive-offset signal to DC. Values outside [−0.5, 0.5) are folded.
* `num_in` Fixed input block size in samples. All subsequent calls to dp\_ddc\_execute must pass this exact count.
* `rate` fs\_out / fs\_in. Must be &gt; 0.



**Returns:**

Heap-allocated DDC state, or NULL on failure.







<hr>



### function dp\_ddc\_create\_custom

_Create a DDC with a caller-supplied resampler._
```C++
dp_ddc_t * dp_ddc_create_custom (
    float norm_freq,
    size_t num_in,
    dp_resamp_dpmfs_t * r
)
```



Use when the built-in coefficients are not sufficient — for example, when a rate-matched design from `doppler.polyphase.optimize_dpmfs` is required.


The DDC takes **ownership** of `r`. The caller must not use or destroy `r` after this call. On failure, `r` is destroyed so the caller does not leak it.


Passing `r` = NULL creates a DDC without a resampler (passthrough at the input rate, same as dp\_ddc\_create with rate = 1.0).




**Parameters:**


* `norm_freq` NCO normalised frequency (see dp\_ddc\_create).
* `num_in` Fixed input block size in samples.
* `r` Pre-built DPMFS resampler, or NULL.



**Returns:**

Heap-allocated DDC state, or NULL on failure.







<hr>



### function dp\_ddc\_destroy

_Destroy the DDC and release all resources._
```C++
void dp_ddc_destroy (
    dp_ddc_t * ddc
)
```



Also destroys the resampler that was passed to dp\_ddc\_create or dp\_ddc\_create\_custom.




**Parameters:**


* `ddc` May be NULL (no-op).






<hr>



### function dp\_ddc\_execute

_Mix and resample a block of CF32 IQ samples._
```C++
size_t dp_ddc_execute (
    dp_ddc_t * ddc,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```



Each input sample is multiplied by the NCO phasor, then the mixed block is fed through the resampler (if configured) into `out`. NCO phase and resampler history are preserved across calls.


`num_in` should equal the value passed to dp\_ddc\_create / dp\_ddc\_create\_custom. Passing a smaller value is safe but wastes the pre-allocated buffer capacity.


Pass `dp_ddc_max_out(ddc)` as `max_out` to guarantee all output samples are captured.




**Parameters:**


* `ddc` Must be non-NULL.
* `in` Input samples, CF32, length ≥ `num_in`.
* `num_in` Number of input samples to process.
* `out` Output buffer, CF32, capacity ≥ `max_out`.
* `max_out` Maximum output samples to write.



**Returns:**

Number of output samples written (0 if num\_in == 0).







<hr>



### function dp\_ddc\_get\_freq

_Return the current NCO normalised frequency._
```C++
float dp_ddc_get_freq (
    const dp_ddc_t * ddc
)
```





**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_max\_out

_Return the maximum output samples per dp\_ddc\_execute call._
```C++
size_t dp_ddc_max_out (
    const dp_ddc_t * ddc
)
```



Allocate at least this many CF32 samples for the `out` buffer passed to dp\_ddc\_execute. The value is fixed at creation time.


Without a resampler: equals `num_in`. With a resampler at rate `r:` equals ⌈num\_in × r⌉ + 4.




**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_nout

_Return the actual output sample count from the last dp\_ddc\_execute call._
```C++
size_t dp_ddc_nout (
    const dp_ddc_t * ddc
)
```



Equals the value returned by dp\_ddc\_execute. With a resampler this can vary by ±1 from call to call (phase accumulator rounding); it is always ≤ [**dp\_ddc\_max\_out()**](ddc_8h.md#function-dp_ddc_max_out).


Zero before the first dp\_ddc\_execute call.




**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_real\_create

_Create a D2 DDC using built-in filter coefficients._
```C++
dp_ddc_real_t * dp_ddc_real_create (
    float norm_freq,
    size_t num_in,
    double rate
)
```



Uses a built-in halfband (60 dB, N=19 taps) and the same M=3 N=19 Kaiser-DPMFS lowpass used by dp\_ddc\_create.




**Parameters:**


* `norm_freq` NCO normalised frequency (see dp\_ddc\_create).
* `num_in` Fixed real input block size in samples.
* `rate` fs\_out / fs\_in. Must be &gt; 0 and &lt; 0.5.



**Returns:**

Heap-allocated DDC state, or NULL on failure.







<hr>



### function dp\_ddc\_real\_destroy

_Destroy the D2 DDC and free all resources._
```C++
void dp_ddc_real_destroy (
    dp_ddc_real_t * ddc
)
```





**Parameters:**


* `ddc` May be NULL (no-op).






<hr>



### function dp\_ddc\_real\_execute

_Process a block of real float32 samples._
```C++
size_t dp_ddc_real_execute (
    dp_ddc_real_t * ddc,
    const float * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```





**Parameters:**


* `ddc` Must be non-NULL.
* `in` Real input samples, float32, length &gt;= num\_in.
* `num_in` Number of input samples to process.
* `out` CF32 output buffer, capacity &gt;= max\_out.
* `max_out` Maximum output samples to write.



**Returns:**

Number of CF32 output samples written.







<hr>



### function dp\_ddc\_real\_get\_freq

_Return the current normalised carrier frequency._
```C++
float dp_ddc_real_get_freq (
    const dp_ddc_real_t * ddc
)
```




<hr>



### function dp\_ddc\_real\_max\_out

_Return the maximum CF32 output samples per execute call._
```C++
size_t dp_ddc_real_max_out (
    const dp_ddc_real_t * ddc
)
```





**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_real\_nout

_Return the output sample count from the last execute call._
```C++
size_t dp_ddc_real_nout (
    const dp_ddc_real_t * ddc
)
```





**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_real\_reset

_Reset halfband, NCO, and resampler history to zero._
```C++
void dp_ddc_real_reset (
    dp_ddc_real_t * ddc
)
```





**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_real\_set\_freq

_Change the carrier frequency without resetting state._
```C++
void dp_ddc_real_set_freq (
    dp_ddc_real_t * ddc,
    float norm_freq
)
```





**Parameters:**


* `ddc` Must be non-NULL.
* `norm_freq` New normalised frequency.






<hr>



### function dp\_ddc\_reset

_Reset NCO phase and resampler history to zero._
```C++
void dp_ddc_reset (
    dp_ddc_t * ddc
)
```



Use after a stream discontinuity to prevent stale state from contaminating the next block.




**Parameters:**


* `ddc` Must be non-NULL.






<hr>



### function dp\_ddc\_set\_freq

_Change the NCO tune frequency without resetting phase._
```C++
void dp_ddc_set_freq (
    dp_ddc_t * ddc,
    float norm_freq
)
```



Takes effect on the next dp\_ddc\_execute call. Does not disturb the resampler history, so retunes are seamless across block boundaries.




**Parameters:**


* `ddc` Must be non-NULL.
* `norm_freq` New normalised frequency (same convention as dp\_ddc\_create).






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/ddc.h`
