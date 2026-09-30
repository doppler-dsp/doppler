

# File dp\_hash64.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**dp\_hash64.h**](dp__hash64_8h.md)

[Go to the source code of this file](dp__hash64_8h_source.md)

_FNV-1a 64 over bytes, incrementally: the one content hash in doppler._ [More...](#detailed-description)

* `#include <stddef.h>`
* `#include <stdint.h>`







































## Public Static Functions

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**dp\_hash64**](#function-dp_hash64) (uint64\_t h, const void \* data, size\_t n) <br>_Continue an FNV-1a 64 hash over_ `n` _more bytes._ |

























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_HASH64\_INIT**](dp__hash64_8h.md#define-dp_hash64_init)  `UINT64\_C (0xcbf29ce484222325)`<br>_The FNV-1a 64 offset basis: the hash of no bytes, and where a hash starts._  |
| define  | [**DP\_HASH64\_PRIME**](dp__hash64_8h.md#define-dp_hash64_prime)  `UINT64\_C (0x100000001b3)`<br>_The FNV-1a 64 prime._  |

## Detailed Description


It identifies content and does not protect it. A record names a `--data-from-file` source by its path, its length in bits and this hash, and a replay refuses a file whose hash differs (docs/design/payload-data-source.md §4.8). A path alone replays whatever the file holds on the day, and a length alone misses an edit of the same size.


FNV-1a rather than xxHash64 because identity needs no speed: the hash runs at the rate a file is read, and a file is read at the rate frames are sent. FNV-1a is a few lines, works a byte at a time and so is incremental by construction, and has published test vectors. Not for anything an adversary chooses: a collision can be constructed.


Header-only, for the reason [**dp\_crc16.h**](dp__crc16_8h.md) gives: no component grows a link-line dependency for a few lines of arithmetic. 


    
## Public Static Functions Documentation




### function dp\_hash64 

_Continue an FNV-1a 64 hash over_ `n` _more bytes._
```C++
static inline uint64_t dp_hash64 (
    uint64_t h,
    const void * data,
    size_t n
) 
```



Pass [**DP\_HASH64\_INIT**](dp__hash64_8h.md#define-dp_hash64_init) to start, then feed each block as it is read, passing back what the last call returned. The result does not depend on where the input was split, so a file hashed read by read gives the same value as the whole file hashed at once, and is never read twice. The state is the returned `uint64_t` itself, so a source that serializes its state carries the hash by value.




**Parameters:**


* `h` the hash so far; [**DP\_HASH64\_INIT**](dp__hash64_8h.md#define-dp_hash64_init) for none. 
* `data` the next bytes; may be NULL when `n` is 0. 
* `n` how many. 



**Returns:**

the hash over everything fed so far.



```C++
uint64_t h = DP_HASH64_INIT;
h = dp_hash64 (h, "foo", 3);
h = dp_hash64 (h, "bar", 3);   // 0x85944171f73967e8, as for "foobar"
```
 


        

<hr>
## Macro Definition Documentation





### define DP\_HASH64\_INIT 

_The FNV-1a 64 offset basis: the hash of no bytes, and where a hash starts._ 
```C++
#define DP_HASH64_INIT `UINT64_C (0xcbf29ce484222325)`
```




<hr>



### define DP\_HASH64\_PRIME 

_The FNV-1a 64 prime._ 
```C++
#define DP_HASH64_PRIME `UINT64_C (0x100000001b3)`
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_hash64.h`

