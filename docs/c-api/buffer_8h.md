

# File buffer.h



[**FileList**](files.md) **>** [**buffer**](dir_3a0c1aef7dcd64a21724ce24de18fb81.md) **>** [**buffer.h**](buffer_8h.md)

[Go to the source code of this file](buffer_8h_source.md)

_High-performance x86-64 Circular Buffer for RF Streaming._ [More...](#detailed-description)

* `#include <fcntl.h>`
* `#include <stdio.h>`
* `#include <sys/mman.h>`
* `#include <unistd.h>`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include <stdlib.h>`
* `#include <string.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void \* | [**dp\_\_buf\_alloc**](#function-dp__buf_alloc) (size\_t bytes, void \*\* handle\_out) <br>_Allocates a double-mapped ring-buffer region of_ `bytes` _._ |
|  void | [**dp\_\_buf\_free**](#function-dp__buf_free) (void \* addr, size\_t bytes, void \* handle) <br>_Releases a double-mapped region created by dp\_\_buf\_alloc()._  |
|  size\_t | [**dp\_\_page\_size**](#function-dp__page_size) (void) <br>_Returns the system page size._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DECLARE\_DP\_BUFFER**](buffer_8h.md#define-declare_dp_buffer) (name, type) <br>_Generates a type-specific circular buffer implementation._  |
| define  | [**DP\_ALIGN**](buffer_8h.md#define-dp_align) (n) `\_\_attribute\_\_ ((aligned (n)))`<br> |
| define  | [**DP\_ASSERT\_PWR2**](buffer_8h.md#define-dp_assert_pwr2) (n) `typedef char dp\_assert\_pwr2\_##n[((n) & ((n) - 1)) == 0 ? 1 : -1]`<br> |
| define  | [**DP\_CACHELINE**](buffer_8h.md#define-dp_cacheline)  `64`<br>_Standard x86-64 cache-line size (64 bytes)._  |
| define  | [**DP\_SPIN\_HINT**](buffer_8h.md#define-dp_spin_hint) () `((void)0)`<br> |

## Detailed Description


## Virtual Memory Buffers


doppler uses virtual memory mirroring to eliminate the "wrap-around" problem in circular buffers. This allows for zero-copy, branchless access to contiguous blocks of data across the buffer boundary.



## Virtual Memory Mirroring


By mapping the same physical memory to two adjacent virtual addresses (A and A + N), we exploit the CPU's MMU to handle circular indexing at the hardware level.



## of Two Masking


We use & mask instead of % capacity. On x86-64, bitwise AND is a single-cycle instruction, whereas integer modulo can take 20-80 cycles.



## Sharing


The head and tail pointers are separated by 64 bytes to prevent the "Ping-Pong" effect where two CPU cores constantly invalidate each other's cache lines when updating indices.



## Optimization


[**DP\_SPIN\_HINT()**](buffer_8h.md#define-dp_spin_hint) is used in the consumer loop to reduce power consumption and prevent the CPU from mispredicting the "loop end" during high-frequency polling. 



    
## Public Static Functions Documentation




### function dp\_\_buf\_alloc 

_Allocates a double-mapped ring-buffer region of_ `bytes` _._
```C++
static inline void * dp__buf_alloc (
    size_t bytes,
    void ** handle_out
) 
```



The returned address `addr` satisfies:
* addr(0..bytes-1) ← first view (writable)
* addr(bytes..2\*bytes-1) ← second view (same physical pages)




On Windows, a HANDLE to the file-mapping object is written to `handle_out` and must be passed to dp\_\_buf\_free(). On POSIX, `handle_out` is set to NULL.




**Returns:**

Base address of the double-mapped region, or NULL on failure. 





        

<hr>



### function dp\_\_buf\_free 

_Releases a double-mapped region created by dp\_\_buf\_alloc()._ 
```C++
static inline void dp__buf_free (
    void * addr,
    size_t bytes,
    void * handle
) 
```





**Parameters:**


* `addr` Base address returned by dp\_\_buf\_alloc(). 
* `bytes` Size of ONE mapping (same value passed to dp\_\_buf\_alloc). 
* `handle` Platform handle returned via handle\_out (Win32: HANDLE, else NULL). 




        

<hr>



### function dp\_\_page\_size 

_Returns the system page size._ 
```C++
static inline size_t dp__page_size (
    void
) 
```




<hr>
## Macro Definition Documentation





### define DECLARE\_DP\_BUFFER 

_Generates a type-specific circular buffer implementation._ 
```C++
#define DECLARE_DP_BUFFER (
    name,
    type
) 
```





**Parameters:**


* `name` Suffix for generated names (e.g., f32, i16). 
* `type` Underlying primitive type for complex I/Q samples. 




        

<hr>



### define DP\_ALIGN 

```C++
#define DP_ALIGN (
    n
) `__attribute__ ((aligned (n)))`
```




<hr>



### define DP\_ASSERT\_PWR2 

```C++
#define DP_ASSERT_PWR2 (
    n
) `typedef char dp_assert_pwr2_##n[((n) & ((n) - 1)) == 0 ? 1 : -1]`
```




<hr>



### define DP\_CACHELINE 

_Standard x86-64 cache-line size (64 bytes)._ 
```C++
#define DP_CACHELINE `64`
```




<hr>



### define DP\_SPIN\_HINT 

```C++
#define DP_SPIN_HINT (
    
) `((void)0)`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/buffer/buffer.h`

