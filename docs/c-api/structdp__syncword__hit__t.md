

# Struct dp\_syncword\_hit\_t



[**ClassList**](annotated.md) **>** [**dp\_syncword\_hit\_t**](structdp__syncword__hit__t.md)



_Where a marker was found, and in which polarity._ [More...](#detailed-description)

* `#include <dp_syncword.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  unsigned | [**errors**](#variable-errors)  <br> |
|  int | [**inverted**](#variable-inverted)  <br> |
|  size\_t | [**offset**](#variable-offset)  <br> |












































## Detailed Description


`inverted` is not a curiosity. A BPSK carrier recovered by a loop with a 180-degree ambiguity delivers the whole stream complemented, and a marker that no randomiser covers is the only thing in a frame that can say so — it looks the same in every frame and in exactly one polarity. 


    
## Public Attributes Documentation




### variable errors 

```C++
unsigned dp_syncword_hit_t::errors;
```



Hamming distance to the marker there 
 


        

<hr>



### variable inverted 

```C++
int dp_syncword_hit_t::inverted;
```



The stream is complemented 
 


        

<hr>



### variable offset 

```C++
size_t dp_syncword_hit_t::offset;
```



Bit index where the marker starts 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_syncword.h`

