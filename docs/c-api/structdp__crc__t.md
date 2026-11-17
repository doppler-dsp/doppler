

# Struct dp\_crc\_t



[**ClassList**](annotated.md) **>** [**dp\_crc\_t**](structdp__crc__t.md)



_A CRC model: width, polynomial, initial register, output reflection and output XOR._ [More...](#detailed-description)

* `#include <dp_crc.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**init**](#variable-init)  <br> |
|  uint64\_t | [**poly**](#variable-poly)  <br> |
|  int | [**refout**](#variable-refout)  <br> |
|  unsigned | [**width**](#variable-width)  <br> |
|  uint64\_t | [**xorout**](#variable-xorout)  <br> |












































## Detailed Description


The polynomial is written in normal form, MSB-first, without the top term: CRC-32's `x^32 + x^26 + ... + 1` is `0x04C11DB7`, and CRC-8's `x^8 + x^2 + x + 1` is `0x07`. 


    
## Public Attributes Documentation




### variable init 

```C++
uint64_t dp_crc_t::init;
```



initial register, `width` bits 
 


        

<hr>



### variable poly 

```C++
uint64_t dp_crc_t::poly;
```



generator polynomial, normal form, `width` bits 
 


        

<hr>



### variable refout 

```C++
int dp_crc_t::refout;
```



non-zero: bit-reverse the register before xorout 


        

<hr>



### variable width 

```C++
unsigned dp_crc_t::width;
```



register width in bits, 1..64 
 


        

<hr>



### variable xorout 

```C++
uint64_t dp_crc_t::xorout;
```



XORed into the (possibly reflected) register 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/dp_crc.h`

