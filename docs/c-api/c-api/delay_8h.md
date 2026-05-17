

# File delay.h



[**FileList**](files.md) **>** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md) **>** [**delay.h**](delay_8h.md)

[Go to the source code of this file](delay_8h_source.md)

_Dual-buffer circular delay line for cf64 IQ samples._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_delay\_cf64 | [**dp\_delay\_cf64\_t**](#typedef-dp_delay_cf64_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  size\_t | [**dp\_delay\_cf64\_capacity**](#function-dp_delay_cf64_capacity) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br> |
|  [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* | [**dp\_delay\_cf64\_create**](#function-dp_delay_cf64_create) (size\_t num\_taps) <br> |
|  void | [**dp\_delay\_cf64\_destroy**](#function-dp_delay_cf64_destroy) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br> |
|  size\_t | [**dp\_delay\_cf64\_num\_taps**](#function-dp_delay_cf64_num_taps) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br> |
|  const double \_Complex \* | [**dp\_delay\_cf64\_ptr**](#function-dp_delay_cf64_ptr) (const [**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br> |
|  void | [**dp\_delay\_cf64\_push**](#function-dp_delay_cf64_push) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, double \_Complex x) <br> |
|  const double \_Complex \* | [**dp\_delay\_cf64\_push\_ptr**](#function-dp_delay_cf64_push_ptr) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, double \_Complex x) <br> |
|  void | [**dp\_delay\_cf64\_reset**](#function-dp_delay_cf64_reset) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl) <br> |
|  void | [**dp\_delay\_cf64\_write**](#function-dp_delay_cf64_write) ([**dp\_delay\_cf64\_t**](delay_8h.md#typedef-dp_delay_cf64_t) \* dl, const double \_Complex \* in, size\_t n) <br> |




























## Detailed Description


Lifted from c/src/delay.c for the native Python extension tree.


The dual-buffer trick keeps the most-recent num\_taps samples in a contiguous window at all times, eliminating wrap-around in the MAC hot loop. Internal capacity is rounded up to the next power of two so head advances with a bitmask, not modulo.


Layout: buf[0 .. cap-1] lower half buf[cap .. 2cap-1] upper half (mirror)


Every push writes to buf[head] and buf[head+capacity]; ptr() returns &buf[head], which is always a valid num\_taps-element read window.



## Public Types Documentation




### typedef dp\_delay\_cf64\_t

```C++
typedef struct dp_delay_cf64 dp_delay_cf64_t;
```




<hr>
## Public Functions Documentation




### function dp\_delay\_cf64\_capacity

```C++
size_t dp_delay_cf64_capacity (
    const dp_delay_cf64_t * dl
)
```




<hr>



### function dp\_delay\_cf64\_create

```C++
dp_delay_cf64_t * dp_delay_cf64_create (
    size_t num_taps
)
```




<hr>



### function dp\_delay\_cf64\_destroy

```C++
void dp_delay_cf64_destroy (
    dp_delay_cf64_t * dl
)
```




<hr>



### function dp\_delay\_cf64\_num\_taps

```C++
size_t dp_delay_cf64_num_taps (
    const dp_delay_cf64_t * dl
)
```




<hr>



### function dp\_delay\_cf64\_ptr

```C++
const double _Complex * dp_delay_cf64_ptr (
    const dp_delay_cf64_t * dl
)
```




<hr>



### function dp\_delay\_cf64\_push

```C++
void dp_delay_cf64_push (
    dp_delay_cf64_t * dl,
    double _Complex x
)
```




<hr>



### function dp\_delay\_cf64\_push\_ptr

```C++
const double _Complex * dp_delay_cf64_push_ptr (
    dp_delay_cf64_t * dl,
    double _Complex x
)
```




<hr>



### function dp\_delay\_cf64\_reset

```C++
void dp_delay_cf64_reset (
    dp_delay_cf64_t * dl
)
```




<hr>



### function dp\_delay\_cf64\_write

```C++
void dp_delay_cf64_write (
    dp_delay_cf64_t * dl,
    const double _Complex * in,
    size_t n
)
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/delay/delay.h`
