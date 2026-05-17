

# File resamp.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**resamp.h**](resamp_8h.md)

[Go to the source code of this file](resamp_8h_source.md)

_Continuously-variable polyphase resampler for cf32 IQ samples._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_resamp\_cf32 | [**dp\_resamp\_cf32\_t**](#typedef-dp_resamp_cf32_t)  <br>_Opaque polyphase resampler state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* | [**dp\_resamp\_cf32\_create**](#function-dp_resamp_cf32_create) (size\_t num\_phases, size\_t num\_taps, const float \* bank, double rate) <br>_Create a polyphase cf32 resampler._  |
|  void | [**dp\_resamp\_cf32\_destroy**](#function-dp_resamp_cf32_destroy) ([**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r) <br>_Free a resampler._  |
|  size\_t | [**dp\_resamp\_cf32\_execute**](#function-dp_resamp_cf32_execute) ([**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample a block of cf32 IQ samples._  |
|  size\_t | [**dp\_resamp\_cf32\_num\_phases**](#function-dp_resamp_cf32_num_phases) (const [**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r) <br>_Return the number of polyphase branches._  |
|  size\_t | [**dp\_resamp\_cf32\_num\_taps**](#function-dp_resamp_cf32_num_taps) (const [**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r) <br>_Return the taps per branch._  |
|  double | [**dp\_resamp\_cf32\_rate**](#function-dp_resamp_cf32_rate) (const [**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r) <br>_Return the rate (fs\_out / fs\_in) the resampler was created with._  |
|  void | [**dp\_resamp\_cf32\_reset**](#function-dp_resamp_cf32_reset) ([**dp\_resamp\_cf32\_t**](resamp_8h.md#typedef-dp_resamp_cf32_t) \* r) <br>_Zero the sample history and reset the phase accumulator._  |




























## Detailed Description


## Public Types Documentation




### typedef dp\_resamp\_cf32\_t

_Opaque polyphase resampler state._
```C++
typedef struct dp_resamp_cf32 dp_resamp_cf32_t;
```




<hr>
## Public Functions Documentation




### function dp\_resamp\_cf32\_create

_Create a polyphase cf32 resampler._
```C++
dp_resamp_cf32_t * dp_resamp_cf32_create (
    size_t num_phases,
    size_t num_taps,
    const float * bank,
    double rate
)
```



The coefficient bank is copied internally; the caller may free `bank` immediately after this call returns.


`num_phases` must be a power of two ≥ 1.




**Parameters:**


* `num_phases` Number of polyphase branches (power of two).
* `num_taps` Taps per branch (filter length ≥ 1).
* `bank` Flat row-major array [num\_phases][num\_taps], float32 coefficients.
* `rate` Output rate / input rate (fs\_out / fs\_in). Must be &gt; 0.



**Returns:**

Heap-allocated resampler, or NULL on failure.







<hr>



### function dp\_resamp\_cf32\_destroy

_Free a resampler._
```C++
void dp_resamp_cf32_destroy (
    dp_resamp_cf32_t * r
)
```





**Parameters:**


* `r` May be NULL (no-op).






<hr>



### function dp\_resamp\_cf32\_execute

_Resample a block of cf32 IQ samples._
```C++
size_t dp_resamp_cf32_execute (
    dp_resamp_cf32_t * r,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```



Processes `num_in` input samples and writes at most `max_out` output samples. Internal state (phase accumulator, delay history) is preserved across calls, so blocks may be any size.


**Interpolation** (rate ≥ 1): the function consumes all `num_in` inputs. Provide `max_out` ≥ ceil(num\_in × rate) + 2.


**Decimation** (rate &lt; 1): the function consumes all `num_in` inputs unless `max_out` is exhausted first. Provide `max_out` ≥ ceil(num\_in × rate) + 2.




**Parameters:**


* `r` Must be non-NULL.
* `in` Input sample array (may be NULL if num\_in == 0).
* `num_in` Number of input samples.
* `out` Output sample buffer (must hold ≥ max\_out samples).
* `max_out` Capacity of `out` in samples.



**Returns:**

Number of output samples written to `out`.







<hr>



### function dp\_resamp\_cf32\_num\_phases

_Return the number of polyphase branches._
```C++
size_t dp_resamp_cf32_num_phases (
    const dp_resamp_cf32_t * r
)
```




<hr>



### function dp\_resamp\_cf32\_num\_taps

_Return the taps per branch._
```C++
size_t dp_resamp_cf32_num_taps (
    const dp_resamp_cf32_t * r
)
```




<hr>



### function dp\_resamp\_cf32\_rate

_Return the rate (fs\_out / fs\_in) the resampler was created with._
```C++
double dp_resamp_cf32_rate (
    const dp_resamp_cf32_t * r
)
```




<hr>



### function dp\_resamp\_cf32\_reset

_Zero the sample history and reset the phase accumulator._
```C++
void dp_resamp_cf32_reset (
    dp_resamp_cf32_t * r
)
```



Use after a stream discontinuity.




**Parameters:**


* `r` Must be non-NULL.






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/resamp.h`
