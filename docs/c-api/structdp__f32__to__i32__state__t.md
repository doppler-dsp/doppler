

# Struct dp\_f32\_to\_i32\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_f32\_to\_i32\_state\_t**](structdp__f32__to__i32__state__t.md)



_F32ToI32 state._ [More...](#detailed-description)

* `#include <f32_to_i32_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**dp\_f32\_to\_i32\_create()**](f32__to__i32__core_8h.md#function-dp_f32_to_i32_create).


`clipped` is sticky: set to 1 by the first sample whose pre-saturation scaled value falls outside `[-2147483648, 2147483647]`; cleared only by reset(). 


    
## Public Attributes Documentation




### variable clipped 

```C++
uint8_t dp_f32_to_i32_state_t::clipped;
```



Sticky saturation flag (running state): 1 once any sample has saturated, 0 otherwise; written by step()/steps(), cleared by reset(). 


        

<hr>



### variable scale 

```C++
float dp_f32_to_i32_state_t::scale;
```



Multiply factor applied before rounding and saturation (config, set by create(); &gt; 0; default 2^31 = 2147483648.0). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/f32_to_i32/f32_to_i32_core.h`

