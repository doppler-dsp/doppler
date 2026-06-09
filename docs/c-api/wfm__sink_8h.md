

# File wfm\_sink.h



[**FileList**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfmgen**](dir_2784f51dc2a964fe71c3814677da8805.md) **>** [**wfm\_sink.h**](wfm__sink_8h.md)

[Go to the source code of this file](wfm__sink_8h_source.md)

_ZMQ PUB sink for generated IQ (Phase B)._ [More...](#detailed-description)

* `#include "clib_common.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_zmq\_sink | [**wfm\_zmq\_sink\_t**](#typedef-wfm_zmq_sink_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**wfm\_zmq\_sink\_close**](#function-wfm_zmq_sink_close) ([**wfm\_zmq\_sink\_t**](wfm__sink_8h.md#typedef-wfm_zmq_sink_t) \* sink) <br>_Close the sink and destroy the publisher._  |
|  [**wfm\_zmq\_sink\_t**](wfm__sink_8h.md#typedef-wfm_zmq_sink_t) \* | [**wfm\_zmq\_sink\_open**](#function-wfm_zmq_sink_open) (const char \* endpoint, int sample\_type) <br>_Open a ZMQ PUB sink._  |
|  int | [**wfm\_zmq\_sink\_send**](#function-wfm_zmq_sink_send) ([**wfm\_zmq\_sink\_t**](wfm__sink_8h.md#typedef-wfm_zmq_sink_t) \* sink, const float \_Complex \* iq, size\_t n, double fs, double fc) <br>_Convert a cf32 block to the wire type and publish it._  |




























## Detailed Description


Streams cf32 blocks (from synth or the composer) to a ZMQ PUB endpoint using doppler's `dp_pub_*` wire layer (SIGS header, magic "SIGS"), converting to the requested wire sample type per block. This is the `--output zmq://…` destination; a `dp_sub_*` receiver (e.g. examples/c/spectrum\_analyzer) reads the stream.


Lifecycle: wfm\_zmq\_sink\_open -&gt; wfm\_zmq\_sink\_send\* -&gt; wfm\_zmq\_sink\_close



```C++
wfm_zmq_sink_t *s = wfm_zmq_sink_open("tcp://0.0.0.0:5555", 3); // ci16
wfm_zmq_sink_send(s, iq, 4096, 1e6, 2.4e9);
wfm_zmq_sink_close(s);
```
 


    
## Public Types Documentation




### typedef wfm\_zmq\_sink\_t 

```C++
typedef struct wfm_zmq_sink wfm_zmq_sink_t;
```



Opaque ZMQ sink. 


        

<hr>
## Public Functions Documentation




### function wfm\_zmq\_sink\_close 

_Close the sink and destroy the publisher._ 
```C++
void wfm_zmq_sink_close (
    wfm_zmq_sink_t * sink
) 
```





**Parameters:**


* `sink` May be NULL. 




        

<hr>



### function wfm\_zmq\_sink\_open 

_Open a ZMQ PUB sink._ 
```C++
wfm_zmq_sink_t * wfm_zmq_sink_open (
    const char * endpoint,
    int sample_type
) 
```





**Parameters:**


* `endpoint` ZMQ bind endpoint, e.g. "tcp://0.0.0.0:5555". 
* `sample_type` Wire type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. Integer types use full-scale ±1.0. 



**Returns:**

Sink handle, or NULL on bad type / publisher-create failure. 




**Note:**

Caller must [**wfm\_zmq\_sink\_close()**](wfm__sink_8h.md#function-wfm_zmq_sink_close) when done. 





        

<hr>



### function wfm\_zmq\_sink\_send 

_Convert a cf32 block to the wire type and publish it._ 
```C++
int wfm_zmq_sink_send (
    wfm_zmq_sink_t * sink,
    const float _Complex * iq,
    size_t n,
    double fs,
    double fc
) 
```





**Parameters:**


* `sink` the sink handle. 
* `iq` Complex-float samples;
* `n` complex sample count. 
* `fs` sample rate (Hz);
* `fc` center frequency (Hz) — wire header. 



**Returns:**

0 on success, non-zero on a send/allocation error. 





        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/wfmgen/wfm_sink.h`

