

# Struct fec\_conv\_t



[**ClassList**](annotated.md) **>** [**fec\_conv\_t**](structfec__conv__t.md)



_Rate-1/2 constraint-length-7 convolutional encoder state._ [More...](#detailed-description)

* `#include <fec_ccsds.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint8\_t | [**reg**](#variable-reg)  <br> |












































## Detailed Description


Seven bits of shift register, the current input in the high stage. Held in a struct rather than passed as a `uint8_t` because the encoder is **continuous**: 3.3.2 fixes the output sequence as `C1(1), C2(1), C1(2), C2(2)...` with no per-frame flush, so a caller encoding a long record in chunks must carry the register across calls or introduce a discontinuity every chunk boundary that no decoder expects. 


    
## Public Attributes Documentation




### variable reg 

```C++
uint8_t fec_conv_t::reg;
```



7 stages; bit 6 is the newest input 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/fec/fec_ccsds.h`

