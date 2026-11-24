

# Struct carrier\_acq\_tlm\_t



[**ClassList**](annotated.md) **>** [**carrier\_acq\_tlm\_t**](structcarrier__acq__tlm__t.md)



_Telemetry attachment: a borrowed context and this object's probe ids. NULL ctx (the default) means detached: every probe site is one predicted-not-taken branch per tested block. Not part of the state blob, and kept across reset() and set\_state()._ 

* `#include <carrier_acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_hz**](#variable-id_hz)  <br> |
|  int32\_t | [**id\_n**](#variable-id_n)  <br> |
|  int32\_t | [**id\_ready**](#variable-id_ready)  <br> |
|  int32\_t | [**id\_stat**](#variable-id_stat)  <br> |
|  int32\_t | [**id\_thr**](#variable-id_thr)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* carrier_acq_tlm_t::ctx;
```



NULL = detached. 
 


        

<hr>



### variable id\_hz 

```C++
int32_t carrier_acq_tlm_t::id_hz;
```



"&lt;prefix&gt;.hz"  the peak's frequency. 
 


        

<hr>



### variable id\_n 

```C++
int32_t carrier_acq_tlm_t::id_n;
```



"&lt;prefix&gt;.n"  blocks folded. 
 


        

<hr>



### variable id\_ready 

```C++
int32_t carrier_acq_tlm_t::id_ready;
```



"&lt;prefix&gt;.ready"  the decision, 0/1. 
 


        

<hr>



### variable id\_stat 

```C++
int32_t carrier_acq_tlm_t::id_stat;
```



"&lt;prefix&gt;.stat"  the test statistic. 
 


        

<hr>



### variable id\_thr 

```C++
int32_t carrier_acq_tlm_t::id_thr;
```



"&lt;prefix&gt;.thr"  the threshold it met. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/carrier_acq/carrier_acq_core.h`

