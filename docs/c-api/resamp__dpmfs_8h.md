

# File resamp\_dpmfs.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**resamp\_dpmfs.h**](resamp__dpmfs_8h.md)

[Go to the source code of this file](resamp__dpmfs_8h_source.md)

_DPMFS polyphase resampler for cf32 IQ samples._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_resamp\_dpmfs | [**dp\_resamp\_dpmfs\_t**](#typedef-dp_resamp_dpmfs_t)  <br>_Opaque DPMFS resampler state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* | [**dp\_resamp\_dpmfs\_create**](#function-dp_resamp_dpmfs_create) (size\_t M, size\_t N, const float \* c0, const float \* c1, double rate) <br>_Create a DPMFS polyphase resampler._  |
|  void | [**dp\_resamp\_dpmfs\_destroy**](#function-dp_resamp_dpmfs_destroy) ([**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Free a DPMFS resampler._  |
|  size\_t | [**dp\_resamp\_dpmfs\_execute**](#function-dp_resamp_dpmfs_execute) ([**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Resample a block of cf32 IQ samples._  |
|  size\_t | [**dp\_resamp\_dpmfs\_num\_taps**](#function-dp_resamp_dpmfs_num_taps) (const [**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Return the taps per phase (N)._  |
|  size\_t | [**dp\_resamp\_dpmfs\_poly\_order**](#function-dp_resamp_dpmfs_poly_order) (const [**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Return the polynomial order (M)._  |
|  double | [**dp\_resamp\_dpmfs\_rate**](#function-dp_resamp_dpmfs_rate) (const [**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Return the rate (fs\_out / fs\_in)._  |
|  void | [**dp\_resamp\_dpmfs\_reset**](#function-dp_resamp_dpmfs_reset) ([**dp\_resamp\_dpmfs\_t**](resamp__dpmfs_8h.md#typedef-dp_resamp_dpmfs_t) \* r) <br>_Zero the sample history and reset the phase accumulator._  |




























## Detailed Description


## Public Types Documentation




### typedef dp\_resamp\_dpmfs\_t

_Opaque DPMFS resampler state._
```C++
typedef struct dp_resamp_dpmfs dp_resamp_dpmfs_t;
```




<hr>
## Public Functions Documentation




### function dp\_resamp\_dpmfs\_create

_Create a DPMFS polyphase resampler._
```C++
dp_resamp_dpmfs_t * dp_resamp_dpmfs_create (
    size_t M,
    size_t N,
    const float * c0,
    const float * c1,
    double rate
)
```



The coefficient arrays are copied internally; the caller may free them immediately after this call returns.


M must be in [1, 3].




**Parameters:**


* `M` Polynomial order (typically 3).
* `N` Taps per phase.
* `c0` (M+1)\*N float32 coefficients for j=0, row-major [m][k].
* `c1` (M+1)\*N float32 coefficients for j=1, row-major [m][k].
* `rate` fs\_out / fs\_in. Must be &gt; 0.



**Returns:**

Heap-allocated resampler, or NULL on failure.







<hr>



### function dp\_resamp\_dpmfs\_destroy

_Free a DPMFS resampler._
```C++
void dp_resamp_dpmfs_destroy (
    dp_resamp_dpmfs_t * r
)
```





**Parameters:**


* `r` May be NULL (no-op).






<hr>



### function dp\_resamp\_dpmfs\_execute

_Resample a block of cf32 IQ samples._
```C++
size_t dp_resamp_dpmfs_execute (
    dp_resamp_dpmfs_t * r,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```



Processes `num_in` input samples and writes at most `max_out` output samples. Internal state is preserved across calls.




**Parameters:**


* `r` Must be non-NULL.
* `in` Input sample array (may be NULL if num\_in == 0).
* `num_in` Number of input samples.
* `out` Output sample buffer (must hold &gt;= max\_out samples).
* `max_out` Capacity of `out` in samples.



**Returns:**

Number of output samples written.







<hr>



### function dp\_resamp\_dpmfs\_num\_taps

_Return the taps per phase (N)._
```C++
size_t dp_resamp_dpmfs_num_taps (
    const dp_resamp_dpmfs_t * r
)
```




<hr>



### function dp\_resamp\_dpmfs\_poly\_order

_Return the polynomial order (M)._
```C++
size_t dp_resamp_dpmfs_poly_order (
    const dp_resamp_dpmfs_t * r
)
```




<hr>



### function dp\_resamp\_dpmfs\_rate

_Return the rate (fs\_out / fs\_in)._
```C++
double dp_resamp_dpmfs_rate (
    const dp_resamp_dpmfs_t * r
)
```




<hr>



### function dp\_resamp\_dpmfs\_reset

_Zero the sample history and reset the phase accumulator._
```C++
void dp_resamp_dpmfs_reset (
    dp_resamp_dpmfs_t * r
)
```





**Parameters:**


* `r` Must be non-NULL.






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/resamp_dpmfs.h`
