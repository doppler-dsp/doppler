

# Group pubsub



[**Modules**](modules.md) **>** [**pubsub**](group__pubsub.md)



[More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* | [**dp\_pub\_create**](#function-dp_pub_create) (const char \* endpoint, [**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) sample\_type) <br>_Create a Publisher and connect to_ `endpoint` _._ |
|  void | [**dp\_pub\_destroy**](#function-dp_pub_destroy) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx) <br>_Destroy a Publisher context and release all resources._  |
|  int | [**dp\_pub\_send\_cf128**](#function-dp_pub_send_cf128) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const long double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CF128 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_cf32**](#function-dp_pub_send_cf32) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const float \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CF32 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_cf64**](#function-dp_pub_send_cf64) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CF64 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci16**](#function-dp_pub_send_ci16) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int16\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI16 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci32**](#function-dp_pub_send_ci32) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int32\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI32 samples via a Publisher._  |
|  int | [**dp\_pub\_send\_ci8**](#function-dp_pub_send_ci8) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, const int8\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send an array of CI8 samples via a Publisher._  |
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



### function dp\_pub\_send\_cf128 

_Send an array of CF128 samples via a Publisher._ 
```
int dp_pub_send_cf128 (
    dp_pub_t * ctx,
    const long double _Complex * samples,
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


