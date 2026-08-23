

# File clib\_common.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**clib\_common.h**](clib__common_8h.md)

[Go to the source code of this file](clib__common_8h_source.md)



* `#include <complex.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`
* `#include "jm_perf.h"`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void \* | [**dp\_xcalloc**](#function-dp_xcalloc) (size\_t nmemb, size\_t size) <br> |
|  void \* | [**dp\_xmalloc**](#function-dp_xmalloc) (size\_t n) <br> |
|  void \* | [**dp\_xnn**](#function-dp_xnn) (void \* p) <br> |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**CMPLXF**](clib__common_8h.md#define-cmplxf) (r, i) `\_\_builtin\_complex ((float)(r), (float)(i))`<br> |
| define  | [**DP\_ERR\_CLOSED**](clib__common_8h.md#define-dp_err_closed)  `(-9)`<br> |
| define  | [**DP\_ERR\_EOF**](clib__common_8h.md#define-dp_err_eof)  `(-10)`<br> |
| define  | [**DP\_ERR\_INIT**](clib__common_8h.md#define-dp_err_init)  `(-1)`<br> |
| define  | [**DP\_ERR\_INTERRUPTED**](clib__common_8h.md#define-dp_err_interrupted)  `(-8)`<br> |
| define  | [**DP\_ERR\_INVALID**](clib__common_8h.md#define-dp_err_invalid)  `(-4)`<br> |
| define  | [**DP\_ERR\_MEMORY**](clib__common_8h.md#define-dp_err_memory)  `(-6)`<br> |
| define  | [**DP\_ERR\_RECV**](clib__common_8h.md#define-dp_err_recv)  `(-3)`<br> |
| define  | [**DP\_ERR\_SEND**](clib__common_8h.md#define-dp_err_send)  `(-2)`<br> |
| define  | [**DP\_ERR\_TIMEOUT**](clib__common_8h.md#define-dp_err_timeout)  `(-5)`<br> |
| define  | [**DP\_ERR\_TOO\_LARGE**](clib__common_8h.md#define-dp_err_too_large)  `(-7)`<br> |
| define  | [**DP\_OK**](clib__common_8h.md#define-dp_ok)  `0`<br> |

## Public Static Functions Documentation




### function dp\_xcalloc 

```C++
static inline void * dp_xcalloc (
    size_t nmemb,
    size_t size
) 
```



calloc that aborts on OOM (zero-initialised trusted allocation). 


        

<hr>



### function dp\_xmalloc 

```C++
static inline void * dp_xmalloc (
    size_t n
) 
```



malloc that aborts on OOM (for a trusted internal allocation). 


        

<hr>



### function dp\_xnn 

```C++
static inline void * dp_xnn (
    void * p
) 
```



Assert a just-constructed object / allocation is non-NULL, aborting with a diagnostic on the impossible OOM. The single check point: a sub-object create() returns NULL only on OOM once its arguments are validated, so wrap the call — `x = dp_xnn (foo_create (...))` — instead of checking-and-unwinding at every call site. (Classic GNU `xmalloc`.) 


        

<hr>
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



### define DP\_ERR\_CLOSED 

```C++
#define DP_ERR_CLOSED `(-9)`
```



The context is draining or closed and accepts no more sends  \ a state, not a transport failure. 


        

<hr>



### define DP\_ERR\_EOF 

```C++
#define DP_ERR_EOF `(-10)`
```



The producer has finished: no more data is coming, ever. A \ state, not a failure, and distinct from DP\_ERR\_TIMEOUT, which \ means "not yet". Every transport spells it the same way  see \ docs/design/io-termination.md. 


        

<hr>



### define DP\_ERR\_INIT 

```C++
#define DP_ERR_INIT `(-1)`
```



Initialisation failed (context/socket). 


        

<hr>



### define DP\_ERR\_INTERRUPTED 

```C++
#define DP_ERR_INTERRUPTED `(-8)`
```



A blocking call returned because [**dp\_stream\_interrupt()**](group__interrupt.md#function-dp_stream_interrupt) was \ called  a request to stop, not a failure. 


        

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

