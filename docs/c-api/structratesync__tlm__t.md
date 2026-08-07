

# Struct ratesync\_tlm\_t



[**ClassList**](annotated.md) **>** [**ratesync\_tlm\_t**](structratesync__tlm__t.md)



_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then one predicted-not-taken branch per recovered symbol._ 

* `#include <ratesync_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_ctrl**](#variable-id_ctrl)  <br> |
|  int32\_t | [**id\_e**](#variable-id_e)  <br> |
|  int32\_t | [**id\_lock**](#variable-id_lock)  <br> |
|  int32\_t | [**id\_locked**](#variable-id_locked)  <br> |
|  int32\_t | [**id\_mu**](#variable-id_mu)  <br> |
|  int32\_t | [**id\_rate**](#variable-id_rate)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* ratesync_tlm_t::ctx;
```



NULL = detached 
 


        

<hr>



### variable id\_ctrl 

```C++
int32_t ratesync_tlm_t::id_ctrl;
```



"&lt;prefix&gt;.ctrl" — loop control 
 


        

<hr>



### variable id\_e 

```C++
int32_t ratesync_tlm_t::id_e;
```



"&lt;prefix&gt;.e" — normalised TED error 


        

<hr>



### variable id\_lock 

```C++
int32_t ratesync_tlm_t::id_lock;
```



"&lt;prefix&gt;.lock" — lock\_signal mean 
 


        

<hr>



### variable id\_locked 

```C++
int32_t ratesync_tlm_t::id_locked;
```



"&lt;prefix&gt;.locked" — lockdet flag 
 


        

<hr>



### variable id\_mu 

```C++
int32_t ratesync_tlm_t::id_mu;
```



"&lt;prefix&gt;.mu" — timing NCO phase 
 


        

<hr>



### variable id\_rate 

```C++
int32_t ratesync_tlm_t::id_rate;
```



"&lt;prefix&gt;.rate" — tracked samples/sym 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/ratesync/ratesync_core.h`

