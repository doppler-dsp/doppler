

# Struct dll\_tlm\_t



[**ClassList**](annotated.md) **>** [**dll\_tlm\_t**](structdll__tlm__t.md)



_Telemetry attachment: a borrowed context + this object's probe ids. NULL ctx (the default) means detached — every probe site is then a single predicted-not-taken branch per code epoch. Zeroed in state blobs and preserved across set\_state (the hand-written triplet treats it like the borrowed_ `code` _)._

* `#include <dll_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_e**](#variable-id_e)  <br> |
|  int32\_t | [**id\_lock**](#variable-id_lock)  <br> |
|  int32\_t | [**id\_locked**](#variable-id_locked)  <br> |
|  int32\_t | [**id\_rate**](#variable-id_rate)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* dll_tlm_t::ctx;
```



NULL = detached 


        

<hr>



### variable id\_e 

```C++
int32_t dll_tlm_t::id_e;
```



"&lt;prefix&gt;.e" — E-L discriminator 


        

<hr>



### variable id\_lock 

```C++
int32_t dll_tlm_t::id_lock;
```



"&lt;prefix&gt;.lock" — CFAR lock statistic R 


        

<hr>



### variable id\_locked 

```C++
int32_t dll_tlm_t::id_locked;
```



"&lt;prefix&gt;.locked" — lockdet decision 0/1 


        

<hr>



### variable id\_rate 

```C++
int32_t dll_tlm_t::id_rate;
```



"&lt;prefix&gt;.rate" — tracked code rate 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dll/dll_core.h`

