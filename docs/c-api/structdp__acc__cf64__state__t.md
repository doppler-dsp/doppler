

# Struct dp\_acc\_cf64\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_acc\_cf64\_state\_t**](structdp__acc__cf64__state__t.md)



_AccCf64 state: the single running complex sum._ [More...](#detailed-description)

* `#include <acc_cf64_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double \_Complex | [**acc**](#variable-acc)  <br> |












































## Detailed Description


Pointer-free, so it serializes as a whole-struct snapshot. Allocate with [**dp\_acc\_cf64\_create()**](acc__cf64__core_8h.md#function-dp_acc_cf64_create). 


    
## Public Attributes Documentation




### variable acc 

```C++
double _Complex dp_acc_cf64_state_t::acc;
```



Running complex sum (running state). Seeded by create() or set\_acc(), grown by step/steps/madd/ add2d/madd2d, zeroed by reset() and dump(). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/acc_cf64/acc_cf64_core.h`

