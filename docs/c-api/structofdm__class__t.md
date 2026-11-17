

# Struct ofdm\_class\_t



[**ClassList**](annotated.md) **>** [**ofdm\_class\_t**](structofdm__class__t.md)



_One kind of OFDM symbol. The arrays are owned by the modulator, copied in by_ [_**dp\_ofdm\_add\_class()**_](ofdm__core_8h.md#function-dp_ofdm_add_class) _and_[_**dp\_ofdm\_add\_fixed()**_](ofdm__core_8h.md#function-dp_ofdm_add_fixed) _._

* `#include <ofdm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**cyclic\_prefix**](#variable-cyclic_prefix)  <br> |
|  [**ofdm\_fixed\_t**](structofdm__fixed__t.md) \* | [**fixed**](#variable-fixed)  <br> |
|  uint32\_t | [**fixed\_cap**](#variable-fixed_cap)  <br> |
|  uint32\_t \* | [**n\_data**](#variable-n_data)  <br> |
|  uint32\_t | [**n\_fft**](#variable-n_fft)  <br> |
|  uint32\_t | [**n\_fixed**](#variable-n_fixed)  <br> |
|  uint32\_t | [**n\_phase**](#variable-n_phase)  <br> |
|  uint32\_t | [**n\_polarity**](#variable-n_polarity)  <br> |
|  float \_Complex \* | [**polarity**](#variable-polarity)  <br> |
|  float \* | [**rise**](#variable-rise)  <br> |
|  double | [**scale**](#variable-scale)  <br> |
|  float \_Complex \* | [**subcarrier\_gain**](#variable-subcarrier_gain)  <br> |
|  uint8\_t \* | [**subcarrier\_map**](#variable-subcarrier_map)  <br> |
|  uint32\_t | [**useful**](#variable-useful)  <br> |
|  uint32\_t | [**window**](#variable-window)  <br> |












































## Public Attributes Documentation




### variable cyclic\_prefix 

```C++
uint32_t ofdm_class_t::cyclic_prefix;
```



Default cyclic prefix, samples. 
 


        

<hr>



### variable fixed 

```C++
ofdm_fixed_t* ofdm_class_t::fixed;
```



Sorted by (phase, k). 
 


        

<hr>



### variable fixed\_cap 

```C++
uint32_t ofdm_class_t::fixed_cap;
```



Bookkeeping: allocated entries. 
 


        

<hr>



### variable n\_data 

```C++
uint32_t* ofdm_class_t::n_data;
```



Derived at seal: points per phase. 


        

<hr>



### variable n\_fft 

```C++
uint32_t ofdm_class_t::n_fft;
```



Transform length of this class. 
 


        

<hr>



### variable n\_fixed 

```C++
uint32_t ofdm_class_t::n_fixed;
```



Entries in `fixed`. 
 


        

<hr>



### variable n\_phase 

```C++
uint32_t ofdm_class_t::n_phase;
```



Fixed-pattern period, symbols. 
 


        

<hr>



### variable n\_polarity 

```C++
uint32_t ofdm_class_t::n_polarity;
```



Length of `polarity`. 
 


        

<hr>



### variable polarity 

```C++
float _Complex* ofdm_class_t::polarity;
```



Per-OFDM-symbol scalar, or NULL. 
 


        

<hr>



### variable rise 

```C++
float* ofdm_class_t::rise;
```



Derived at seal: `window` weights. 


        

<hr>



### variable scale 

```C++
double ofdm_class_t::scale;
```



Multiplies the transform output. 
 


        

<hr>



### variable subcarrier\_gain 

```C++
float _Complex* ofdm_class_t::subcarrier_gain;
```



n\_fft multipliers, or NULL. 
 


        

<hr>



### variable subcarrier\_map 

```C++
uint8_t* ofdm_class_t::subcarrier_map;
```



n\_fft: OFDM\_DATA or OFDM\_NULL, index k + n\_fft/2. 
 


        

<hr>



### variable useful 

```C++
uint32_t ofdm_class_t::useful;
```



Body samples read cyclically; 0 = n\_fft. 
 


        

<hr>



### variable window 

```C++
uint32_t ofdm_class_t::window;
```



Window transition, samples. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/ofdm/ofdm_core.h`

