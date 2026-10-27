

# Struct dp\_f32\_to\_i16u64\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_f32\_to\_i16u64\_state\_t**](structdp__f32__to__i16u64__state__t.md)



_F32ToI16U64 state._ [More...](#detailed-description)

* `#include <f32_to_i16u64_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**dp\_f32\_to\_i16u64\_create()**](f32__to__i16u64__core_8h.md#function-dp_f32_to_i16u64_create).


`clipped` is sticky: set to 1 by the first sample whose pre-saturation scaled value falls outside `[-32768, 32767]`; cleared only by reset(). 


    
## Public Attributes Documentation




### variable clipped 

```C++
uint8_t dp_f32_to_i16u64_state_t::clipped;
```



Sticky saturation flag: 1 once any sample has been clamped, else 0; written by step(), cleared only by reset(). 


        

<hr>



### variable scale 

```C++
float dp_f32_to_i16u64_state_t::scale;
```



Multiply factor applied before saturation; set once by create() (&gt; 0), never changed after. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/f32_to_i16u64/f32_to_i16u64_core.h`

