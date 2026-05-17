

# File resamp\_dpmfs\_core.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp\_dpmfs**](dir_8ad30719b9735422ba744b401f0aead4.md) **>** [**resamp\_dpmfs\_core.h**](resamp__dpmfs__core_8h.md)

[Go to the source code of this file](resamp__dpmfs__core_8h_source.md)

_DPMFS polyphase resampler for cf32 IQ — self-contained core._ [More...](#detailed-description)

* `#include "clib_common.h"`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) <br>_Full DPMFS resampler state (not opaque — allocated by \_create)._  |






















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* | [**resamp\_dpmfs\_create**](#function-resamp_dpmfs_create) (size\_t poly\_order, size\_t n\_taps, const float \* c0, const float \* c1, double rate) <br>_Create a DPMFS polyphase resampler._  |
|  void | [**resamp\_dpmfs\_destroy**](#function-resamp_dpmfs_destroy) ([**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Free a DPMFS resampler._  |
|  size\_t | [**resamp\_dpmfs\_execute**](#function-resamp_dpmfs_execute) ([**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state, const float complex \* in, size\_t n\_in, float complex \* out) <br>_Resample a block of cf32 IQ samples._  |
|  size\_t | [**resamp\_dpmfs\_execute\_max\_out**](#function-resamp_dpmfs_execute_max_out) ([**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Maximum output samples per execute call._  |
|  size\_t | [**resamp\_dpmfs\_get\_num\_taps**](#function-resamp_dpmfs_get_num_taps) (const [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Return the taps per phase (N)._  |
|  size\_t | [**resamp\_dpmfs\_get\_poly\_order**](#function-resamp_dpmfs_get_poly_order) (const [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Return the polynomial order (M)._  |
|  double | [**resamp\_dpmfs\_get\_rate**](#function-resamp_dpmfs_get_rate) (const [**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Return the rate (fs\_out / fs\_in)._  |
|  void | [**resamp\_dpmfs\_reset**](#function-resamp_dpmfs_reset) ([**resamp\_dpmfs\_state\_t**](structresamp__dpmfs__state__t.md) \* state) <br>_Zero sample history and reset the phase accumulator._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**RESAMP\_DPMFS\_DEFAULT\_BLOCK**](resamp__dpmfs__core_8h.md#define-resamp_dpmfs_default_block)  `4096u`<br> |

## Detailed Description


The Dual Phase Modified Farrow Structure (DPMFS) replaces the large polyphase coefficient table with a compact polynomial bank (M+1)\*N\*2 float32 values. Both interpolation and decimation paths share the same 32-bit NCO.


The Python extension passes c0/c1 as 2-D arrays of shape (M+1, N), where M = poly\_order and N = taps per branch. The extension extracts M+1 = dim[0] and N = dim[1], then calls resamp\_dpmfs\_create(M, N, ...). Coefficient arrays are copied internally; the caller may free them after create returns.


execute\_max\_out() assumes a default input block of 4096 samples and returns ceil(4096 \* rate) + M + 8. Process in &lt;= 4096-sample chunks.


Lifecycle:
```C++
// c0, c1: (M+1)*N float arrays from doppler.polyphase.fit_dpmfs
resamp_dpmfs_state_t *r =
    resamp_dpmfs_create(3, 19, c0, c1, 2.0);
size_t n = resamp_dpmfs_execute(r, in, 4096, out);
resamp_dpmfs_destroy(r);
```




## Public Functions Documentation




### function resamp\_dpmfs\_create

_Create a DPMFS polyphase resampler._
```C++
resamp_dpmfs_state_t * resamp_dpmfs_create (
    size_t poly_order,
    size_t n_taps,
    const float * c0,
    const float * c1,
    double rate
)
```





**Parameters:**


* `poly_order` Polynomial order M (typically 3; must be in [1,3]).
* `n_taps` Taps per phase N.
* `c0` (M+1)\*N float32 coefficients for j=0, row-major.
* `c1` (M+1)\*N float32 coefficients for j=1, row-major.
* `rate` fs\_out / fs\_in. Must be &gt; 0.



**Returns:**

Heap-allocated state, or NULL on failure.







<hr>



### function resamp\_dpmfs\_destroy

_Free a DPMFS resampler._
```C++
void resamp_dpmfs_destroy (
    resamp_dpmfs_state_t * state
)
```





**Parameters:**


* `state` May be NULL.






<hr>



### function resamp\_dpmfs\_execute

_Resample a block of cf32 IQ samples._
```C++
size_t resamp_dpmfs_execute (
    resamp_dpmfs_state_t * state,
    const float complex * in,
    size_t n_in,
    float complex * out
)
```





**Parameters:**


* `state` Must be non-NULL.
* `in` Input array of length n\_in.
* `n_in` Number of input samples (keep &lt;= 4096 per call).
* `out` Output buffer (must hold &gt;= execute\_max\_out() samples).



**Returns:**

Number of output samples written.







<hr>



### function resamp\_dpmfs\_execute\_max\_out

_Maximum output samples per execute call._
```C++
size_t resamp_dpmfs_execute_max_out (
    resamp_dpmfs_state_t * state
)
```



Returns ceil(RESAMP\_DPMFS\_DEFAULT\_BLOCK \* rate) + poly\_order + 8. Allocate at least this many samples for the output buffer.




<hr>



### function resamp\_dpmfs\_get\_num\_taps

_Return the taps per phase (N)._
```C++
size_t resamp_dpmfs_get_num_taps (
    const resamp_dpmfs_state_t * state
)
```




<hr>



### function resamp\_dpmfs\_get\_poly\_order

_Return the polynomial order (M)._
```C++
size_t resamp_dpmfs_get_poly_order (
    const resamp_dpmfs_state_t * state
)
```




<hr>



### function resamp\_dpmfs\_get\_rate

_Return the rate (fs\_out / fs\_in)._
```C++
double resamp_dpmfs_get_rate (
    const resamp_dpmfs_state_t * state
)
```




<hr>



### function resamp\_dpmfs\_reset

_Zero sample history and reset the phase accumulator._
```C++
void resamp_dpmfs_reset (
    resamp_dpmfs_state_t * state
)
```





**Parameters:**


* `state` Must be non-NULL.






<hr>
## Macro Definition Documentation





### define RESAMP\_DPMFS\_DEFAULT\_BLOCK

```C++
#define RESAMP_DPMFS_DEFAULT_BLOCK `4096u`
```



Default input block size for pre-allocated output buffer.




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/resamp_dpmfs/resamp_dpmfs_core.h`
