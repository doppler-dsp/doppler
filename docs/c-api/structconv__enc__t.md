

# Struct conv\_enc\_t



[**ClassList**](annotated.md) **>** [**conv\_enc\_t**](structconv__enc__t.md)



_Encoder state: the shift register, and nothing else._ [More...](#detailed-description)

* `#include <conv_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**reg**](#variable-reg)  <br> |












































## Detailed Description


Held in a struct rather than passed by value because the encoder is **continuous** — a caller encoding a long record in chunks must carry the register across calls or introduce a discontinuity at every chunk boundary that no decoder expects. 


    
## Public Attributes Documentation




### variable reg 

```C++
uint32_t conv_enc_t::reg;
```



the k-1 previous inputs; newest in the high stage 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/conv/conv_core.h`

