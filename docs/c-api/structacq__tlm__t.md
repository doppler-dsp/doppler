

# Struct acq\_tlm\_t



[**ClassList**](annotated.md) **>** [**acq\_tlm\_t**](structacq__tlm__t.md)



_Telemetry attachment: a borrowed context + this engine's probe ids (design §2.4). NULL ctx (the default) means detached — the one probe site is then a single predicted-not-taken branch per decided dwell. Never in a state blob; preserved across_ [_**acq\_set\_state()**_](acq__core_8h.md#function-acq_set_state) _like the borrowed code._

* `#include <acq_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_t) \* | [**ctx**](#variable-ctx)  <br> |
|  int32\_t | [**id\_col**](#variable-id_col)  <br> |
|  int32\_t | [**id\_conc**](#variable-id_conc)  <br> |
|  int32\_t | [**id\_gate**](#variable-id_gate)  <br> |
|  int32\_t | [**id\_hit**](#variable-id_hit)  <br> |
|  int32\_t | [**id\_n\_held**](#variable-id_n_held)  <br> |
|  int32\_t | [**id\_n\_peaks**](#variable-id_n_peaks)  <br> |
|  int32\_t | [**id\_noise**](#variable-id_noise)  <br> |
|  int32\_t | [**id\_peak**](#variable-id_peak)  <br> |
|  int32\_t | [**id\_row**](#variable-id_row)  <br> |
|  int32\_t | [**id\_stat**](#variable-id_stat)  <br> |












































## Public Attributes Documentation




### variable ctx 

```C++
dp_tlm_t* acq_tlm_t::ctx;
```



NULL = detached 
 


        

<hr>



### variable id\_col 

```C++
int32_t acq_tlm_t::id_col;
```



"&lt;prefix&gt;.col" — its code-phase column 
 


        

<hr>



### variable id\_conc 

```C++
int32_t acq_tlm_t::id_conc;
```



"&lt;prefix&gt;.conc" — the peak's concentration 
 


        

<hr>



### variable id\_gate 

```C++
int32_t acq_tlm_t::id_gate;
```



"&lt;prefix&gt;.gate" — the gate it was held to 
 


        

<hr>



### variable id\_hit 

```C++
int32_t acq_tlm_t::id_hit;
```



"&lt;prefix&gt;.hit" — 1 when the gate fired 
 


        

<hr>



### variable id\_n\_held 

```C++
int32_t acq_tlm_t::id_n_held;
```



"&lt;prefix&gt;.n\_held" — picks held as twins 
 


        

<hr>



### variable id\_n\_peaks 

```C++
int32_t acq_tlm_t::id_n_peaks;
```



"&lt;prefix&gt;.n\_peaks" — picks in the dwell 
 


        

<hr>



### variable id\_noise 

```C++
int32_t acq_tlm_t::id_noise;
```



"&lt;prefix&gt;.noise" — the CFAR reference 
 


        

<hr>



### variable id\_peak 

```C++
int32_t acq_tlm_t::id_peak;
```



"&lt;prefix&gt;.peak" — the strongest cell's value 


        

<hr>



### variable id\_row 

```C++
int32_t acq_tlm_t::id_row;
```



"&lt;prefix&gt;.row" — its native Doppler row 
 


        

<hr>



### variable id\_stat 

```C++
int32_t acq_tlm_t::id_stat;
```



"&lt;prefix&gt;.stat" — the dwell's test statistic 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/acq/acq_core.h`

