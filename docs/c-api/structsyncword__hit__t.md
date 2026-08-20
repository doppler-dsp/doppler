

# Struct syncword\_hit\_t



[**ClassList**](annotated.md) **>** [**syncword\_hit\_t**](structsyncword__hit__t.md)



_What_ [_**syncword\_find**_](syncword__core_8h.md#function-syncword_find) _found._[More...](#detailed-description)

* `#include <syncword_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**errors**](#variable-errors)  <br> |
|  int | [**found**](#variable-found)  <br> |
|  int | [**inverted**](#variable-inverted)  <br> |
|  size\_t | [**offset**](#variable-offset)  <br> |












































## Detailed Description


A record rather than an out-parameter and a status, because offset, polarity and distance are ONE answer: a receiver that took the offset without the polarity would hand its frame decoder bits it will silently misread. `found` is what the other three mean nothing without, which is why it is a field rather than a sentinel offset — the same choice `frame_check_t` makes with its `checked`.


Distinct from [**dp\_syncword\_hit\_t**](structdp__syncword__hit__t.md), which the kernel fills through a pointer and leaves untouched on a miss: this one is a total answer, returned by value, and so has somewhere to put "no". 


    
## Public Attributes Documentation




### variable errors 

```C++
uint32_t syncword_hit_t::errors;
```



Hamming distance to the marker there 
 


        

<hr>



### variable found 

```C++
int syncword_hit_t::found;
```



A marker was found: 1 yes, 0 no 
 


        

<hr>



### variable inverted 

```C++
int syncword_hit_t::inverted;
```



The stream is complemented 
 


        

<hr>



### variable offset 

```C++
size_t syncword_hit_t::offset;
```



Bit index where the marker starts 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/syncword/syncword_core.h`

