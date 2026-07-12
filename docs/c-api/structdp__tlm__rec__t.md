

# Struct dp\_tlm\_rec\_t



[**ClassList**](annotated.md) **>** [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md)



_One telemetry sample: a probe's scalar value at sample index_ `n` _._[More...](#detailed-description)

* `#include <telemetry.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint16\_t | [**flags**](#variable-flags)  <br> |
|  uint64\_t | [**n**](#variable-n)  <br> |
|  uint16\_t | [**probe**](#variable-probe)  <br> |
|  float | [**value**](#variable-value)  <br> |












































## Detailed Description


16 bytes, 8-aligned — one ring slot. `value` is float: ~7 significant digits is ample for diagnostics (timing error, dB gains, lock metrics); `flags` reserves room for a future wide-value record class. 


    
## Public Attributes Documentation




### variable flags 

```C++
uint16_t dp_tlm_rec_t::flags;
```



Reserved; 0. 


        

<hr>



### variable n 

```C++
uint64_t dp_tlm_rec_t::n;
```



Caller-stamped sample index (dp\_tlm\_set\_now). 


        

<hr>



### variable probe 

```C++
uint16_t dp_tlm_rec_t::probe;
```



Probe id (index into the context's table). 


        

<hr>



### variable value 

```C++
float dp_tlm_rec_t::value;
```



The scalar, narrowed to float. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/telemetry/telemetry.h`

