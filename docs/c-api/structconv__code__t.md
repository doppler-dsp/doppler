

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
|  uint32\_t | [**punct\_mask**](#variable-punct_mask)  <br> |
|  unsigned | [**punct\_period**](#variable-punct_period)  <br> |












































## Detailed Description


`invert` is a bitmask over outputs, not a flag: bit `j` set means output `j` is transmitted inverted. CCSDS sets bit 1 and nothing else.


### Puncturing



A pattern repeats every `punct_period` input bits. Bit `i * n + j` of `punct_mask` says whether output `j` of the `i-th` input bit of the period is **transmitted** (1) or deleted (0), so the mask reads in the order the encoder emits symbols. Both fields zero is no puncturing, which is what a zero-initialized code means, so a code written before the fields existed is unchanged. `punct_period * n` must not exceed 32, and every input bit must keep at least one symbol: an input that transmits nothing would leave a decoder unable to say how many input bits a received stream holds.


The pattern is part of the code, not of a caller's framing. A decoder given a different pattern than the encoder used decodes a stream it was never sent, and nothing in the symbols says so. 



    
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



### variable punct\_mask 

```C++
uint32_t conv_code_t::punct_mask;
```



bit i\*n+j: transmit output j of input i of the period 
 


        

<hr>



### variable punct\_period 

```C++
unsigned conv_code_t::punct_period;
```



input bits per pattern; 0 = none 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/conv/conv_core.h`

