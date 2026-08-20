

# Struct syncword\_state\_t



[**ClassList**](annotated.md) **>** [**syncword\_state\_t**](structsyncword__state__t.md)



_A searcher for one marker._ [More...](#detailed-description)

* `#include <syncword_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t \* | [**marker**](#variable-marker)  <br> |
|  size\_t | [**nbits**](#variable-nbits)  <br> |












































## Detailed Description


Opaque and heap-allocated: it owns a copy of the marker, so a caller may free or reuse the array it constructed from.


Allocate with [**syncword\_create()**](syncword__core_8h.md#function-syncword_create). 


    
## Public Attributes Documentation




### variable marker 

```C++
uint8_t* syncword_state_t::marker;
```



the pattern, unpacked, one bit per byte 
 


        

<hr>



### variable nbits 

```C++
size_t syncword_state_t::nbits;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/syncword/syncword_core.h`

