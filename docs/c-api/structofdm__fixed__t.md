

# Struct ofdm\_fixed\_t



[**ClassList**](annotated.md) **>** [**ofdm\_fixed\_t**](structofdm__fixed__t.md)



_One entry of a class's fixed pattern._ 

* `#include <ofdm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int32\_t | [**k**](#variable-k)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  float \_Complex | [**value**](#variable-value)  <br> |












































## Public Attributes Documentation




### variable k 

```C++
int32_t ofdm_fixed_t::k;
```



Subcarrier, -n\_fft/2 .. n\_fft/2 - 1. 
 


        

<hr>



### variable phase 

```C++
uint32_t ofdm_fixed_t::phase;
```



Pattern phase, or OFDM\_ALL\_PHASES. 
 


        

<hr>



### variable value 

```C++
float _Complex ofdm_fixed_t::value;
```



A pilot, a training value, a boosted pilot. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ofdm/ofdm_core.h`

