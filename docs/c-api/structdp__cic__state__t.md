

# Struct dp\_cic\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_cic\_state\_t**](structdp__cic__state__t.md)



_CIC filter state._ [More...](#detailed-description)

* `#include <cic_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**R**](#variable-r)  <br> |
|  uint32\_t | [**bits**](#variable-bits)  <br> |
|  uint8\_t | [**clipped**](#variable-clipped)  <br> |
|  uint64\_t | [**comb\_im**](#variable-comb_im)  <br> |
|  uint64\_t | [**comb\_re**](#variable-comb_re)  <br> |
|  uint32\_t | [**headroom\_bits**](#variable-headroom_bits)  <br> |
|  float | [**in\_hi**](#variable-in_hi)  <br> |
|  float | [**in\_lo**](#variable-in_lo)  <br> |
|  float | [**in\_scale**](#variable-in_scale)  <br> |
|  uint64\_t | [**integ\_im**](#variable-integ_im)  <br> |
|  uint64\_t | [**integ\_re**](#variable-integ_re)  <br> |
|  uint64\_t | [**offset**](#variable-offset)  <br> |
|  float | [**out\_scale**](#variable-out_scale)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**precision**](#variable-precision)  <br> |
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



### variable bits 

```C++
uint32_t dp_cic_state_t::bits;
```



The input word's width, sign included: the precision, or `CIC_REG_BITS - shift` when that is less; derived. 


        

<hr>



### variable clipped 

```C++
uint8_t dp_cic_state_t::clipped;
```



Sticky: 1 once an input component reached the bound ([**dp\_cic\_get\_full\_scale()**](cic__core_8h.md#function-dp_cic_get_full_scale)); cleared by reset/reconfigure, and overwritten by set\_state(). 


        

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



### variable headroom\_bits 

```C++
uint32_t dp_cic_state_t::headroom_bits;
```



Bits of the input word above unit amplitude; config, set with the precision and by [**dp\_cic\_set\_headroom\_bits()**](cic__core_8h.md#function-dp_cic_set_headroom_bits). 


        

<hr>



### variable in\_hi 

```C++
float dp_cic_state_t::in_hi;
```



The largest sample the word holds, as a float: the float under `2^(bits - 1)`; derived. 


        

<hr>



### variable in\_lo 

```C++
float dp_cic_state_t::in_lo;
```



The smallest, `-2^(bits - 1)`; derived. 


        

<hr>



### variable in\_scale 

```C++
float dp_cic_state_t::in_scale;
```



Unit amplitude as an integer, `2^(bits - 1 - headroom_bits)`; derived. 


        

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



Integrator accumulators, real path (the sample, wrapping mod 2^64); running state. 


        

<hr>



### variable offset 

```C++
uint64_t dp_cic_state_t::offset;
```



The word's offset-binary midpoint, `2^(bits - 1)`; derived. 


        

<hr>



### variable out\_scale 

```C++
float dp_cic_state_t::out_scale;
```



`1 / in_scale`; derived. 


        

<hr>



### variable phase 

```C++
uint32_t dp_cic_state_t::phase;
```



Input samples since the last output, 0..R-1; running state. 


        

<hr>



### variable precision 

```C++
uint32_t dp_cic_state_t::precision;
```



CIC\_PRECISION\_16, \_32 or \_64; config, set by create (CIC\_PRECISION\_DEFAULT) and [**dp\_cic\_set\_precision()**](cic__core_8h.md#function-dp_cic_set_precision). 


        

<hr>



### variable shift 

```C++
uint32_t dp_cic_state_t::shift;
```



Normalizing right-shift in bits, `CIC_N * log2(R)`; derived from R. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/cic/cic_core.h`

