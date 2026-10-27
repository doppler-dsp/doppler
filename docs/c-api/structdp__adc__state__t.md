

# Struct dp\_adc\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_adc\_state\_t**](structdp__adc__state__t.md)



_ADC state._ [More...](#detailed-description)

* `#include <adc_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  int | [**bits**](#variable-bits)  <br> |
|  int64\_t | [**clip\_max**](#variable-clip_max)  <br> |
|  int64\_t | [**clip\_min**](#variable-clip_min)  <br> |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  float | [**dbfs**](#variable-dbfs)  <br> |
|  int | [**dithering**](#variable-dithering)  <br> |
|  uint32\_t | [**rng**](#variable-rng)  <br> |
|  double | [**scale**](#variable-scale)  <br> |












































## Detailed Description


Allocate with [**dp\_adc\_create()**](adc__core_8h.md#function-dp_adc_create).


`clipped` is sticky — set on any sample that saturates; cleared only by reset(). 


    
## Public Attributes Documentation




### variable bits 

```C++
int dp_adc_state_t::bits;
```



ADC resolution in bits, 1..64. Config. 


        

<hr>



### variable clip\_max 

```C++
int64_t dp_adc_state_t::clip_max;
```



Largest output code, 2^(bits-1) - 1 (INT64\_MAX when bits = 64). Config. 


        

<hr>



### variable clip\_min 

```C++
int64_t dp_adc_state_t::clip_min;
```



Smallest output code, -2^(bits-1) (INT64\_MIN when bits = 64). Config. 


        

<hr>



### variable clipped 

```C++
uint8_t dp_adc_state_t::clipped;
```



Sticky saturation flag: 1 once any sample saturated, 0 otherwise. Running state, set by step()/steps(), cleared by reset(). 


        

<hr>



### variable dbfs 

```C++
float dp_adc_state_t::dbfs;
```



Full-scale reference level in dBFS, as passed to create(). Config; informational once `scale` is derived. 


        

<hr>



### variable dithering 

```C++
int dp_adc_state_t::dithering;
```



0 = no dither; positive = TPDF dither before rounding. Config. 


        

<hr>



### variable rng 

```C++
uint32_t dp_adc_state_t::rng;
```



xorshift32 dither PRNG state. Running state; seeded to a fixed constant by create() and reset(), advanced only when dithering. 


        

<hr>



### variable scale 

```C++
double dp_adc_state_t::scale;
```



Input-to-code gain, 2^(bits-1) \* 10^(-dbfs/20) (codes per unit input). Config, set by create(). 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/adc/adc_core.h`

