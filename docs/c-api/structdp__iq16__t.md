

# Struct dp\_iq16\_t



[**ClassList**](annotated.md) **>** [**dp\_iq16\_t**](structdp__iq16__t.md)



_One q15 complex sample: the element the i16 ring's view hands back._ [More...](#detailed-description)

* `#include <i16_buffer_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int16\_t | [**i**](#variable-i)  <br> |
|  int16\_t | [**q**](#variable-q)  <br> |












































## Detailed Description


numpy has no complex-integer dtype, so the Python face is a structured array of this record  1-D, one element per sample, zero-copy over the ring's interleaved int16 storage. 


    
## Public Attributes Documentation




### variable i 

```C++
int16_t dp_iq16_t::i;
```



In-phase component. 


        

<hr>



### variable q 

```C++
int16_t dp_iq16_t::q;
```



Quadrature component. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/i16_buffer/i16_buffer_core.h`

