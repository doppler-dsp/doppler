

# Struct dp\_lo\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_lo\_state\_t**](structdp__lo__state__t.md)



_LO state._ [More...](#detailed-description)

* `#include <lo_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**norm\_freq**](#variable-norm_freq)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**phase\_inc**](#variable-phase_inc)  <br> |












































## Detailed Description


Allocate with [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create), or embed by value and [**lo\_init()**](lo__core_8h.md#function-lo_init) (see the inline composition API below). The shared 65536-entry LUT is initialised lazily on the first [**dp\_lo\_create()**](lo__core_8h.md#function-dp_lo_create)/lo\_init() call and never freed. 


    
## Public Attributes Documentation




### variable norm\_freq 

```C++
double dp_lo_state_t::norm_freq;
```




<hr>



### variable phase 

```C++
uint32_t dp_lo_state_t::phase;
```




<hr>



### variable phase\_inc 

```C++
uint32_t dp_lo_state_t::phase_inc;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/lo/lo_core.h`

