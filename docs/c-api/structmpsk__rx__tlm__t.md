

# Struct mpsk\_rx\_tlm\_t



[**ClassList**](annotated.md) **>** [**mpsk\_rx\_tlm\_t**](structmpsk__rx__tlm__t.md)



_Telemetry attachment for the receiver's own two probes; the timing and carrier probes ride their own sub-attachments._ 

* `#include <mpsk_rx_loops.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_e**](#variable-id_e)  <br> |
|  int32\_t | [**id\_freq**](#variable-id_freq)  <br> |
|  int32\_t | [**id\_lock**](#variable-id_lock)  <br> |
|  int32\_t | [**id\_locked**](#variable-id_locked)  <br> |
|  int32\_t | [**id\_nco**](#variable-id_nco)  <br> |
|  int32\_t | [**id\_tracking**](#variable-id_tracking)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* mpsk_rx_tlm_t::ctx;
```



NULL = detached 
 


        

<hr>



### variable id\_e 

```C++
int32_t mpsk_rx_tlm_t::id_e;
```



"&lt;prefix&gt;.car.e" — carrier disc 
 


        

<hr>



### variable id\_freq 

```C++
int32_t mpsk_rx_tlm_t::id_freq;
```



"&lt;prefix&gt;.car.freq" — integrator only 
 


        

<hr>



### variable id\_lock 

```C++
int32_t mpsk_rx_tlm_t::id_lock;
```



"&lt;prefix&gt;.lock" — carrier lock EMA 


        

<hr>



### variable id\_locked 

```C++
int32_t mpsk_rx_tlm_t::id_locked;
```



"&lt;prefix&gt;.car.locked" — lockdet flag 
 


        

<hr>



### variable id\_nco 

```C++
int32_t mpsk_rx_tlm_t::id_nco;
```



"&lt;prefix&gt;.car.nco" — the SUM that actually drives the LO 
 


        

<hr>



### variable id\_tracking 

```C++
int32_t mpsk_rx_tlm_t::id_tracking;
```



"&lt;prefix&gt;.tracking" — handover 0/1 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/mpsk_receiver/mpsk_rx_loops.h`

