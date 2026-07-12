

# File tlm\_sink.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**tlm\_sink.h**](tlm__sink_8h.md)

[Go to the source code of this file](tlm__sink_8h_source.md)

_NATS PUB sink for telemetry records._ [More...](#detailed-description)

* `#include "telemetry/telemetry.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_tlm\_sink | [**dp\_tlm\_sink\_t**](#typedef-dp_tlm_sink_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_tlm\_sink\_close**](#function-dp_tlm_sink_close) ([**dp\_tlm\_sink\_t**](tlm__sink_8h.md#typedef-dp_tlm_sink_t) \* sink) <br>_Close the sink and destroy the publisher._  |
|  [**dp\_tlm\_sink\_t**](tlm__sink_8h.md#typedef-dp_tlm_sink_t) \* | [**dp\_tlm\_sink\_open**](#function-dp_tlm_sink_open) (const char \* endpoint) <br>_Open a telemetry sink (PUB) bound to a NATS subject._  |
|  int | [**dp\_tlm\_sink\_pump**](#function-dp_tlm_sink_pump) ([**dp\_tlm\_sink\_t**](tlm__sink_8h.md#typedef-dp_tlm_sink_t) \* sink, [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) \* tlm) <br>_Drain every available record from_ `tlm` _and publish them._ |
|  uint64\_t | [**dp\_tlm\_sink\_sent**](#function-dp_tlm_sink_sent) (const [**dp\_tlm\_sink\_t**](tlm__sink_8h.md#typedef-dp_tlm_sink_t) \* sink) <br>_Total records published since open._  |




























## Detailed Description


Drains a [**dp\_tlm\_t**](telemetry_8h.md#typedef-dp_tlm_t) record ring ([**telemetry/telemetry.h**](telemetry_8h.md)) to a NATS subject using doppler's `dp_pub_*` wire layer: each pump publishes the available records as TLM16 frames (SIGS header, sample\_type = TLM16, num\_samples = record count, payload = packed 16-byte [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md)). A `dp_sub_*` receiver on the subject gets the same structured rows the in-process Python `Telemetry.read()` returns.


The implementation lives in the optional `libdoppler_stream` component (it publishes through the vendored nats.c client) — link `doppler::stream` alongside the core to use it. Unlike [**wfm\_sink.h**](wfm__sink_8h.md) there is no weak-stub seam: nothing in the core references these symbols, so a consumer that doesn't link the stream component simply doesn't call them.


Threading: the pump is a CONSUMER of the SPSC telemetry ring — run it on the (single) consumer thread, never the DSP producer thread. It is non-blocking on the ring side (drains what is available and returns) and lossy end-to-end by design: a record dropped by the ring (overrun) or a failed publish is gone; `dp_tlm_dropped()` / the pump's return value make the losses visible.


Lifecycle: dp\_tlm\_sink\_open -&gt; dp\_tlm\_sink\_pump\* -&gt; dp\_tlm\_sink\_close



```C++
dp_tlm_sink_t *sink = dp_tlm_sink_open ("nats://127.0.0.1:4222/tlm");
for (;;)                      // consumer-thread loop
  {
    int n = dp_tlm_sink_pump (sink, tlm);
    if (n < 0)
      break;                  // publish failure (broker gone)
    usleep (10000);           // pace to taste; pump is non-blocking
  }
dp_tlm_sink_close (sink);
```
 


    
## Public Types Documentation




### typedef dp\_tlm\_sink\_t 

```C++
typedef struct dp_tlm_sink dp_tlm_sink_t;
```



Opaque telemetry sink (a [**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) publisher + a sent counter). 


        

<hr>
## Public Functions Documentation




### function dp\_tlm\_sink\_close 

_Close the sink and destroy the publisher._ 
```C++
void dp_tlm_sink_close (
    dp_tlm_sink_t * sink
) 
```





**Parameters:**


* `sink` May be NULL. 




        

<hr>



### function dp\_tlm\_sink\_open 

_Open a telemetry sink (PUB) bound to a NATS subject._ 
```C++
dp_tlm_sink_t * dp_tlm_sink_open (
    const char * endpoint
) 
```





**Parameters:**


* `endpoint` Endpoint, e.g. "nats://127.0.0.1:4222/tlm". 



**Returns:**

Sink handle, or NULL on publisher-create failure. 




**Note:**

Caller must [**dp\_tlm\_sink\_close()**](tlm__sink_8h.md#function-dp_tlm_sink_close) when done. 





        

<hr>



### function dp\_tlm\_sink\_pump 

_Drain every available record from_ `tlm` _and publish them._
```C++
int dp_tlm_sink_pump (
    dp_tlm_sink_t * sink,
    dp_tlm_t * tlm
) 
```



Reads the ring in batches and publishes one TLM16 frame per batch until the ring is empty. Non-blocking on the ring; the NATS publish is fire-and-forget fan-out (PUB). On a publish failure the records already drained from the ring for that frame are lost (telemetry is lossy end-to-end by design) and the error is returned.




**Parameters:**


* `sink` Sink handle. Must be non-NULL. 
* `tlm` Telemetry context to drain. Must be non-NULL. 



**Returns:**

Number of records published (&gt;= 0), or a negative DP\_ERR\_\* code on a publish failure. 





        

<hr>



### function dp\_tlm\_sink\_sent 

_Total records published since open._ 
```C++
uint64_t dp_tlm_sink_sent (
    const dp_tlm_sink_t * sink
) 
```





**Parameters:**


* `sink` Must be non-NULL. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/telemetry/tlm_sink.h`

