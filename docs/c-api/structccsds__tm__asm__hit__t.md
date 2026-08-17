

# Struct ccsds\_tm\_asm\_hit\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_asm\_hit\_t**](structccsds__tm__asm__hit__t.md)



_Where an ASM was found, and in which polarity._ [More...](#detailed-description)

* `#include <ccsds_tm.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**errors**](#variable-errors)  <br> |
|  int | [**inverted**](#variable-inverted)  <br> |
|  size\_t | [**offset**](#variable-offset)  <br> |












































## Detailed Description


`inverted` is not a curiosity. A BPSK carrier recovered by a loop with a 180-degree ambiguity delivers the whole stream complemented, and the marker is the only thing in a CADU that can say so — the randomiser does not cover it, so it looks the same in every frame and in exactly one polarity. 


    
## Public Attributes Documentation




### variable errors 

```C++
unsigned ccsds_tm_asm_hit_t::errors;
```



Hamming distance to the marker there 
 


        

<hr>



### variable inverted 

```C++
int ccsds_tm_asm_hit_t::inverted;
```



The stream is complemented 
 


        

<hr>



### variable offset 

```C++
size_t ccsds_tm_asm_hit_t::offset;
```



Bit index where the marker starts 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm.h`

