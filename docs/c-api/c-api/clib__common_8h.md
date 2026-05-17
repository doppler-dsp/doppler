

# File clib\_common.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the source code of this file](clib__common_8h_source.md)



* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`
































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CMPLX**](clib__common_8h.md#define-cmplx) (x, y) `\_\_builtin\_complex ((double)(x), (double)(y))`<br> |
| define  | [**CMPLXF**](clib__common_8h.md#define-cmplxf) (x, y) `\_\_builtin\_complex ((float)(x), (float)(y))`<br> |

## Macro Definition Documentation





### define CMPLX

```C++
#define CMPLX (
    x,
    y
) `__builtin_complex ((double)(x), (double)(y))`
```




<hr>



### define CMPLXF

```C++
#define CMPLXF (
    x,
    y
) `__builtin_complex ((float)(x), (float)(y))`
```



[**clib\_common.h**](clib__common_8h.md) — common C99 types for doppler.




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/clib_common.h`
