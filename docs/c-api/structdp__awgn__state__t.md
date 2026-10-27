

# Struct dp\_awgn\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_awgn\_state\_t**](structdp__awgn__state__t.md)



_AWGN generator state (opaque to callers; use the API)._ [More...](#detailed-description)

* `#include <awgn_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  float | [**amplitude**](#variable-amplitude)  <br> |
|  uint64\_t | [**s**](#variable-s)  <br> |
|  uint64\_t | [**seed**](#variable-seed)  <br> |












































## Detailed Description


All three fields are serialized by [**dp\_awgn\_get\_state()**](awgn__core_8h.md#function-dp_awgn_get_state). 


    
## Public Attributes Documentation




### variable amplitude 

```C++
float dp_awgn_state_t::amplitude;
```



Per-component (I and Q) standard deviation; finite and &gt;= 0. Set by create and [**dp\_awgn\_set\_amplitude()**](awgn__core_8h.md#function-dp_awgn_set_amplitude). 


        

<hr>



### variable s 

```C++
uint64_t dp_awgn_state_t::s[4];
```



xoshiro256++ running state; advanced by two words per generated sample. Never all zero. 


        

<hr>



### variable seed 

```C++
uint64_t dp_awgn_state_t::seed;
```



the seed [**dp\_awgn\_reset()**](awgn__core_8h.md#function-dp_awgn_reset) replays: create's, or the last reseed's or restored blob's 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/awgn/awgn_core.h`

