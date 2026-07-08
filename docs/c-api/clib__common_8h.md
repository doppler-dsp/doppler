

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
| define  | [**DP\_ERR\_INIT**](clib__common_8h.md#define-dp_err_init)  `(-1)`<br> |
| define  | [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid)  `(-4)`<br> |
| define  | [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory)  `(-6)`<br> |
| define  | [**DP\_ERR\_RECV**](clib__common_8h.md#define-dp_err_recv)  `(-3)`<br> |
| define  | [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send)  `(-2)`<br> |
| define  | [**DP\_ERR\_TIMEOUT**](clib__common_8h.md#define-dp_err_timeout)  `(-5)`<br> |
| define  | [**DP\_ERR\_TOO\_LARGE**](clib__common_8h.md#define-dp_err_too_large)  `(-7)`<br> |
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



### define DP\_ERR\_INIT 

```C++
#define DP_ERR_INIT `(-1)`
```



Initialisation failed (context/socket). 


        

<hr>



### define DP\_ERR\_INVALID 

```C++
#define DP_ERR_INVALID `(-4)`
```



Invalid argument. 


        

<hr>



### define DP\_ERR\_MEMORY 

```C++
#define DP_ERR_MEMORY `(-6)`
```



Memory allocation failure. 


        

<hr>



### define DP\_ERR\_RECV 

```C++
#define DP_ERR_RECV `(-3)`
```



Receive failed or timed out (EAGAIN). 


        

<hr>



### define DP\_ERR\_SEND 

```C++
#define DP_ERR_SEND `(-2)`
```



Send failed. 


        

<hr>



### define DP\_ERR\_TIMEOUT 

```C++
#define DP_ERR_TIMEOUT `(-5)`
```



Operation timed out. 


        

<hr>



### define DP\_ERR\_TOO\_LARGE 

```C++
#define DP_ERR_TOO_LARGE `(-7)`
```



Frame exceeds transport max payload. 


        

<hr>



### define DP\_OK 

```C++
#define DP_OK `0`
```



Success. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/clib_common.h`

