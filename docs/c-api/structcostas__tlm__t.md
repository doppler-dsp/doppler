

# Struct costas\_tlm\_t



[**ClassList**](annotated.md) **>** [**costas\_tlm\_t**](structcostas__tlm__t.md)



_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per symbol. Zeroed in state blobs and preserved across set\_state (DP\_DEFINE\_POD\_STATE\_TLM)._ 

* `#include <costas_core.h>`





















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
dp_tlm_t* costas_tlm_t::ctx;
```



NULL = detached 


        

<hr>



### variable id\_e 

```C++
int32_t costas_tlm_t::id_e;
```



"&lt;prefix&gt;.e" — PLL discriminator 


        

<hr>



### variable id\_freq 

```C++
int32_t costas_tlm_t::id_freq;
```



"&lt;prefix&gt;.freq" — tracked NCO freq 


        

<hr>



### variable id\_lock 

```C++
int32_t costas_tlm_t::id_lock;
```



"&lt;prefix&gt;.lock" — lock-metric EMA 


        

<hr>



### variable id\_locked 

```C++
int32_t costas_tlm_t::id_locked;
```



"&lt;prefix&gt;.locked" — lockdet flag 0/1 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/costas/costas_core.h`

