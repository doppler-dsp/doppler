

# Struct dp\_pn\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_pn\_state\_t**](structdp__pn__state__t.md)



_PN state._ [More...](#detailed-description)

* `#include <pn_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint64\_t | [**fib\_taps**](#variable-fib_taps)  <br> |
|  int | [**kind**](#variable-kind)  <br> |
|  uint64\_t | [**mask**](#variable-mask)  <br> |
|  uint64\_t | [**poly**](#variable-poly)  <br> |
|  uint64\_t | [**reg**](#variable-reg)  <br> |
|  uint64\_t | [**seed**](#variable-seed)  <br> |
|  uint32\_t | [**topshift**](#variable-topshift)  <br> |












































## Detailed Description


Allocate with [**dp\_pn\_create()**](pn__core_8h.md#function-dp_pn_create). Every field except `reg` is config set by create() and never changed; `reg` is the running register. 


    
## Public Attributes Documentation




### variable fib\_taps 

```C++
uint64_t dp_pn_state_t::fib_taps;
```



Fibonacci feedback taps (canonical polynomial derived from `poly`, masked); 0 for Galois. 


        

<hr>



### variable kind 

```C++
int dp_pn_state_t::kind;
```



PN\_GALOIS or PN\_FIBONACCI; any other lfsr argument to create() becomes PN\_GALOIS. 


        

<hr>



### variable mask 

```C++
uint64_t dp_pn_state_t::mask;
```



(1 &lt;&lt; length) - 1; all ones when length == 64. 


        

<hr>



### variable poly 

```C++
uint64_t dp_pn_state_t::poly;
```



Galois feedback tap mask, masked to the register (right-shift convention). 


        

<hr>



### variable reg 

```C++
uint64_t dp_pn_state_t::reg;
```



Current LFSR register (running state); its LSB is the next chip. The only serialized field. 


        

<hr>



### variable seed 

```C++
uint64_t dp_pn_state_t::seed;
```



Initial register value, masked to the register; reloaded by reset(). Never zero. 


        

<hr>



### variable topshift 

```C++
uint32_t dp_pn_state_t::topshift;
```



length - 1: the bit the Fibonacci feedback enters at. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/pn/pn_core.h`

