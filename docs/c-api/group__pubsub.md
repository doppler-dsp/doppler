

# Group pubsub



[**Modules**](modules.md) **>** [**pubsub**](group__pubsub.md)



[More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* | [**dp\_pub\_create**](#function-dp_pub_create) (const char \* endpoint, [**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) sample\_type) <br>_Create a Publisher and connect to_ `endpoint` _._ |
|  [**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* | [**dp\_pub\_create\_tlm**](#function-dp_pub_create_tlm) (const char \* endpoint) <br>_Create a Publisher that emits telemetry frames._  |
|  void | [**dp\_pub\_destroy**](#function-dp_pub_destroy) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx) <br>_Destroy a Publisher context and release all resources._  |
|  int | [**dp\_pub\_flush**](#function-dp_pub_flush) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, int timeout\_ms) <br>_Wait until the server has everything published so far._  |
|  int | [**dp\_pub\_send\_cf32**](#function-dp_pub_send_cf32) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const float \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CF32 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_cf64**](#function-dp_pub_send_cf64) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CF64 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci16**](#function-dp_pub_send_ci16) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int16\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI16 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci32**](#function-dp_pub_send_ci32) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int32\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI32 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci8**](#function-dp_pub_send_ci8) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int8\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI8 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_tlm16**](#function-dp_pub_send_tlm16) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const void \* records, size\_t num\_records, double sample\_rate, double center\_freq) <br>_Send an array of 16-byte telemetry records via a Publisher._  |
|  int | [**dp\_stream\_drain**](#function-dp_stream_drain) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, int timeout\_ms) <br>_Shut a context down gracefully: drain, then closed._  |
|  [**dp\_sub\_t**](group__types.md#typedef-dp_sub_t) \* | [**dp\_sub\_create**](#function-dp_sub_create) (const char \* endpoint) <br>_Create a Subscriber and connect to_ `endpoint` _._ |
|  void | [**dp\_sub\_destroy**](#function-dp_sub_destroy) ([**dp\_sub\_t**](group__types.md#typedef-dp_sub_t) \* ctx) <br>_Destroy a Subscriber context and release all resources._  |
|  int | [**dp\_sub\_recv**](#function-dp_sub_recv) ([**dp\_sub\_t**](group__types.md#typedef-dp_sub_t) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, [**dp\_header\_t**](structdp__header__t.md) \* header) <br>_Receive one frame from a Subscriber socket (zero-copy)._  |
|  void | [**dp\_sub\_set\_timeout**](#function-dp_sub_set_timeout) ([**dp\_sub\_t**](group__types.md#typedef-dp_sub_t) \* ctx, int timeout\_ms) <br>_Set receive timeout for a Subscriber socket._  |




























## Detailed Description


The Publisher publishes to a subject and fans out every message to all connected Subscribers. Subscribers subscribe and receive every frame published after they connect — a slow or absent subscriber simply misses frames (core NATS PUB/SUB has no queuing/replay). 


    
## Public Functions Documentation




### function dp\_pub\_create 

_Create a Publisher and connect to_ `endpoint` _._
```
dp_pub_t * dp_pub_create (
    const char * endpoint,
    dp_sample_type_t sample_type
) 
```





**Parameters:**


* `endpoint` NATS endpoint, e.g. `"nats://127.0.0.1:4222/iq"`. 
* `sample_type` Sample format that will be sent. 



**Returns:**

Non-NULL context on success, NULL on failure. 





        

<hr>



### function dp\_pub\_create\_tlm 

_Create a Publisher that emits telemetry frames._ 
```
dp_pub_t * dp_pub_create_tlm (
    const char * endpoint
) 
```



A separate constructor because DP\_KIND\_TLM is a frame kind rather than a sample format: there is no BLUE code to pass to [**dp\_pub\_create()**](group__pubsub.md#function-dp_pub_create), and a publisher that emits records does not also emit I/Q. Send with [**dp\_pub\_send\_tlm16()**](group__pubsub.md#function-dp_pub_send_tlm16).




**Parameters:**


* `endpoint` `nats://host:port/subject`. 



**Returns:**

Publisher handle, or NULL on failure. 





        

<hr>



### function dp\_pub\_destroy 

_Destroy a Publisher context and release all resources._ 
```
void dp_pub_destroy (
    dp_pub_t * ctx
) 
```





**Parameters:**


* `ctx` Publisher context (may be NULL). 




        

<hr>



### function dp\_pub\_flush 

_Wait until the server has everything published so far._ 
```
int dp_pub_flush (
    dp_pub_t * ctx,
    int timeout_ms
) 
```



`dp_pub_send_*` hands the frame to the client and returns; the client writes it in the background. That is what makes publishing fast, and it means "the send returned" is not "the server has it". This waits for a round trip, so when it returns DP\_OK everything published before it has arrived.


You do NOT need this before destroy: the NATS client flushes what is buffered when the connection closes. But it does so best-effort with a **500 ms cap and no way to report failure**, so a backlog that cannot drain in half a second is dropped silently — and on a link slower than loopback that is not a large backlog. Call this when losing the tail would matter, and you get a budget you chose and an answer you can act on.


It is also the only way to ask the question WITHOUT closing: a long-lived publisher that wants "everything up to here is on the
server" has nothing else to call. (PUSH does not need it — the JetStream publish is server-acked before it returns — and REQ/REP flush on every message already.)


**A drained shutdown does not need one.** [**dp\_stream\_drain()**](group__pubsub.md#function-dp_stream_drain) ends with this same flush as its final phase, so flush belongs at checkpoints a drain does not cover — confirming a batch is on the server before treating it as complete — not in front of a drain.




**Parameters:**


* `ctx` Any send-capable context (dp\_pub\_t / dp\_push\_t / dp\_req\_t / dp\_rep\_t are the same underlying type). 
* `timeout_ms` How long to wait; &lt;= 0 uses 2000 ms. 



**Returns:**

DP\_OK when the server has it, [**DP\_ERR\_TIMEOUT**](clib__common_8h.md#define-dp_err_timeout) if the budget ran out with data still pending, DP\_ERR\_INVALID for a NULL context. 





        

<hr>



### function dp\_pub\_send\_cf32 

_Send an array of CF32 samples via a Publisher._ 
```
int dp_pub_send_cf32 (
    dp_pub_t * ctx,
    const float _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
) 
```





**Parameters:**


* `ctx` Publisher context. 
* `samples` Interleaved int32\_t I/Q pairs; length 2×num\_samples. 
* `num_samples` Number of complex samples. 
* `sample_rate` Sample rate in Hz. 
* `center_freq` Centre frequency in Hz. 



**Returns:**

DP\_OK (0) on success, negative error code on failure.   





        

<hr>



### function dp\_pub\_send\_cf64 

_Send an array of CF64 samples via a Publisher._ 
```
int dp_pub_send_cf64 (
    dp_pub_t * ctx,
    const double _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
) 
```





**Parameters:**


* `ctx` Publisher context. 
* `samples` Interleaved int32\_t I/Q pairs; length 2×num\_samples. 
* `num_samples` Number of complex samples. 
* `sample_rate` Sample rate in Hz. 
* `center_freq` Centre frequency in Hz. 



**Returns:**

DP\_OK (0) on success, negative error code on failure.   





        

<hr>



### function dp\_pub\_send\_ci16 

_Send an array of CI16 samples via a Publisher._ 
```
int dp_pub_send_ci16 (
    dp_pub_t * ctx,
    const int16_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
) 
```





**Parameters:**


* `ctx` Publisher context. 
* `samples` Interleaved int32\_t I/Q pairs; length 2×num\_samples. 
* `num_samples` Number of complex samples. 
* `sample_rate` Sample rate in Hz. 
* `center_freq` Centre frequency in Hz. 



**Returns:**

DP\_OK (0) on success, negative error code on failure.   





        

<hr>



### function dp\_pub\_send\_ci32 

_Send an array of CI32 samples via a Publisher._ 
```
int dp_pub_send_ci32 (
    dp_pub_t * ctx,
    const int32_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
) 
```





**Parameters:**


* `ctx` Publisher context. 
* `samples` Interleaved int32\_t I/Q pairs; length 2×num\_samples. 
* `num_samples` Number of complex samples. 
* `sample_rate` Sample rate in Hz. 
* `center_freq` Centre frequency in Hz. 



**Returns:**

DP\_OK (0) on success, negative error code on failure. 





        

<hr>



### function dp\_pub\_send\_ci8 

_Send an array of CI8 samples via a Publisher._ 
```
int dp_pub_send_ci8 (
    dp_pub_t * ctx,
    const int8_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
) 
```





**Parameters:**


* `ctx` Publisher context. 
* `samples` Interleaved int32\_t I/Q pairs; length 2×num\_samples. 
* `num_samples` Number of complex samples. 
* `sample_rate` Sample rate in Hz. 
* `center_freq` Centre frequency in Hz. 



**Returns:**

DP\_OK (0) on success, negative error code on failure.   





        

<hr>



### function dp\_pub\_send\_tlm16 

_Send an array of 16-byte telemetry records via a Publisher._ 
```
int dp_pub_send_tlm16 (
    dp_pub_t * ctx,
    const void * records,
    size_t num_records,
    double sample_rate,
    double center_freq
) 
```



The payload is `num_records` packed [**dp\_tlm\_rec\_t**](structdp__tlm__rec__t.md) (see [**dp\_tlm/dp\_tlm\_core.h**](dp__tlm__core_8h.md)) — the header's num\_samples counts records and sample\_type is TLM16. Kept `const void *` so the wire layer stays decoupled from the telemetry component; the dp\_tlm\_sink\_\* helper ([**stream/tlm\_sink.h**](tlm__sink_8h.md)) is the intended caller.




**Parameters:**


* `ctx` Publisher context. 
* `records` Packed 16-byte records. 
* `num_records` Record count. 
* `sample_rate` Wire-header field; 0.0 if not meaningful. 
* `center_freq` Wire-header field; 0.0 if not meaningful. 



**Returns:**

DP\_OK (0) on success, negative error code on failure. 





        

<hr>



### function dp\_stream\_drain 

_Shut a context down gracefully: drain, then closed._ 
```
int dp_stream_drain (
    dp_pub_t * ctx,
    int timeout_ms
) 
```



The ordered shutdown, and the one a signal handler's exit path wants. The client stops accepting new deliveries, lets what is in flight finish, flushes everything pending, and then closes.


**It waits for the connection to reach CLOSED before returning**, and that is the part worth having in the library rather than in every caller: `natsConnection_Drain` returns immediately and does the work in the background, so a process that exits when it returns abandons exactly the work the drain was for. Getting that wrong looks like success.


Against [**dp\_pub\_flush()**](group__pubsub.md#function-dp_pub_flush): flush answers "does the server have what I
published", and the context keeps working afterwards. Drain answers "let everything finish, then stop", and the context is finished when it returns — call the matching `*_destroy` next, which is then just the free.


**Drain last, after your application has stopped producing.** A drain cannot be reversed, and a send issued while one is in progress is racing its phases: it may slip through while subscriptions drain, or be refused once the connection reaches its publish-flushing phase. Do not publish a "shutting down" notice after calling this and assume it went.


Because this waits for CLOSED, a single-threaded caller does not have to reason about that race: once it has returned, a send is refused with [**DP\_ERR\_CLOSED**](clib__common_8h.md#define-dp_err_closed), deterministically. The race is real only for a thread still publishing while another drains.


Size `timeout_ms` to the slowest thing the drain has to wait for, with margin: cutting a drain off mid-write every deploy is worse than waiting. doppler's own receive is synchronous — there is no message handler to finish — so the wait is dominated by flushing whatever is still buffered, and the 5 s default is generous for a link that is keeping up. A slow or congested link, or a large backlog, wants more.




**Parameters:**


* `ctx` Any context. 
* `timeout_ms` How long to wait for CLOSED; &lt;= 0 uses 5000 ms. 



**Returns:**

DP\_OK once closed, [**DP\_ERR\_TIMEOUT**](clib__common_8h.md#define-dp_err_timeout) if the budget ran out with the drain still in progress (the context is still safe to destroy), DP\_ERR\_INVALID for a NULL context. 





        

<hr>



### function dp\_sub\_create 

_Create a Subscriber and connect to_ `endpoint` _._
```
dp_sub_t * dp_sub_create (
    const char * endpoint
) 
```



Subscribes to all topics (empty topic filter).




**Parameters:**


* `endpoint` NATS endpoint, e.g. `"nats://127.0.0.1:4222/iq"`. 



**Returns:**

Non-NULL context on success, NULL on failure. 





        

<hr>



### function dp\_sub\_destroy 

_Destroy a Subscriber context and release all resources._ 
```
void dp_sub_destroy (
    dp_sub_t * ctx
) 
```





**Parameters:**


* `ctx` Subscriber context (may be NULL). 




        

<hr>



### function dp\_sub\_recv 

_Receive one frame from a Subscriber socket (zero-copy)._ 
```
int dp_sub_recv (
    dp_sub_t * ctx,
    dp_msg_t ** msg,
    dp_header_t * header
) 
```



On success, `*msg` is set to a message handle whose data buffer is valid until [**dp\_msg\_free()**](group__msg.md#function-dp_msg_free) is called. Use [**dp\_msg\_data()**](group__msg.md#function-dp_msg_data) to access the sample pointer.




**Parameters:**


* `ctx` Subscriber context. 
* `msg` Set to a zero-copy message handle. 
* `header` Set to the frame metadata. 



**Returns:**

DP\_OK on success, DP\_ERR\_TIMEOUT on timeout, negative on error. 





        

<hr>



### function dp\_sub\_set\_timeout 

_Set receive timeout for a Subscriber socket._ 
```
void dp_sub_set_timeout (
    dp_sub_t * ctx,
    int timeout_ms
) 
```





**Parameters:**


* `ctx` Subscriber context. 
* `timeout_ms` Timeout in milliseconds (-1 = infinite, 0 = non-blocking). 




        

<hr>

------------------------------


