

# Struct mpsk\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md)



_M-PSK receiver state._ [More...](#detailed-description)

* `#include <mpsk_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  double | [**centre\_freq**](#variable-centre_freq)  <br> |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**fe**](#variable-fe)  <br> |
|  [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) | [**l**](#variable-l)  <br> |












































## Detailed Description


Allocate with [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create). Owns the matched DDC (`fe`) and embeds the loops by value. Treat all fields as internal (use the getters); they are exposed for the inline sample loop. 


    
## Public Attributes Documentation




### variable centre\_freq 

```C++
double mpsk_receiver_state_t::centre_freq;
```



create-time carrier offset (cycles/sample). 
 


        

<hr>



### variable fe 

```C++
ddc_state_t* mpsk_receiver_state_t::fe;
```



matched DDC: mix + cascade + matched filter. 


        

<hr>



### variable l 

```C++
mpsk_rx_loops_t mpsk_receiver_state_t::l;
```



carrier + timing loops, handover, demapper. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_receiver_core.h`

