

# Struct f32\_to\_i16u32\_state\_t



[**ClassList**](annotated.md) **>** [**f32\_to\_i16u32\_state\_t**](structf32__to__i16u32__state__t.md)



_F32ToI16U32 state._ [More...](#detailed-description)

* `#include <f32_to_i16u32_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**f32\_to\_i16u32\_create()**](f32__to__i16u32__core_8h.md#function-f32_to_i16u32_create).


`clipped` is sticky: set to 1 by the first sample whose pre-saturation scaled value falls outside `[-32768, 32767]`; cleared only by reset(). 


    
## Public Attributes Documentation




### variable clipped 

```C++
uint8_t f32_to_i16u32_state_t::clipped;
```




<hr>



### variable scale 

```C++
float f32_to_i16u32_state_t::scale;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/f32_to_i16u32/f32_to_i16u32_core.h`

