

# Struct cic\_state\_t



[**ClassList**](annotated.md) **>** [**cic\_state\_t**](structcic__state__t.md)



_CIC filter state._ [More...](#detailed-description)

* `#include <cic_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**R**](#variable-r)  <br> |
|  uint64\_t | [**comb\_im**](#variable-comb_im)  <br> |
|  uint64\_t | [**comb\_re**](#variable-comb_re)  <br> |
|  uint64\_t | [**integ\_im**](#variable-integ_im)  <br> |
|  uint64\_t | [**integ\_re**](#variable-integ_re)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |
|  uint32\_t | [**shift**](#variable-shift)  <br> |












































## Detailed Description


Allocate with [**cic\_create()**](cic__core_8h.md#function-cic_create); free with [**cic\_destroy()**](cic__core_8h.md#function-cic_destroy). All comb state fits in-struct — no heap members. 


    
## Public Attributes Documentation




### variable R 

```C++
uint32_t cic_state_t::R;
```




<hr>



### variable comb\_im 

```C++
uint64_t cic_state_t::comb_im[CIC_N];
```




<hr>



### variable comb\_re 

```C++
uint64_t cic_state_t::comb_re[CIC_N];
```




<hr>



### variable integ\_im 

```C++
uint64_t cic_state_t::integ_im[CIC_N];
```




<hr>



### variable integ\_re 

```C++
uint64_t cic_state_t::integ_re[CIC_N];
```




<hr>



### variable phase 

```C++
uint32_t cic_state_t::phase;
```




<hr>



### variable shift 

```C++
uint32_t cic_state_t::shift;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

