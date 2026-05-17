

# Struct delay\_state\_t



[**ClassList**](annotated.md) **>** [**delay\_state\_t**](structdelay__state__t.md)



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


Allocate with [**delay\_create()**](delay__core_8h.md#function-delay_create). 


    
## Public Attributes Documentation




### variable buf 

```C++
double _Complex* delay_state_t::buf;
```




<hr>



### variable capacity 

```C++
size_t delay_state_t::capacity;
```




<hr>



### variable head 

```C++
size_t delay_state_t::head;
```




<hr>



### variable mask 

```C++
size_t delay_state_t::mask;
```




<hr>



### variable num\_taps 

```C++
size_t delay_state_t::num_taps;
```




<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/delay/delay_core.h`

