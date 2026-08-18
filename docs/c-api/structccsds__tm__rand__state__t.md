

# Struct ccsds\_tm\_rand\_state\_t



[**ClassList**](annotated.md) **>** [**ccsds\_tm\_rand\_state\_t**](structccsds__tm__rand__state__t.md)



_A generator part-way through a run._ [More...](#detailed-description)

* `#include <ccsds_tm.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**reg**](#variable-reg)  <br> |
|  unsigned | [**stages**](#variable-stages)  <br> |
|  uint32\_t | [**taps**](#variable-taps)  <br> |












































## Detailed Description


Exposed because a consumer that is already walking the data — the frame decoder packs bits to octets and derandomises in the same pass — cannot hand a mutable run to [**ccsds\_tm\_randomise**](ccsds__tm_8h.md#function-ccsds_tm_randomise), and must not hold a sequence the size of the data either. Stepping the generator alongside costs one word and works for any period; the alternative was a table indexed modulo the period, which is 128 KB at 10.4.1's and is longer than any CADU. 


    
## Public Attributes Documentation




### variable reg 

```C++
uint32_t ccsds_tm_rand_state_t::reg;
```




<hr>



### variable stages 

```C++
unsigned ccsds_tm_rand_state_t::stages;
```




<hr>



### variable taps 

```C++
uint32_t ccsds_tm_rand_state_t::taps;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ccsds_tm/ccsds_tm.h`

