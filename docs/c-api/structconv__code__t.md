

# Struct conv\_code\_t



[**ClassList**](annotated.md) **>** [**conv\_code\_t**](structconv__code__t.md)



_A rate-1/n convolutional code._ [More...](#detailed-description)

* `#include <conv_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**invert**](#variable-invert)  <br> |
|  unsigned | [**k**](#variable-k)  <br> |
|  unsigned | [**n**](#variable-n)  <br> |
|  uint32\_t | [**poly**](#variable-poly)  <br> |












































## Detailed Description


`invert` is a bitmask over outputs, not a flag: bit `j` set means output `j` is transmitted inverted. CCSDS sets bit 1 and nothing else. 


    
## Public Attributes Documentation




### variable invert 

```C++
uint32_t conv_code_t::invert;
```



bit j: output j is inverted 
 


        

<hr>



### variable k 

```C++
unsigned conv_code_t::k;
```



constraint length, 2..CONV\_K\_MAX 
 


        

<hr>



### variable n 

```C++
unsigned conv_code_t::n;
```



outputs per input, 1..CONV\_N\_MAX 
 


        

<hr>



### variable poly 

```C++
uint32_t conv_code_t::poly[CONV_N_MAX];
```



generator polynomials, k bits each 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/conv/conv_core.h`

