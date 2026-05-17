

# File hbdecim.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**hbdecim.h**](hbdecim_8h.md)

[Go to the source code of this file](hbdecim_8h_source.md)

_Halfband 2:1 decimator for cf32 IQ samples._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_hbdecim\_cf32 | [**dp\_hbdecim\_cf32\_t**](#typedef-dp_hbdecim_cf32_t)  <br>_Opaque halfband decimator state._  |
| typedef struct dp\_hbdecim\_r2cf32 | [**dp\_hbdecim\_r2cf32\_t**](#typedef-dp_hbdecim_r2cf32_t)  <br>_Opaque real-to-complex D2 halfband decimator state._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* | [**dp\_hbdecim\_cf32\_create**](#function-dp_hbdecim_cf32_create) (size\_t num\_taps, const float \* h) <br>_Create a halfband 2:1 decimator._  |
|  void | [**dp\_hbdecim\_cf32\_destroy**](#function-dp_hbdecim_cf32_destroy) ([**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* r) <br>_Free a halfband decimator._  |
|  size\_t | [**dp\_hbdecim\_cf32\_execute**](#function-dp_hbdecim_cf32_execute) ([**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* r, const float \_Complex \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate a block of cf32 IQ samples by 2._  |
|  size\_t | [**dp\_hbdecim\_cf32\_num\_taps**](#function-dp_hbdecim_cf32_num_taps) (const [**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* r) <br>_Return the FIR branch length._  |
|  double | [**dp\_hbdecim\_cf32\_rate**](#function-dp_hbdecim_cf32_rate) (const [**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* r) <br>_Return the decimation rate (always 0.5)._  |
|  void | [**dp\_hbdecim\_cf32\_reset**](#function-dp_hbdecim_cf32_reset) ([**dp\_hbdecim\_cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_cf32_t) \* r) <br>_Zero the sample history and clear any pending sample._  |
|  [**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* | [**dp\_hbdecim\_r2cf32\_create**](#function-dp_hbdecim_r2cf32_create) (size\_t num\_taps, const float \* h) <br>_Create a real-input D2 halfband decimator._  |
|  void | [**dp\_hbdecim\_r2cf32\_destroy**](#function-dp_hbdecim_r2cf32_destroy) ([**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* r) <br>_Free a real-to-complex D2 halfband decimator._  |
|  size\_t | [**dp\_hbdecim\_r2cf32\_execute**](#function-dp_hbdecim_r2cf32_execute) ([**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* r, const float \* in, size\_t num\_in, float \_Complex \* out, size\_t max\_out) <br>_Decimate real float32 samples by 2, producing CF32 IQ._  |
|  size\_t | [**dp\_hbdecim\_r2cf32\_num\_taps**](#function-dp_hbdecim_r2cf32_num_taps) (const [**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* r) <br>_Return the FIR branch length._  |
|  double | [**dp\_hbdecim\_r2cf32\_rate**](#function-dp_hbdecim_r2cf32_rate) (const [**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* r) <br>_Return the decimation rate (always 0.5)._  |
|  void | [**dp\_hbdecim\_r2cf32\_reset**](#function-dp_hbdecim_r2cf32_reset) ([**dp\_hbdecim\_r2cf32\_t**](hbdecim_8h.md#typedef-dp_hbdecim_r2cf32_t) \* r) <br>_Zero history, clear pending sample, reset output parity._  |




























## Detailed Description


## Public Types Documentation




### typedef dp\_hbdecim\_cf32\_t

_Opaque halfband decimator state._
```C++
typedef struct dp_hbdecim_cf32 dp_hbdecim_cf32_t;
```




<hr>



### typedef dp\_hbdecim\_r2cf32\_t

_Opaque real-to-complex D2 halfband decimator state._
```C++
typedef struct dp_hbdecim_r2cf32 dp_hbdecim_r2cf32_t;
```




<hr>
## Public Functions Documentation




### function dp\_hbdecim\_cf32\_create

_Create a halfband 2:1 decimator._
```C++
dp_hbdecim_cf32_t * dp_hbdecim_cf32_create (
    size_t num_taps,
    const float * h
)
```



`h` is the FIR branch of the polyphase bank produced by `kaiser_prototype(phases=2)`. The pure-delay branch is implicit (unit gain at the centre position). The array is copied internally; the caller may free it immediately after this call.


`num_taps` may be even or odd depending on the prototype filter length. Banks from `kaiser_prototype`(phases=2) may produce either an 18-tap (even) or 19-tap (odd) FIR branch.




**Parameters:**


* `num_taps` Length of the FIR branch (odd).
* `h` FIR branch coefficients, float32, length num\_taps.



**Returns:**

Heap-allocated decimator, or NULL on failure.







<hr>



### function dp\_hbdecim\_cf32\_destroy

_Free a halfband decimator._
```C++
void dp_hbdecim_cf32_destroy (
    dp_hbdecim_cf32_t * r
)
```





**Parameters:**


* `r` May be NULL (no-op).






<hr>



### function dp\_hbdecim\_cf32\_execute

_Decimate a block of cf32 IQ samples by 2._
```C++
size_t dp_hbdecim_cf32_execute (
    dp_hbdecim_cf32_t * r,
    const float _Complex * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```



Processes `num_in` input samples and writes at most `max_out` output samples. Produces one output per two inputs; odd-length blocks are buffered transparently.


Internal state is preserved across calls — blocks may be any size.




**Parameters:**


* `r` Must be non-NULL.
* `in` Input sample array (may be NULL if num\_in == 0).
* `num_in` Number of input samples.
* `out` Output buffer (must hold ≥ max\_out samples).
* `max_out` Capacity of `out` in samples.



**Returns:**

Number of output samples written.







<hr>



### function dp\_hbdecim\_cf32\_num\_taps

_Return the FIR branch length._
```C++
size_t dp_hbdecim_cf32_num_taps (
    const dp_hbdecim_cf32_t * r
)
```




<hr>



### function dp\_hbdecim\_cf32\_rate

_Return the decimation rate (always 0.5)._
```C++
double dp_hbdecim_cf32_rate (
    const dp_hbdecim_cf32_t * r
)
```




<hr>



### function dp\_hbdecim\_cf32\_reset

_Zero the sample history and clear any pending sample._
```C++
void dp_hbdecim_cf32_reset (
    dp_hbdecim_cf32_t * r
)
```



Use after a stream discontinuity.




**Parameters:**


* `r` Must be non-NULL.






<hr>



### function dp\_hbdecim\_r2cf32\_create

_Create a real-input D2 halfband decimator._
```C++
dp_hbdecim_r2cf32_t * dp_hbdecim_r2cf32_create (
    size_t num_taps,
    const float * h
)
```





**Parameters:**


* `num_taps` Length of the FIR branch from kaiser\_prototype.
* `h` FIR branch coefficients, float32, length num\_taps.



**Returns:**

Heap-allocated decimator, or NULL on failure.







<hr>



### function dp\_hbdecim\_r2cf32\_destroy

_Free a real-to-complex D2 halfband decimator._
```C++
void dp_hbdecim_r2cf32_destroy (
    dp_hbdecim_r2cf32_t * r
)
```




<hr>



### function dp\_hbdecim\_r2cf32\_execute

_Decimate real float32 samples by 2, producing CF32 IQ._
```C++
size_t dp_hbdecim_r2cf32_execute (
    dp_hbdecim_r2cf32_t * r,
    const float * in,
    size_t num_in,
    float _Complex * out,
    size_t max_out
)
```



Applies the embedded fs/4 shift and produces complex output at half the input rate. Odd-length blocks are handled transparently.




**Parameters:**


* `r` Must be non-NULL.
* `in` Real input samples, float32, length num\_in.
* `num_in` Number of input samples.
* `out` CF32 output buffer, capacity &gt;= max\_out.
* `max_out` Maximum output samples to write.



**Returns:**

Number of output samples written.







<hr>



### function dp\_hbdecim\_r2cf32\_num\_taps

_Return the FIR branch length._
```C++
size_t dp_hbdecim_r2cf32_num_taps (
    const dp_hbdecim_r2cf32_t * r
)
```




<hr>



### function dp\_hbdecim\_r2cf32\_rate

_Return the decimation rate (always 0.5)._
```C++
double dp_hbdecim_r2cf32_rate (
    const dp_hbdecim_r2cf32_t * r
)
```




<hr>



### function dp\_hbdecim\_r2cf32\_reset

_Zero history, clear pending sample, reset output parity._
```C++
void dp_hbdecim_r2cf32_reset (
    dp_hbdecim_r2cf32_t * r
)
```




<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/hbdecim.h`
