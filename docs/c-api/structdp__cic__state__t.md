

# Struct dp\_cic\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_cic\_state\_t**](structdp__cic__state__t.md)



_CIC filter state._ [More...](#detailed-description)

* `#include <cic_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**R**](#variable-r)  <br> |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  uint64\_t | [**comb\_im**](#variable-comb_im)  <br> |
|  uint64\_t | [**comb\_re**](#variable-comb_re)  <br> |
|  uint64\_t | [**integ\_im**](#variable-integ_im)  <br> |
|  uint64\_t | [**integ\_re**](#variable-integ_re)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**shift**](#variable-shift)  <br> |












































## Detailed Description


Allocate with [**dp\_cic\_create()**](cic__core_8h.md#function-dp_cic_create); free with [**dp\_cic\_destroy()**](cic__core_8h.md#function-dp_cic_destroy). All comb state fits in-struct — no heap members. 


    
## Public Attributes Documentation




### variable R 

```C++
uint32_t dp_cic_state_t::R;
```



Decimation ratio, a power of two in `[2, CIC_R_MAX]`; config, set by create and [**dp\_cic\_reconfigure()**](cic__core_8h.md#function-dp_cic_reconfigure). 


        

<hr>



### variable clipped 

```C++
uint8_t dp_cic_state_t::clipped;
```



Sticky: 1 once an input component exceeded +-2.0 (CIC\_PAPR\_HEADROOM); cleared only by reset/reconfigure. 


        

<hr>



### variable comb\_im 

```C++
uint64_t dp_cic_state_t::comb_im[CIC_N];
```



Previous comb input per stage, imag path; running state. 


        

<hr>



### variable comb\_re 

```C++
uint64_t dp_cic_state_t::comb_re[CIC_N];
```



Previous comb input per stage, real path; running state. 


        

<hr>



### variable integ\_im 

```C++
uint64_t dp_cic_state_t::integ_im[CIC_N];
```



Integrator accumulators, imag path; running state. 


        

<hr>



### variable integ\_re 

```C++
uint64_t dp_cic_state_t::integ_re[CIC_N];
```



Integrator accumulators, real path (UQ16 input, wrapping mod 2^64); running state. 


        

<hr>



### variable phase 

```C++
uint32_t dp_cic_state_t::phase;
```



Input samples since the last output, 0..R-1; running state. 


        

<hr>



### variable shift 

```C++
uint32_t dp_cic_state_t::shift;
```



Normalizing right-shift in bits, `CIC_N * log2(R)`; derived from R. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/cic/cic_core.h`

