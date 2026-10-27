

# Struct dp\_delay\_state\_t



[**ClassList**](annotated.md) **>** [**dp\_delay\_state\_t**](structdp__delay__state__t.md)



_Delay state._ [More...](#detailed-description)

* `#include <delay_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double \_Complex \* | [**buf**](#variable-buf)  <br> |
|  size\_t | [**capacity**](#variable-capacity)  <br> |
|  size\_t | [**head**](#variable-head)  <br> |
|  size\_t | [**mask**](#variable-mask)  <br> |
|  size\_t | [**num\_taps**](#variable-num_taps)  <br> |












































## Detailed Description


Dual-buffer circular delay line. The backing store is a contiguous allocation of 2\*capacity elements: the first half is the live ring; the second half mirrors it so that any window of num\_taps consecutive samples is always contiguous in memory (no wrap-around copy needed).


Allocate with [**dp\_delay\_create()**](delay__core_8h.md#function-dp_delay_create). 


    
## Public Attributes Documentation




### variable buf 

```C++
double _Complex* dp_delay_state_t::buf;
```



Owned ring, 2\*capacity elements; the second half mirrors the first (running state). 


        

<hr>



### variable capacity 

```C++
size_t dp_delay_state_t::capacity;
```



Smallest power of two &gt;= num\_taps (1 when num\_taps is 0), samples (config). 
 


        

<hr>



### variable head 

```C++
size_t dp_delay_state_t::head;
```



Index of the newest sample, in [0, capacity); decremented mod capacity on every write (running state). 
 


        

<hr>



### variable mask 

```C++
size_t dp_delay_state_t::mask;
```



capacity - 1, the power-of-two index mask (config, set at create()). 
 


        

<hr>



### variable num\_taps 

```C++
size_t dp_delay_state_t::num_taps;
```



Window length requested at create(), samples (config). 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/delay/delay_core.h`

