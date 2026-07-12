

# Struct dp\_tlm



[**ClassList**](annotated.md) **>** [**dp\_tlm**](structdp__tlm.md)



_Telemetry context: probe registry + SPSC record ring._ [More...](#detailed-description)

* `#include <telemetry.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**n\_probes**](#variable-n_probes)  <br> |
|  uint64\_t | [**now**](#variable-now)  <br> |
|  [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md) | [**probes**](#variable-probes)  <br> |
|  dp\_tlmr\_t \* | [**ring**](#variable-ring)  <br> |












































## Detailed Description


Public (not opaque) because the emit path is inline; treat the fields as read-only outside telemetry\_core.c and dp\_tlm\_emit. 


    
## Public Attributes Documentation




### variable n\_probes 

```C++
uint32_t dp_tlm::n_probes;
```




<hr>



### variable now 

```C++
uint64_t dp_tlm::now;
```



Caller-stamped sample index for records. 


        

<hr>



### variable probes 

```C++
dp_tlm_probe_t dp_tlm::probes[DP_TLM_MAX_PROBES];
```




<hr>



### variable ring 

```C++
dp_tlmr_t* dp_tlm::ring;
```



Lock-free SPSC record ring. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/telemetry/telemetry.h`

