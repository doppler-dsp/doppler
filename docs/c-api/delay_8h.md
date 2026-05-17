

# File delay.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**delay.h**](delay_8h.md)

[Go to the source code of this file](delay_8h_source.md)

_Dual-buffer circular delay line for cf64 IQ samples._ [More...](#detailed-description)

* `#include <dp/stream.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_delay\_cf64 | [**dp\_delay\_cf64\_t**](#typedef-dp_delay_cf64_t)  <br>_Opaque cf64 delay line._  |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_delay\_cf64\_capacity**](#function-dp_delay_cf64_capacity) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br>_Return the internal capacity (power of two ≥ num\_taps)._  |
|  [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* | [**dp\_delay\_cf64\_create**](#function-dp_delay_cf64_create) (size\_t num\_taps) <br>_Allocate a cf64 delay line for_ `num_taps` _samples._ |
|  void | [**dp\_delay\_cf64\_destroy**](#function-dp_delay_cf64_destroy) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br>_Free a cf64 delay line._  |
|  size\_t | [**dp\_delay\_cf64\_num\_taps**](#function-dp_delay_cf64_num_taps) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br>_Return the number of taps the delay line was created for._  |
|  const double \_Complex \* | [**dp\_delay\_cf64\_ptr**](#function-dp_delay_cf64_ptr) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br>_Return a pointer to the contiguous_ `num_taps-sample` _window._ |
|  void | [**dp\_delay\_cf64\_push**](#function-dp_delay_cf64_push) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, double \_Complex x) <br>_Push one cf64 sample into the delay line._  |
|  const double \_Complex \* | [**dp\_delay\_cf64\_push\_ptr**](#function-dp_delay_cf64_push_ptr) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, double \_Complex x) <br>_Push a sample and return the updated read pointer in one call._  |
|  void | [**dp\_delay\_cf64\_reset**](#function-dp_delay_cf64_reset) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br>_Zero the sample history without freeing the delay line._  |
|  void | [**dp\_delay\_cf64\_write**](#function-dp_delay_cf64_write) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, const double \_Complex \* in, size\_t n) <br>_Push_ `n` _samples from an array into the delay line._ |




























## Detailed Description


## Public Types Documentation




### typedef dp\_delay\_cf64\_t

_Opaque cf64 delay line._
```C++
typedef struct dp_delay_cf64 dp_delay_cf64_t;
```




<hr>
## Public Functions Documentation




### function dp\_delay\_cf64\_capacity

_Return the internal capacity (power of two ≥ num\_taps)._
```C++
size_t dp_delay_cf64_capacity (
    const dp_delay_cf64_t * dl
)
```





**Parameters:**


* `dl` Must be non-NULL.






<hr>



### function dp\_delay\_cf64\_create

_Allocate a cf64 delay line for_ `num_taps` _samples._
```C++
dp_delay_cf64_t * dp_delay_cf64_create (
    size_t num_taps
)
```



Internal capacity is rounded up to the next power of two so that the head pointer can be advanced with a bitmask instead of a modulo operation. The dual buffer is initialised to all zeros.




**Parameters:**


* `num_taps` Number of taps (filter length ≥ 1).



**Returns:**

Heap-allocated delay line, or NULL on failure.







<hr>



### function dp\_delay\_cf64\_destroy

_Free a cf64 delay line._
```C++
void dp_delay_cf64_destroy (
    dp_delay_cf64_t * dl
)
```





**Parameters:**


* `dl` May be NULL (no-op).






<hr>



### function dp\_delay\_cf64\_num\_taps

_Return the number of taps the delay line was created for._
```C++
size_t dp_delay_cf64_num_taps (
    const dp_delay_cf64_t * dl
)
```





**Parameters:**


* `dl` Must be non-NULL.






<hr>



### function dp\_delay\_cf64\_ptr

_Return a pointer to the contiguous_ `num_taps-sample` _window._
```C++
const double _Complex * dp_delay_cf64_ptr (
    const dp_delay_cf64_t * dl
)
```




```C++
ptr[0]          — most recent sample
ptr[1]          — one sample ago
...
ptr[num_taps-1] — oldest sample in the window
```



The pointer is valid until the next call to [**dp\_delay\_cf64\_push()**](delay_8h.md#function-dp_delay_cf64_push). Pass directly to [**dp\_acc\_cf64\_madd()**](accumulator_8h.md#function-dp_acc_cf64_madd) — no copy required.




**Parameters:**


* `dl` Must be non-NULL.



**Returns:**

Pointer into the dual buffer; valid for `num_taps` reads.







<hr>



### function dp\_delay\_cf64\_push

_Push one cf64 sample into the delay line._
```C++
void dp_delay_cf64_push (
    dp_delay_cf64_t * dl,
    double _Complex x
)
```



Advances the internal head pointer by one (bitmask wrap) and writes `x` to both halves of the dual buffer. After the call, [**dp\_delay\_cf64\_ptr()**](delay_8h.md#function-dp_delay_cf64_ptr) returns a window with `x` at index 0.




**Parameters:**


* `dl` Must be non-NULL.
* `x` New sample (most recent).






<hr>



### function dp\_delay\_cf64\_push\_ptr

_Push a sample and return the updated read pointer in one call._
```C++
const double _Complex * dp_delay_cf64_push_ptr (
    dp_delay_cf64_t * dl,
    double _Complex x
)
```



Convenience wrapper for the interpolation hot loop:
```C++
const double _Complex *win = dp_delay_cf64_push_ptr(dl, x);
dp_acc_cf64_madd(acc, win, h, num_taps);
```





**Parameters:**


* `dl` Must be non-NULL.
* `x` New sample.



**Returns:**

Updated contiguous read pointer.







<hr>



### function dp\_delay\_cf64\_reset

_Zero the sample history without freeing the delay line._
```C++
void dp_delay_cf64_reset (
    dp_delay_cf64_t * dl
)
```



Use after a stream discontinuity to prevent history contamination.




**Parameters:**


* `dl` Must be non-NULL.






<hr>



### function dp\_delay\_cf64\_write

_Push_ `n` _samples from an array into the delay line._
```C++
void dp_delay_cf64_write (
    dp_delay_cf64_t * dl,
    const double _Complex * in,
    size_t n
)
```



Equivalent to calling [**dp\_delay\_cf64\_push()**](delay_8h.md#function-dp_delay_cf64_push) `n` times with `in`[0], `in`[1], ..., `in`[n-1] in order. After the call, [**dp\_delay\_cf64\_ptr()**](delay_8h.md#function-dp_delay_cf64_ptr)[0] == `in`[n-1] (the last sample pushed).


Useful for loading a block of input samples before running the polyphase MAC:
```C++
dp_delay_cf64_write(dl, block, block_len);
const double _Complex *win = dp_delay_cf64_ptr(dl);
dp_acc_cf64_madd(acc, win, h, num_taps);
```





**Parameters:**


* `dl` Must be non-NULL.
* `in` Input array of `n` cf64 samples (oldest first).
* `n` Number of samples to push.






<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/delay.h`
