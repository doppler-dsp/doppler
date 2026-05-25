

# Struct cic\_state\_t



[**ClassList**](annotated.md) **>** [**cic\_state\_t**](structcic__state__t.md)



_CIC filter state._ [More...](#detailed-description)

* `#include <cic_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**M**](#variable-m)  <br> |
|  uint32\_t | [**N**](#variable-n)  <br> |
|  uint32\_t | [**R**](#variable-r)  <br> |
|  uint32\_t | [**comb\_head**](#variable-comb_head)  <br> |
|  uint64\_t \* | [**comb\_im**](#variable-comb_im)  <br> |
|  uint64\_t \* | [**comb\_re**](#variable-comb_re)  <br> |
|  double | [**input\_scale**](#variable-input_scale)  <br> |
|  uint64\_t | [**integ\_im**](#variable-integ_im)  <br> |
|  uint64\_t | [**integ\_re**](#variable-integ_re)  <br> |
|  double | [**output\_scale**](#variable-output_scale)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |












































## Detailed Description


Allocate with [**cic\_create()**](cic__core_8h.md#function-cic_create); free with [**cic\_destroy()**](cic__core_8h.md#function-cic_destroy).


integ\_re / integ\_im are fixed-size (max N=6); comb\_re / comb\_im are heap-allocated to N×M elements and freed in [**cic\_destroy()**](cic__core_8h.md#function-cic_destroy). 


    
## Public Attributes Documentation




### variable M 

```C++
uint32_t cic_state_t::M;
```




<hr>



### variable N 

```C++
uint32_t cic_state_t::N;
```




<hr>



### variable R 

```C++
uint32_t cic_state_t::R;
```




<hr>



### variable comb\_head 

```C++
uint32_t cic_state_t::comb_head[6];
```




<hr>



### variable comb\_im 

```C++
uint64_t* cic_state_t::comb_im;
```




<hr>



### variable comb\_re 

```C++
uint64_t* cic_state_t::comb_re;
```




<hr>



### variable input\_scale 

```C++
double cic_state_t::input_scale;
```




<hr>



### variable integ\_im 

```C++
uint64_t cic_state_t::integ_im[6];
```




<hr>



### variable integ\_re 

```C++
uint64_t cic_state_t::integ_re[6];
```




<hr>



### variable output\_scale 

```C++
double cic_state_t::output_scale;
```




<hr>



### variable phase 

```C++
uint32_t cic_state_t::phase;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/cic/cic_core.h`

