

# Struct mpsk\_receiver\_state\_t



[**ClassList**](annotated.md) **>** [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md)



_M-PSK receiver state._ [More...](#detailed-description)

* `#include <mpsk_receiver_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**ddc\_state\_t**](ddc__core_8h.md#typedef-ddc_state_t) \* | [**c**](#variable-c)  <br> |
|  double | [**centre\_freq**](#variable-centre_freq)  <br> |
|  union [**mpsk\_receiver\_state\_t**](structmpsk__receiver__state__t.md) | [**fe**](#variable-fe)  <br> |
|  [**mpsk\_rx\_loops\_t**](structmpsk__rx__loops__t.md) | [**l**](#variable-l)  <br> |
|  [**ddcr\_state\_t**](ddcr__core_8h.md#typedef-ddcr_state_t) \* | [**r**](#variable-r)  <br> |
|  int | [**real**](#variable-real)  <br> |












































## Detailed Description


Allocate with [**mpsk\_receiver\_create()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create) (complex input) or [**mpsk\_receiver\_create\_real()**](mpsk__receiver__core_8h.md#function-mpsk_receiver_create_real) (real IF). Owns one matched front end (`fe`) and embeds the loops by value. Treat all fields as internal (use the getters); they are exposed for the inline sample loop. 


    
## Public Attributes Documentation




### variable c 

```C++
ddc_state_t* mpsk_receiver_state_t::c;
```



matched DDC: mix + cascade + MF. 
 


        

<hr>



### variable centre\_freq 

```C++
double mpsk_receiver_state_t::centre_freq;
```



create-time carrier offset (cycles/sample), at the receiver's INPUT rate on both faces. 


        

<hr>



### variable fe 

```C++
union mpsk_receiver_state_t mpsk_receiver_state_t::fe;
```



The matched front end. Which arm is live is `real`, and nothing else reads it — the two step entry points each name their own arm. 


        

<hr>



### variable l 

```C++
mpsk_rx_loops_t mpsk_receiver_state_t::l;
```



carrier + timing loops, demapper. 
 


        

<hr>



### variable r 

```C++
ddcr_state_t* mpsk_receiver_state_t::r;
```



matched DDCR: R2C + mix + cascade + MF. 
 


        

<hr>



### variable real 

```C++
int mpsk_receiver_state_t::real;
```



0 = complex front end, 1 = real IF. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_receiver_core.h`

