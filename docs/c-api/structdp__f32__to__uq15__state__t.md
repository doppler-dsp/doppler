

# Struct dp\_f32\_to\_uq15\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_f32\_to\_uq15\_state\_t**](structdp__f32__to__uq15__state__t.md)



_F32ToUQ15 state._ [More...](#detailed-description)

* `#include <f32_to_uq15_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**dp\_f32\_to\_uq15\_create()**](f32__to__uq15__core_8h.md#function-dp_f32_to_uq15_create).


`clipped` is sticky: set to 1 by the first sample whose pre-saturation scaled value falls outside `[-32768, 32767]`; cleared only by reset(). 


    
## Public Attributes Documentation




### variable clipped 

```C++
uint8_t dp_f32_to_uq15_state_t::clipped;
```



Sticky clip flag (running state): 1 once any scaled sample fell outside -32768..32767, else 0; cleared only by reset(). 


        

<hr>



### variable scale 

```C++
float dp_f32_to_uq15_state_t::scale;
```



Multiply factor applied before rounding and saturation (config, fixed at create; &gt; 0). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/f32_to_uq15/f32_to_uq15_core.h`

