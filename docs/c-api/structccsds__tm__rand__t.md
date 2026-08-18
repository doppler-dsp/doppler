

# Struct ccsds\_tm\_rand\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_rand\_t**](structccsds__tm__rand__t.md)



_A pseudo-randomiser: a maximal-length generator and its preset._ [More...](#detailed-description)

* `#include <ccsds_tm.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**period**](#variable-period)  <br> |
|  uint32\_t | [**seed**](#variable-seed)  <br> |
|  unsigned | [**stages**](#variable-stages)  <br> |
|  uint32\_t | [**taps**](#variable-taps)  <br> |












































## Detailed Description


131.0-B-6 section 10.4 specifies **two**, and which one a mission uses is a choice rather than a property of the coding — so it is a configuration, exactly as the inner code and the outer code are. One implementation serves both; only the table changes.


`taps` is a mask over the register: bit `i` set means stage `i` feeds back. The mask is DERIVED from the characteristic polynomial rather than transcribed from its exponents, and the difference is not cosmetic — `rand.c` records that writing the exponents produced a generator which walked to the all-zero fixed point and passed every structural check except the published prefix. 


    
## Public Attributes Documentation




### variable period 

```C++
size_t ccsds_tm_rand_t::period;
```



`2^stages - 1`, since both are maximal 
 


        

<hr>



### variable seed 

```C++
uint32_t ccsds_tm_rand_t::seed;
```



preset, loaded at the start of every run 
 


        

<hr>



### variable stages 

```C++
unsigned ccsds_tm_rand_t::stages;
```



register width 
 


        

<hr>



### variable taps 

```C++
uint32_t ccsds_tm_rand_t::taps;
```



feedback mask over `stages` bits 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm.h`

