

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
|  double | [**dp\_fftfreq**](#function-dp_fftfreq) (size\_t bin, size\_t n, double fs) <br>_The frequency of an FFT bin, in the units of_ `fs` _._ |
|  long | [**dp\_fftfreq\_index**](#function-dp_fftfreq_index) (size\_t bin, size\_t n) <br>`numpy.fft.fftfreq(n)[bin] * n` _— the SIGNED index of an FFT bin._ |
|  void \* | [**dp\_xcalloc**](#function-dp_xcalloc) (size\_t nmemb, size\_t size) <br> |
|  void \* | [**dp\_xmalloc**](#function-dp_xmalloc) (size\_t n) <br> |
|  void \* | [**dp\_xnn**](#function-dp_xnn) (void \* p) <br> |
|  void \* | [**dp\_xrealloc**](#function-dp_xrealloc) (void \* p, size\_t n) <br> |

























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




### function dp\_fftfreq 

_The frequency of an FFT bin, in the units of_ `fs` _._
```C++
static inline double dp_fftfreq (
    size_t bin,
    size_t n,
    double fs
) 
```



`dp_fftfreq_index(bin, n) * fs / n` — numpy's `fftfreq(n, d)[bin]` with the sample RATE where numpy takes the sample SPACING. That is the one deliberate difference from the numpy signature, and it is the right way round for this library: every caller here has `fs` in hand and would otherwise write `1.0 / fs` at the call site, which is a reciprocal to get wrong for no benefit. Pass `fs = 1.0` for normalised cycles/sample, which is numpy's default.




**Parameters:**


* `bin` Bin index in `[0, n)`. 
* `n` Grid size (&gt; 0). 
* `fs` Sample rate; the result is in these units. 



**Returns:**

Bin frequency in `[-fs/2, +fs/2)`. 





        

<hr>



### function dp\_fftfreq\_index 

`numpy.fft.fftfreq(n)[bin] * n` _— the SIGNED index of an FFT bin._
```C++
static inline long dp_fftfreq_index (
    size_t bin,
    size_t n
) 
```



`0 = DC`, ascending positive to `(n-1)/2`, then wrapping negative, so an even-length grid puts its Nyquist bin at `-n/2`. Multiply by the grid's bin spacing for Hz, or use dp\_fftfreq() for the normalised frequency.


Named for what it is. It arrived as an acquisition-specific helper called `dp_fftfreq_index`, which is how it came to disagree with numpy at exactly one index: it reported `+n/2` at the Nyquist bin. That is not wrong on its own  `+n/2` and `-n/2` are the same frequency, and a search on this grid cannot tell them apart  but every formula ported in from numpy then disagreed with the engine at the one bin the engine was most careful about. Following the universal convention deletes that class of surprise rather than documenting it.


What must not vary is the READER: a consumer seeded on one side of the fold while the search meant the other is off by the full span. That happened here once  an acquisition's wideband search and its hand-off spelled the fold differently  and it surfaced as a receiver reporting `tracking == 1` while decoding noise. So this lives in the COMMON header, inline, and `doppler.dsss.bin_to_signed` is a thin wrapper over it, so C and Python call the same code instead of restating the arithmetic.




**Parameters:**


* `bin` Bin index in `[0, n)`. 
* `n` Grid size. 



**Returns:**

Signed index in `[-(n/2), +((n-1)/2)]`. 





        

<hr>



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



### function dp\_xrealloc 

```C++
static inline void * dp_xrealloc (
    void * p,
    size_t n
) 
```



realloc that aborts on OOM — the third member of the family.


A grow-on-demand scratch buffer is the shape that wants it: the new size scales with the caller's block, but the only way the call fails is still genuine exhaustion, so the unwind path is as uncoverable as malloc's. Passing NULL for `p` is a fresh allocation, exactly as realloc defines. 


        

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

