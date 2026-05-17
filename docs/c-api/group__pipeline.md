

# Group pipeline



[**Modules**](modules.md) **>** [**pipeline**](group__pipeline.md)



[More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_pull**](group__types.md#typedef-dp_pull) \* | [**dp\_pull\_create**](#function-dp_pull_create) (const char \* endpoint) <br>_Create a Pull socket and connect to_ `endpoint` _._ |
|  void | [**dp\_pull\_destroy**](#function-dp_pull_destroy) ([**dp\_pull**](group__types.md#typedef-dp_pull) \* ctx) <br>_Destroy a Pull context and release all resources._  |
|  int | [**dp\_pull\_recv**](#function-dp_pull_recv) ([**dp\_pull**](group__types.md#typedef-dp_pull) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, [**dp\_header\_t**](structdp__header__t.md) \* header) <br>_Receive one frame from a Pull socket (zero-copy)._  |
|  void | [**dp\_pull\_set\_timeout**](#function-dp_pull_set_timeout) ([**dp\_pull**](group__types.md#typedef-dp_pull) \* ctx, int timeout\_ms) <br>_Set receive timeout for a Pull socket._  |
|  [**dp\_push**](group__types.md#typedef-dp_push) \* | [**dp\_push\_create**](#function-dp_push_create) (const char \* endpoint, [**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) sample\_type) <br>_Create a Push socket and bind to_ `endpoint` _._ |
|  void | [**dp\_push\_destroy**](#function-dp_push_destroy) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx) <br>_Destroy a Push context and release all resources._  |
|  int | [**dp\_push\_send\_cf128**](#function-dp_push_send_cf128) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const long double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF128 samples via a Push socket._  |
|  int | [**dp\_push\_send\_cf32**](#function-dp_push_send_cf32) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const float \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF32 samples via a Push socket._  |
|  int | [**dp\_push\_send\_cf64**](#function-dp_push_send_cf64) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF64 samples via a Push socket._  |
|  int | [**dp\_push\_send\_ci16**](#function-dp_push_send_ci16) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const int16\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI16 samples via a Push socket._  |
|  int | [**dp\_push\_send\_ci32**](#function-dp_push_send_ci32) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const int32\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI32 samples via a Push socket._  |
|  int | [**dp\_push\_send\_ci8**](#function-dp_push_send_ci8) ([**dp\_push**](group__types.md#typedef-dp_push) \* ctx, const int8\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI8 samples via a Push socket._  |




























## Detailed Description


Push sockets distribute work across all connected Pull workers in a round-robin fashion. Unlike PUB/SUB, each frame is delivered to exactly one Pull consumer. 


    
## Public Functions Documentation




### function dp\_pull\_create 

_Create a Pull socket and connect to_ `endpoint` _._
```
dp_pull * dp_pull_create (
    const char * endpoint
) 
```





**Parameters:**


* `endpoint` ZMQ endpoint to connect to, e.g. `"tcp://localhost:5556"`. 



**Returns:**

Non-NULL context on success, NULL on failure. 





        

<hr>



### function dp\_pull\_destroy 

_Destroy a Pull context and release all resources._ 
```
void dp_pull_destroy (
    dp_pull * ctx
) 
```





**Parameters:**


* `ctx` Pull context (may be NULL). 




        

<hr>



### function dp\_pull\_recv 

_Receive one frame from a Pull socket (zero-copy)._ 
```
int dp_pull_recv (
    dp_pull * ctx,
    dp_msg_t ** msg,
    dp_header_t * header
) 
```



On success, `*msg` is set to a message handle whose data buffer is valid until [**dp\_msg\_free()**](group__msg.md#function-dp_msg_free) is called. Use [**dp\_msg\_data()**](group__msg.md#function-dp_msg_data) to access the sample pointer.




**Parameters:**


* `ctx` Subscriber context. 
* [**Message handle**](group__msg.md) Set to a zero-copy message handle. 
* `header` Set to the frame metadata. 



**Returns:**

DP\_OK on success, DP\_ERR\_TIMEOUT on timeout, negative on error.    





        

<hr>



### function dp\_pull\_set\_timeout 

_Set receive timeout for a Pull socket._ 
```
void dp_pull_set_timeout (
    dp_pull * ctx,
    int timeout_ms
) 
```





**Parameters:**


* `ctx` Pull context. 
* `timeout_ms` Timeout in milliseconds (-1 = infinite, 0 = non-blocking). 




        

<hr>



### function dp\_push\_create 

_Create a Push socket and bind to_ `endpoint` _._
```
dp_push * dp_push_create (
    const char * endpoint,
    dp_sample_type_t sample_type
) 
```





**Parameters:**


* `endpoint` ZMQ endpoint to bind, e.g. `"tcp://\*:5556"`. 
* `sample_type` Sample format that will be sent. 



**Returns:**

Non-NULL context on success, NULL on failure. 





        

<hr>



### function dp\_push\_destroy 

_Destroy a Push context and release all resources._ 
```
void dp_push_destroy (
    dp_push * ctx
) 
```





**Parameters:**


* `ctx` Push context (may be NULL). 




        

<hr>



### function dp\_push\_send\_cf128 

_Send CF128 samples via a Push socket._ 
```
int dp_push_send_cf128 (
    dp_push * ctx,
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



### function dp\_push\_send\_cf32 

_Send CF32 samples via a Push socket._ 
```
int dp_push_send_cf32 (
    dp_push * ctx,
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



### function dp\_push\_send\_cf64 

_Send CF64 samples via a Push socket._ 
```
int dp_push_send_cf64 (
    dp_push * ctx,
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



### function dp\_push\_send\_ci16 

_Send CI16 samples via a Push socket._ 
```
int dp_push_send_ci16 (
    dp_push * ctx,
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



### function dp\_push\_send\_ci32 

_Send CI32 samples via a Push socket._ 
```
int dp_push_send_ci32 (
    dp_push * ctx,
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



### function dp\_push\_send\_ci8 

_Send CI8 samples via a Push socket._ 
```
int dp_push_send_ci8 (
    dp_push * ctx,
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

------------------------------


