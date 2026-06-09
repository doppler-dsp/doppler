

# File clib\_common.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the source code of this file](clib__common_8h_source.md)



* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`
* `#include "jm_perf.h"`
































































## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CMPLXF**](clib__common_8h.md#define-cmplxf) (r, i) `\_\_builtin\_complex ((float)(r), (float)(i))`<br> |
| define  | [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid)  `(-2)`<br> |
| define  | [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory)  `(-1)`<br> |
| define  | [**DP\_OK**](clib__common_8h.md#define-dp_ok)  `0`<br> |

## Macro Definition Documentation





### define CMPLXF 

```C++
#define CMPLXF (
    r,
    i
) `__builtin_complex ((float)(r), (float)(i))`
```



[**clib\_common.h**](clib__common_8h.md) — common C99 types and performance macros for doppler. 


        

<hr>



### define DP\_ERR\_INVALID 

```C++
#define DP_ERR_INVALID `(-2)`
```



Invalid argument. 


        

<hr>



### define DP\_ERR\_MEMORY 

```C++
#define DP_ERR_MEMORY `(-1)`
```



Memory allocation failure. 


        

<hr>



### define DP\_OK 

```C++
#define DP_OK `0`
```



Success. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/clib_common.h`

