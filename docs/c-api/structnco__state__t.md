

# Struct nco\_state\_t



[**ClassList**](annotated.md) **>** [**nco\_state\_t**](structnco__state__t.md)



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


Allocate with [**nco\_create()**](nco__core_8h.md#function-nco_create). All fields are managed by the library; read phase and phase\_inc via the property accessors. 


    
## Public Attributes Documentation




### variable nmax 

```C++
uint32_t nco_state_t::nmax;
```




<hr>



### variable norm\_freq 

```C++
double nco_state_t::norm_freq;
```




<hr>



### variable phase 

```C++
uint32_t nco_state_t::phase;
```




<hr>



### variable phase\_inc 

```C++
uint32_t nco_state_t::phase_inc;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/nco/nco_core.h`

