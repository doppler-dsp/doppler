

# Struct dp\_tlm



[**ClassList**](annotated.md) **>** [**dp\_tlm**](structdp__tlm.md)



_Telemetry context: probe registry + SPSC record ring._ [More...](#detailed-description)

* `#include <dp_tlm_core.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  [**dp\_tlm\_capture\_t**](dp__tlm__core_8h.md#typedef-dp_tlm_capture_t) \* | [**capture**](#variable-capture)  <br> |
|  int(\* | [**capture\_drain**](#variable-capture_drain)  <br> |
|  uint32\_t | [**n\_probes**](#variable-n_probes)  <br> |
|  uint64\_t | [**now**](#variable-now)  <br> |
|  [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md) | [**probes**](#variable-probes)  <br> |
|  dp\_tlmr\_t \* | [**ring**](#variable-ring)  <br> |












































## Detailed Description


Public (not opaque) because the emit path is inline; treat the fields as read-only outside dp\_tlm\_core.c and dp\_tlm\_emit.


`capture` is deliberately LAST: the emit hot path touches `ring`, `now` and `probes`, and appending here leaves their cache layout untouched. 


    
## Public Attributes Documentation




### variable capture 

```C++
dp_tlm_capture_t* dp_tlm::capture;
```



Open capture that dp\_tlm\_set\_now() drains through; NULL when none. 


        

<hr>



### variable capture\_drain 

```C++
int(* dp_tlm::capture_drain) (dp_tlm_capture_t *);
```



Boundary drain, registered by [**dp\_tlm\_capture\_open()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_open).


A function POINTER rather than a direct call, so this translation unit never references a capture symbol: the inline dp\_tlm\_set\_now() below is pulled into every TU that includes this header, and calling [**dp\_tlm\_capture\_block()**](dp__tlm__capture__core_8h.md#function-dp_tlm_capture_block) by name would make the capture a link-time dependency of everything  an inversion, since the capture depends on the ring and not the other way round. NULL when no capture is open. 


        

<hr>



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
The documentation for this class was generated from the following file `native/inc/dp_tlm/dp_tlm_core.h`

