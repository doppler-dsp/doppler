

# File buffer.h



[**FileList**](files.md) **>** [**buffer**](dir_3a0c1aef7dcd64a21724ce24de18fb81.md) **>** [**buffer.h**](buffer_8h.md)

[Go to the source code of this file](buffer_8h_source.md)

_High-performance x86-64 Circular Buffer for RF Streaming._ [More...](#detailed-description)

* `#include <fcntl.h>`
* `#include <stdio.h>`
* `#include <sys/mman.h>`
* `#include <sys/stat.h>`
* `#include <unistd.h>`
* `#include "dp_interrupt.h"`
* `#include <stdbool.h>`
* `#include <stddef.h>`
* `#include <stdint.h>`
* `#include "jm_perf.h"`
* `#include <stdlib.h>`
* `#include <string.h>`

















## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_wait\_status\_t**](#enum-dp_wait_status_t)  <br>_Why a ring's wait can or cannot be satisfied right now._  |






















## Public Static Functions

| Type | Name |
| ---: | :--- |
|  void \* | [**dp\_\_buf\_alloc**](#function-dp__buf_alloc) (size\_t bytes, void \*\* handle\_out) <br>_Allocates a double-mapped ring-buffer region of_ `bytes` _._ |
|  void \* | [**dp\_\_buf\_alloc\_file**](#function-dp__buf_alloc_file) (size\_t bytes, void \*\* handle\_out, const char \* path, int \* existed) <br>_As dp\_\_buf\_alloc(), but the pages are backed by a FILE._  |
|  void | [**dp\_\_buf\_free**](#function-dp__buf_free) (void \* addr, size\_t bytes, void \* handle) <br>_Releases a double-mapped region created by dp\_\_buf\_alloc()._  |
|  void | [**dp\_\_buf\_sync**](#function-dp__buf_sync) (void \* addr, size\_t bytes) <br>_Flush a file-backed region to disk._  |
|  size\_t | [**dp\_\_page\_size**](#function-dp__page_size) (void) <br>_Returns the granularity the double-mapped views must align to._  |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DECLARE\_DP\_BUFFER**](buffer_8h.md#define-declare_dp_buffer) (name, type) <br>_Generates a type-specific circular buffer implementation._  |
| define  | [**DECLARE\_DP\_BUFFER\_VIEW**](buffer_8h.md#define-declare_dp_buffer_view) (name, type, elem) `/* multi line expression */`<br>_Declares the element-typed face of the_ `name` _ring._ |
| define  | [**DP\_ALIGN**](buffer_8h.md#define-dp_align) (n) `\_\_attribute\_\_ ((aligned (n)))`<br> |
| define  | [**DP\_ASSERT\_2X**](buffer_8h.md#define-dp_assert_2x) (tag, elem, type) `typedef char dp\_assert\_2x\_##tag[sizeof (elem) == 2 \* sizeof (type) ? 1 : -1]`<br> |
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



    
## Public Types Documentation




### enum dp\_wait\_status\_t 

_Why a ring's wait can or cannot be satisfied right now._ 
```C++
enum dp_wait_status_t {
    DP_WAIT_OK = 0,
    DP_WAIT_PENDING = 1,
    DP_WAIT_TOO_LARGE = 2,
    DP_WAIT_CLOSED = 3,
    DP_WAIT_INTERRUPTED = 4
};
```



dp\_\*\_wait() and dp\_\*\_peek() return NULL for more than one reason, and the reasons call for different responses: end of stream is normal and a consumer loop catches it, an interrupt means stop, too-large is a caller bug, and "not yet" is no failure at all. dp\_\*\_wait\_status() owns the PRECEDENCE between them, so a binding or a consumer asks one question instead of re-deriving the order from three  which is what the Python binding did, in three hand-written copies. 


        

<hr>
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



### function dp\_\_buf\_alloc\_file 

_As dp\_\_buf\_alloc(), but the pages are backed by a FILE._ 
```C++
static inline void * dp__buf_alloc_file (
    size_t bytes,
    void ** handle_out,
    const char * path,
    int * existed
) 
```



The mirror trick is indifferent to where the fd came from, so a persistent ring is the same double mapping over an `open()`ed path instead of an anonymous one. Because the mapping is `MAP_SHARED`, the ring's samples ARE the file's contents: there is no separate write path to disk, no copy, and no way for the two to disagree. The kernel writes the pages back on its own schedule; dp\_\_buf\_sync() forces the point.


The file is created if absent and truncated to `bytes`. An EXISTING file of the right size is mapped as it stands, which is what lets a ring survive the process that filled it: the caller restores the head/tail positions and the samples are simply there.




**Parameters:**


* `bytes` Size of ONE mapping (the mirror unit is 2x this). 
* `handle_out` Set to NULL on POSIX (as dp\_\_buf\_alloc). 
* `path` File to back the ring with. 
* `existed` If non-NULL, set to 1 when the file was already the right size (so its contents are the ring's), 0 when it was created or resized. 



**Returns:**

Base address of the double-mapped region, or NULL on failure.




**Note:**

On Windows the file is a CreateFileMapping over CreateFileA, mirrored by the same dp\_\_win\_map\_twice() as the anonymous ring, and dp\_\_buf\_sync() flushes the view but does not wait for the disk. 





        

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



### function dp\_\_buf\_sync 

_Flush a file-backed region to disk._ 
```C++
static inline void dp__buf_sync (
    void * addr,
    size_t bytes
) 
```



A no-op for an anonymous ring, and harmless there. Call it where a checkpoint is TAKEN: the samples are in the page cache until the kernel decides otherwise, so a blob written without this names a history that a crash can still lose.




**Parameters:**


* `addr` Base address (the first view). 
* `bytes` Size of ONE mapping. 




        

<hr>



### function dp\_\_page\_size 

_Returns the granularity the double-mapped views must align to._ 
```C++
static inline size_t dp__page_size (
    void
) 
```



This is the unit the ring-buffer mirror is rounded up to. On POSIX that is the page size. On Windows it is the _allocation granularity_ (64 KiB), which is ≥ dwPageSize: MapViewOfFileEx requires each view's base address to be a multiple of the allocation granularity, so the second (mirror) view at base + bytes is only placeable when `bytes` is a whole multiple of it. Using dwPageSize (4 KiB) here would let a sub-64-KiB buffer pass the size check and then fail to map. 


        

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



### define DECLARE\_DP\_BUFFER\_VIEW 

_Declares the element-typed face of the_ `name` _ring._
```C++
#define DECLARE_DP_BUFFER_VIEW (
    name,
    type,
    elem
) `/* multi line expression */`
```



The ring stores SCALARS (`type *data`, two per complex sample); a caller that thinks in samples  the Python binding above all  wants one ELEMENT per sample. `dp_<name>_wait_view()`, `_peek_view()`, `_write_view()` and `_write_some_view()` are the same four calls addressed that way. Each is a cast and nothing else: every count in this header is already in samples, so no length arithmetic is introduced that could disagree with the scalar face, and the view stays zero-copy.


Siblings rather than a change to the scalar face, because the scalar face is what every C consumer addresses. One macro rather than three pairs, because a cast written three times is three places for the element type to drift  which is what doppler#1346 was.


It is instantiated by the header that owns the element type (`f32_buffer/f32_buffer_core.h` and its two siblings), not here: the complex element is spelled through the portability header those include, and this file stays free of it.




**Parameters:**


* `name` Ring instance suffix, as passed to [**DECLARE\_DP\_BUFFER**](buffer_8h.md#define-declare_dp_buffer). 
* `type` Stored scalar type (`float`, `int16_t`, ...). 
* `elem` Element type spanning exactly two scalars.


```C++
dp_f32_t *ab = dp_f32_create (1024);
float _Complex x[4] = { 1, 2, 3, 4 };
dp_f32_write_some_view (ab, x, 4);              // 4 samples
float _Complex *v = dp_f32_peek_view (ab, 4);   // 4 samples, not 8 floats
dp_f32_consume (ab, 4);
dp_f32_destroy (ab);
```
 


        

<hr>



### define DP\_ALIGN 

```C++
#define DP_ALIGN (
    n
) `__attribute__ ((aligned (n)))`
```




<hr>



### define DP\_ASSERT\_2X 

```C++
#define DP_ASSERT_2X (
    tag,
    elem,
    type
) `typedef char dp_assert_2x_##tag[sizeof (elem) == 2 * sizeof (type) ? 1 : -1]`
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

