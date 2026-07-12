

# Struct dp\_tlm\_probe\_t



[**ClassList**](annotated.md) **>** [**dp\_tlm\_probe\_t**](structdp__tlm__probe__t.md)



_Per-probe registry entry: name, decimation and accounting._ [More...](#detailed-description)

* `#include <telemetry.h>`





















## Public Attributes

| Type | Name |
| ---: | :--- |
|  uint32\_t | [**decim**](#variable-decim)  <br> |
|  uint64\_t | [**emitted**](#variable-emitted)  <br> |
|  char | [**name**](#variable-name)  <br> |
|  uint32\_t | [**phase**](#variable-phase)  <br> |












































## Detailed Description


`phase` counts events between emits and is producer-owned (hot path); `emitted` counts records actually written (post-decimation, post-drop), so a consumer can reconcile losses against the ring's dropped counter. 


    
## Public Attributes Documentation




### variable decim 

```C++
uint32_t dp_tlm_probe_t::decim;
```



Emit every decim-th event, &gt;= 1. 


        

<hr>



### variable emitted 

```C++
uint64_t dp_tlm_probe_t::emitted;
```



Records written into the ring. 


        

<hr>



### variable name 

```C++
char dp_tlm_probe_t::name[DP_TLM_NAME_MAX];
```



e.g. "agc.gain\_db". 


        

<hr>



### variable phase 

```C++
uint32_t dp_tlm_probe_t::phase;
```



Producer-owned event counter. 


        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/telemetry/telemetry.h`

