

# File wfm\_sink.h



[**FileList**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_sink.h**](wfm__sink_8h.md)

[Go to the source code of this file](wfm__sink_8h_source.md)

_NATS PUB sink for generated IQ (Phase B)._ [More...](#detailed-description)

* `#include "doppler/clib_common.h"`

















## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct wfm\_stream\_sink | [**wfm\_stream\_sink\_t**](#typedef-wfm_stream_sink_t)  <br> |




















## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_wfm\_stream\_sink\_available**](#function-dp_wfm_stream_sink_available) (void) <br>_1 if the real stream sink (libdoppler\_stream) is linked, else 0 (the pure-C core links only the weak no-op stubs). wfmgen checks this before the_ `--output nats://` _path._ |
|  double | [**dp\_wfm\_stream\_sink\_clip\_fraction**](#function-dp_wfm_stream_sink_clip_fraction) (const [**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink) <br>_Fraction (0..1) of integer I/Q components that saturated._  |
|  void | [**dp\_wfm\_stream\_sink\_close**](#function-dp_wfm_stream_sink_close) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink) <br>_Close the sink and destroy the publisher._  |
|  [**DP\_NODISCARD**](clib__common_8h.md#define-dp_nodiscard) int | [**dp\_wfm\_stream\_sink\_drain**](#function-dp_wfm_stream_sink_drain) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink, int timeout\_ms) <br>_Let everything already sent reach the server, then stop._  |
|  [**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* | [**dp\_wfm\_stream\_sink\_open**](#function-dp_wfm_stream_sink_open) (const char \* endpoint, int sample\_type) <br>_Open a stream sink (PUB) bound to a NATS subject._  |
|  double | [**dp\_wfm\_stream\_sink\_peak**](#function-dp_wfm_stream_sink_peak) (const [**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink) <br>_Largest per-axis magnitude seen on an integer path._  |
|  [**DP\_NODISCARD**](clib__common_8h.md#define-dp_nodiscard) int | [**dp\_wfm\_stream\_sink\_send**](#function-dp_wfm_stream_sink_send) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink, const float \_Complex \* iq, size\_t n, double fs, double fc) <br>_Convert a cf32 block to the wire type and publish it._  |
|  [**DP\_NODISCARD**](clib__common_8h.md#define-dp_nodiscard) int | [**dp\_wfm\_stream\_sink\_send\_eos**](#function-dp_wfm_stream_sink_send_eos) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink) <br>_Tell subscribers this stream has ended._  |
|  void | [**dp\_wfm\_stream\_sink\_set\_gain**](#function-dp_wfm_stream_sink_set_gain) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink, double gain) <br>_Set the output gain applied to every sample before conversion._  |
|  void | [**dp\_wfm\_stream\_sink\_track\_clipping**](#function-dp_wfm_stream_sink_track_clipping) ([**wfm\_stream\_sink\_t**](wfm__sink_8h.md#typedef-wfm_stream_sink_t) \* sink, int on) <br>_Enable or disable the per-component clip counter._  |




























## Detailed Description


Streams cf32 blocks (from synth or the composer) to a NATS subject using doppler's `dp_pub_*` wire layer (`dp_header_t`, magic "DPSTREAM"), converting to the requested wire sample type per block. This is the `--output nats://…` destination; a `dp_sub_*` receiver (e.g. native/examples/spectrum\_analyzer) reads the stream.


Lifecycle: dp\_wfm\_stream\_sink\_open -&gt; dp\_wfm\_stream\_sink\_send\* -&gt; dp\_wfm\_stream\_sink\_drain -&gt; dp\_wfm\_stream\_sink\_close



```C++
wfm_stream_sink_t *s = dp_wfm_stream_sink_open("nats://127.0.0.1:4222/iq", 3); // ci16
if (dp_wfm_stream_sink_send(s, iq, 4096, 1e6, 2.4e9) != 0)
  fprintf(stderr, "block refused: it never left this process\n");
if (dp_wfm_stream_sink_drain(s, 0) != 0)
  fprintf(stderr, "the tail may not have reached the server\n");
dp_wfm_stream_sink_close(s);
```
 


    
## Public Types Documentation




### typedef wfm\_stream\_sink\_t 

```C++
typedef struct wfm_stream_sink wfm_stream_sink_t;
```



Opaque stream sink. 


        

<hr>
## Public Functions Documentation




### function dp\_wfm\_stream\_sink\_available 

_1 if the real stream sink (libdoppler\_stream) is linked, else 0 (the pure-C core links only the weak no-op stubs). wfmgen checks this before the_ `--output nats://` _path._
```C++
int dp_wfm_stream_sink_available (
    void
) 
```





**Returns:**

1 when the real sink is linked, 0 when only the stubs are. 





        

<hr>



### function dp\_wfm\_stream\_sink\_clip\_fraction 

_Fraction (0..1) of integer I/Q components that saturated._ 
```C++
double dp_wfm_stream_sink_clip_fraction (
    const wfm_stream_sink_t * sink
) 
```



Clips counted while [**dp\_wfm\_stream\_sink\_track\_clipping()**](wfm__sink_8h.md#function-dp_wfm_stream_sink_track_clipping) is on, divided by the integer-path I and Q components sent while it was on. The generated StreamSink handle binds peak/clip\_fraction directly as per-field getters (jm#320), so no stats-snapshot struct shim is needed.




**Parameters:**


* `sink` Sink, or NULL. 



**Returns:**

The fraction; 0.0 for NULL, before any integer-path send, when tracking was never on, or in the stub build. 





        

<hr>



### function dp\_wfm\_stream\_sink\_close 

_Close the sink and destroy the publisher._ 
```C++
void dp_wfm_stream_sink_close (
    wfm_stream_sink_t * sink
) 
```



Best-effort on its own: the client flushes what is still buffered for at most 500 ms and reports nothing, so a tail it cannot push out in that time is dropped silently. Call [**dp\_wfm\_stream\_sink\_drain()**](wfm__sink_8h.md#function-dp_wfm_stream_sink_drain) first on any run whose tail matters; close is then just the free.




**Parameters:**


* `sink` May be NULL. 




        

<hr>



### function dp\_wfm\_stream\_sink\_drain 

_Let everything already sent reach the server, then stop._ 
```C++
DP_NODISCARD int dp_wfm_stream_sink_drain (
    wfm_stream_sink_t * sink,
    int timeout_ms
) 
```



A send hands a block to the NATS client and returns; the client writes it in the background. So "send returned" is not "the server
has it", and closing without asking relies on the client's own best-effort flush  capped at 500 ms, with no way to report failure, so a backlog that cannot clear in half a second is dropped silently.


Call this before closing the sink on any run whose tail matters. After it returns the sink is finished: close it next, which is then just the free.




**Parameters:**


* `sink` Sink; NULL is DP\_OK (nothing was buffered). 
* `timeout_ms` Budget; &lt;= 0 uses the stream layer's 5 s default. 



**Returns:**

DP\_OK once drained, or the stream layer's error  DP\_ERR\_TIMEOUT if the budget ran out with the drain still in progress, in which case the sink is still safe to close. 





        

<hr>



### function dp\_wfm\_stream\_sink\_open 

_Open a stream sink (PUB) bound to a NATS subject._ 
```C++
wfm_stream_sink_t * dp_wfm_stream_sink_open (
    const char * endpoint,
    int sample_type
) 
```





**Parameters:**


* `endpoint` Endpoint, e.g. "nats://127.0.0.1:4222/iq". 
* `sample_type` Wire type (wavegen order): 0 cf32, 1 cf64, 2 ci32, 3 ci16, 4 ci8. Integer types use full-scale ±1.0. 



**Returns:**

Sink handle, or NULL on bad type / publisher-create failure. 




**Note:**

Caller must [**dp\_wfm\_stream\_sink\_close()**](wfm__sink_8h.md#function-dp_wfm_stream_sink_close) when done. 





        

<hr>



### function dp\_wfm\_stream\_sink\_peak 

_Largest per-axis magnitude seen on an integer path._ 
```C++
double dp_wfm_stream_sink_peak (
    const wfm_stream_sink_t * sink
) 
```



Measured after the gain and before saturation, relative to full scale 1.0, over everything sent since open (ci32/ci16/ci8 only; cf32/cf64 do not update it). A value &gt; 1.0 means the stream clipped; peak\_dBFS = 20\*log10(peak).




**Parameters:**


* `sink` Sink, or NULL. 



**Returns:**

The peak, or 0.0 for NULL, before any integer-path send, or in the stub build. 





        

<hr>



### function dp\_wfm\_stream\_sink\_send 

_Convert a cf32 block to the wire type and publish it._ 
```C++
DP_NODISCARD int dp_wfm_stream_sink_send (
    wfm_stream_sink_t * sink,
    const float _Complex * iq,
    size_t n,
    double fs,
    double fc
) 
```



A refused block never left this process: the client refuses publishes while a reconnect has 8 MiB pending, and a drained sink refuses outright. The stream goes on, so the next block may go; a caller that keeps sending counts the refusals and reports them, as wfmgen does (#2011).




**Parameters:**


* `sink` The sink handle. 
* `iq` Complex-float samples. 
* `n` Complex sample count. 
* `fs` Sample rate (Hz), carried in the wire header. 
* `fc` Center frequency (Hz), carried in the wire header. 



**Returns:**

0 on success, non-zero on a refused send or an allocation error. 





        

<hr>



### function dp\_wfm\_stream\_sink\_send\_eos 

_Tell subscribers this stream has ended._ 
```C++
DP_NODISCARD int dp_wfm_stream_sink_send_eos (
    wfm_stream_sink_t * sink
) 
```



Publishes an end-of-stream frame, so a consumer learns the sender finished rather than inferring it from silence. Send it BEFORE draining: a drain cannot be reversed and refuses sends once it reaches its publish-flushing phase.




**Parameters:**


* `sink` Sink; NULL is DP\_OK (there is no stream to end). 



**Returns:**

DP\_OK, or the stream layer's error. 





        

<hr>



### function dp\_wfm\_stream\_sink\_set\_gain 

_Set the output gain applied to every sample before conversion._ 
```C++
void dp_wfm_stream_sink_set_gain (
    wfm_stream_sink_t * sink,
    double gain
) 
```



Linear; default 1.0. For headroom H dB pass 10^(-H/20). The gain scales all wire types, cf32 included; at exactly 1.0 cf32 is sent unscaled (the direct, byte-identical path). The stub build ignores it.




**Parameters:**


* `sink` Sink; NULL is a no-op. 
* `gain` Linear amplitude gain (stored as float). 




        

<hr>



### function dp\_wfm\_stream\_sink\_track\_clipping 

_Enable or disable the per-component clip counter._ 
```C++
void dp_wfm_stream_sink_track_clipping (
    wfm_stream_sink_t * sink,
    int on
) 
```



Off by default; the peak is tracked regardless. Only components sent while tracking is on are counted, in both the clips and the [**dp\_wfm\_stream\_sink\_clip\_fraction()**](wfm__sink_8h.md#function-dp_wfm_stream_sink_clip_fraction) denominator, so enabling it mid-stream measures only what follows.




**Parameters:**


* `sink` Sink; NULL is a no-op. 
* `on` Non-zero to count clips, 0 to stop counting. 




        

<hr>

------------------------------
The documentation for this class was generated from the following file `native/inc/doppler/wfm/wfm_sink.h`

