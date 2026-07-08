

# Struct dp\_state\_hdr\_t



[**ClassList**](annotated.md) **>** [**dp\_state\_hdr\_t**](structdp__state__hdr__t.md)



_Common 16-byte envelope at the head of every state blob._ [More...](#detailed-description)

* `#include <dp_state.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**\_pad**](#variable-_pad)  <br> |
|  uint32\_t | [**bytes**](#variable-bytes)  <br> |
|  uint8\_t | [**endian**](#variable-endian)  <br> |
|  uint8\_t | [**flags**](#variable-flags)  <br> |
|  uint32\_t | [**magic**](#variable-magic)  <br> |
|  uint16\_t | [**version**](#variable-version)  <br> |












































## Detailed Description


16 bytes keeps a following `double`/`uint64_t` (a composition's extra header) naturally 8-aligned. 


    
## Public Attributes Documentation




### variable \_pad 

```C++
uint32_t dp_state_hdr_t::_pad;
```



Reserved; 0. 


        

<hr>



### variable bytes 

```C++
uint32_t dp_state_hdr_t::bytes;
```



Total blob size; equals obj\_state\_bytes(). 


        

<hr>



### variable endian 

```C++
uint8_t dp_state_hdr_t::endian;
```



DP\_STATE\_ENDIAN at serialize time. 


        

<hr>



### variable flags 

```C++
uint8_t dp_state_hdr_t::flags;
```



Reserved; 0. 


        

<hr>



### variable magic 

```C++
uint32_t dp_state_hdr_t::magic;
```



Per-object FourCC type tag (DP\_FOURCC). 


        

<hr>



### variable version 

```C++
uint16_t dp_state_hdr_t::version;
```



Per-object blob format version. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_state.h`

