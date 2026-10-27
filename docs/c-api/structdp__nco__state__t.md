

# Struct dp\_nco\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_nco\_state\_t**](structdp__nco__state__t.md)



_NCO state._ [More...](#detailed-description)

* `#include <nco_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**nmax**](#variable-nmax)  <br> |
|  double | [**norm\_freq**](#variable-norm_freq)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**phase\_inc**](#variable-phase_inc)  <br> |












































## Detailed Description


Allocate with [**dp\_nco\_create()**](nco__core_8h.md#function-dp_nco_create). All fields are managed by the library; read phase and phase\_inc via the property accessors. 


    
## Public Attributes Documentation




### variable nmax 

```C++
uint32_t dp_nco_state_t::nmax;
```



Config: wrap target of the \*\_scaled steppers, output in [0, nmax); 0 = raw accumulator. Set once by create. 


        

<hr>



### variable norm\_freq 

```C++
double dp_nco_state_t::norm_freq;
```



Config: frequency in cycles/sample, as given (unfolded); written by create and set\_norm\_freq. 


        

<hr>



### variable phase 

```C++
uint32_t dp_nco_state_t::phase;
```



Running state: phase accumulator in [0, 2^32), one full cycle = 2^32. Zeroed by create/reset; the only field serialized. 


        

<hr>



### variable phase\_inc 

```C++
uint32_t dp_nco_state_t::phase_inc;
```



Config: per-sample advance, floor(frac(norm\_freq) \* 2^32); derived from norm\_freq by create and set\_norm\_freq. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/nco/nco_core.h`

