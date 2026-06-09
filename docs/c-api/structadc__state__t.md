

# Struct adc\_state\_t



[**ClassList**](annotated.md) **>** [**adc\_state\_t**](structadc__state__t.md)



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


Allocate with [**adc\_create()**](adc__core_8h.md#function-adc_create).


`clipped` is sticky — set on any sample that saturates; cleared only by reset(). 


    
## Public Attributes Documentation




### variable bits 

```C++
int adc_state_t::bits;
```




<hr>



### variable clip\_max 

```C++
int64_t adc_state_t::clip_max;
```




<hr>



### variable clip\_min 

```C++
int64_t adc_state_t::clip_min;
```




<hr>



### variable clipped 

```C++
uint8_t adc_state_t::clipped;
```




<hr>



### variable dbfs 

```C++
float adc_state_t::dbfs;
```




<hr>



### variable dithering 

```C++
int adc_state_t::dithering;
```




<hr>



### variable rng 

```C++
uint32_t adc_state_t::rng;
```




<hr>



### variable scale 

```C++
double adc_state_t::scale;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/adc/adc_core.h`

