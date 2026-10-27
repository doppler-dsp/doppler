

# Struct dp\_f32\_to\_i8\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_f32\_to\_i8\_state\_t**](structdp__f32__to__i8__state__t.md)



_F32ToI8 state._ [More...](#detailed-description)

* `#include <f32_to_i8_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**dp\_f32\_to\_i8\_create()**](f32__to__i8__core_8h.md#function-dp_f32_to_i8_create).


`clipped` is sticky: set to 1 by the first sample whose pre-saturation scaled value falls outside `[-128, 127]`; cleared only by reset(). 


    
## Public Attributes Documentation




### variable clipped 

```C++
uint8_t dp_f32_to_i8_state_t::clipped;
```



Sticky flag: 1 once any scaled sample fell outside `[-128, 127]`, 0 otherwise. Running state, set by step()/steps(), cleared by reset(). 


        

<hr>



### variable scale 

```C++
float dp_f32_to_i8_state_t::scale;
```



Multiply factor applied before rounding and saturation (codes per unit input). Config, set by create(). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/f32_to_i8/f32_to_i8_core.h`

