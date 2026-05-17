

# File stream.h



[**FileList**](files.md) **>** [**c**](dir_1784a01aa976a8c78ef5dfc3737bcac8.md) **>** [**include**](dir_2d10db7395ecfee73f7722e70cabff64.md) **>** [**dp**](dir_11a94baa66ce4f1e4099aa44a4fd2c26.md) **>** [**stream.h**](stream_8h.md)

[Go to the source code of this file](stream_8h_source.md)

_Streaming API for doppler — PUB/SUB, PUSH/PULL, REQ/REP._ [More...](#detailed-description)

* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`















## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_header\_t**](structdp__header__t.md) <br>_Frame metadata header carried in every ZMQ message._  |

















































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CMPLX**](stream_8h.md#define-cmplx) (x, y) `\_\_builtin\_complex((double)(x), (double)(y))`<br> |
| define  | [**CMPLXF**](stream_8h.md#define-cmplxf) (x, y) `\_\_builtin\_complex((float)(x), (float)(y))`<br> |
| define  | [**CMPLXL**](stream_8h.md#define-cmplxl) (x, y) `\_\_builtin\_complex((long double)(x), (long double)(y))`<br> |

## Detailed Description


## Macro Definition Documentation





### define CMPLX

```C++
#define CMPLX (
    x,
    y
) `__builtin_complex((double)(x), (double)(y))`
```




<hr>



### define CMPLXF

```C++
#define CMPLXF (
    x,
    y
) `__builtin_complex((float)(x), (float)(y))`
```




<hr>



### define CMPLXL

```C++
#define CMPLXL (
    x,
    y
) `__builtin_complex((long double)(x), (long double)(y))`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `c/include/dp/stream.h`
