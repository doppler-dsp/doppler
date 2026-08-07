

# Struct mpsk\_receiver\_r\_state\_t



[**ClassList**](annotated.md) **>** [**mpsk\_receiver\_r\_state\_t**](structmpsk__receiver__r__state__t.md)



_Real-input M-PSK receiver state._ [More...](#detailed-description)

* `#include <mpsk_receiver_r_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**centre\_freq**](#variable-centre_freq)  <br> |
|  [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* | [**fe**](#variable-fe)  <br> |
|  [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) | [**l**](#variable-l)  <br> |












































## Detailed Description


Allocate with [**mpsk\_receiver\_r\_create()**](mpsk__receiver__r__core_8h.md#function-mpsk_receiver_r_create). Owns the matched DDCR (`fe`) and embeds the shared loops by value. 


    
## Public Attributes Documentation




### variable centre\_freq 

```C++
double mpsk_receiver_r_state_t::centre_freq;
```



create-time carrier offset at the INPUT rate. 


        

<hr>



### variable fe 

```C++
ddcr_state_t* mpsk_receiver_r_state_t::fe;
```



matched DDCR: R2C + mix + cascade + MF. 
 


        

<hr>



### variable l 

```C++
mpsk_rx_loops_t mpsk_receiver_r_state_t::l;
```



carrier + timing loops, handover, demapper. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver_r/mpsk_receiver_r_core.h`

