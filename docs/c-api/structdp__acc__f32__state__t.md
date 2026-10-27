

# Struct dp\_acc\_f32\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_acc\_f32\_state\_t**](structdp__acc__f32__state__t.md)



_AccF32 state: one float running sum._ [More...](#detailed-description)

* `#include <acc_f32_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**acc**](#variable-acc)  <br> |












































## Detailed Description


Allocate with [**dp\_acc\_f32\_create()**](acc__f32__core_8h.md#function-dp_acc_f32_create). 


    
## Public Attributes Documentation




### variable acc 

```C++
float dp_acc_f32_state_t::acc;
```



Running sum (running state); seeded by create() or set\_acc(), zeroed by reset() and dump(). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/acc_f32/acc_f32_core.h`

