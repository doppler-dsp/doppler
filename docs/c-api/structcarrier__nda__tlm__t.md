

# Struct carrier\_nda\_tlm\_t



[**ClassList**](annotated.md) **>** [**carrier\_nda\_tlm\_t**](structcarrier__nda__tlm__t.md)



_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — the probe site is then a single predicted-not-taken branch per block loop. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._ 

* `#include <carrier_nda_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_e**](#variable-id_e)  <br> |
|  int32\_t | [**id\_freq**](#variable-id_freq)  <br> |
|  int32\_t | [**id\_lock**](#variable-id_lock)  <br> |
|  int32\_t | [**id\_locked**](#variable-id_locked)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* carrier_nda_tlm_t::ctx;
```



NULL = detached 


        

<hr>



### variable id\_e 

```C++
int32_t carrier_nda_tlm_t::id_e;
```



"&lt;prefix&gt;.e" — M-th-power phase error 


        

<hr>



### variable id\_freq 

```C++
int32_t carrier_nda_tlm_t::id_freq;
```



"&lt;prefix&gt;.freq" — tracked carrier freq 


        

<hr>



### variable id\_lock 

```C++
int32_t carrier_nda_tlm_t::id_lock;
```



"&lt;prefix&gt;.lock" — lock-signal EMA 


        

<hr>



### variable id\_locked 

```C++
int32_t carrier_nda_tlm_t::id_locked;
```



"&lt;prefix&gt;.locked" — lockdet flag 0/1 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/carrier_nda/carrier_nda_core.h`

