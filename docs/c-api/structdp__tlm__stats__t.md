

# Struct dp\_tlm\_stats\_t



[**ClassList**](annotated.md) **>** [**dp\_tlm\_stats\_t**](structdp__tlm__stats__t.md)



_Context-wide counters, snapshotted together._ [More...](#detailed-description)

* `#include <dp_tlm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  size\_t | [**capacity**](#variable-capacity)  <br> |
|  uint64\_t | [**dropped**](#variable-dropped)  <br> |
|  uint64\_t | [**emitted**](#variable-emitted)  <br> |
|  size\_t | [**probes**](#variable-probes)  <br> |












































## Detailed Description


A by-value record rather than a dict so the whole thing crosses a language boundary as one value. Per-probe detail is not in here on purpose: it is [**dp\_tlm\_probe\_name()**](dp__tlm__core_8h.md#function-dp_tlm_probe_name) + [**dp\_tlm\_emitted()**](dp__tlm__core_8h.md#function-dp_tlm_emitted), which stay the SSOT for it. 


    
## Public Attributes Documentation




### variable capacity 

```C++
size_t dp_tlm_stats_t::capacity;
```



Ring capacity in records. 
 


        

<hr>



### variable dropped 

```C++
uint64_t dp_tlm_stats_t::dropped;
```



Records lost to ring overrun (monotonic). 
 


        

<hr>



### variable emitted 

```C++
uint64_t dp_tlm_stats_t::emitted;
```



Records written, summed over every probe. 
 


        

<hr>



### variable probes 

```C++
size_t dp_tlm_stats_t::probes;
```



Registered probes. 
 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/dp_tlm/dp_tlm_core.h`

