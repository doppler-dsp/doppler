

# Struct dp\_chunk\_t



[**ClassList**](annotated.md) **>** [**dp\_chunk\_t**](structdp__chunk__t.md)



_Reassembly geometry, present only when_ [_**DP\_FLAG\_CHUNKED**_](group__wire.md#define-dp_flag_chunked) _._[More...](#detailed-description)

* `#include <stream.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**count**](#variable-count)  <br> |
|  uint32\_t | [**index**](#variable-index)  <br> |
|  uint64\_t | [**offset**](#variable-offset)  <br> |
|  uint64\_t | [**total\_bytes**](#variable-total_bytes)  <br> |












































## Detailed Description


Immediately follows the header and precedes the chunk's own payload bytes. It rides only chunked frames rather than sitting in every header: the previous format spent a third of its 96 bytes on four `reserved[]` words that were documented as "do not interpret" and were in fact this, zeroed on every unchunked frame. 


    
## Public Attributes Documentation




### variable count 

```C++
uint32_t dp_chunk_t::count;
```



Chunks in this frame. 


        

<hr>



### variable index 

```C++
uint32_t dp_chunk_t::index;
```



0-based chunk number. 


        

<hr>



### variable offset 

```C++
uint64_t dp_chunk_t::offset;
```



This chunk's byte offset into that payload. 


        

<hr>



### variable total\_bytes 

```C++
uint64_t dp_chunk_t::total_bytes;
```



Payload bytes in the whole logical frame. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/stream/stream.h`

