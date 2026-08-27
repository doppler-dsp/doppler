

# Struct interleaver\_state\_t



[**ClassList**](annotated.md) **>** [**interleaver\_state\_t**](structinterleaver__state__t.md)



_A block interleaver's geometry._ [More...](#detailed-description)

* `#include <interleaver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**cols**](#variable-cols)  <br> |
|  size\_t | [**rows**](#variable-rows)  <br> |
|  size\_t | [**unit\_bits**](#variable-unit_bits)  <br> |












































## Detailed Description


Three numbers and no buffers: the permutation is arithmetic, so there is nothing to allocate per block and nothing to grow. 


    
## Public Attributes Documentation




### variable cols 

```C++
size_t interleaver_state_t::cols;
```



block span — units per codeword 
 


        

<hr>



### variable rows 

```C++
size_t interleaver_state_t::rows;
```



interleaving depth — codewords interleaved 
 


        

<hr>



### variable unit\_bits 

```C++
size_t interleaver_state_t::unit_bits;
```



bits per interleaved unit; 1 bit, 8 octet 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/interleaver/interleaver_core.h`

