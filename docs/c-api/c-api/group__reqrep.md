

# Group reqrep



[**Modules**](modules.md) **>** [**reqrep**](group__reqrep.md)



[More...](#detailed-description)






































## Public Functions

| Type | Name |
| ---: | :--- |
|  [**dp\_rep**](group__types.md#typedef-dp_rep) \* | [**dp\_rep\_create**](#function-dp_rep_create) (const char \* endpoint) <br>_Create a Replier socket and bind to_ `endpoint` _._ |
|  void | [**dp\_rep\_destroy**](#function-dp_rep_destroy) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx) <br>_Destroy a Replier context and release all resources._  |
|  int | [**dp\_rep\_recv**](#function-dp_rep_recv) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, size\_t \* size) <br>_Block until an incoming request arrives on the Replier (zero-copy)._  |
|  int | [**dp\_rep\_recv\_signal**](#function-dp_rep_recv_signal) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, [**dp\_header\_t**](structdp__header__t.md) \* header) <br>_Receive a signal frame request (zero-copy)._  |
|  int | [**dp\_rep\_send**](#function-dp_rep_send) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const void \* data, size\_t size) <br>_Send the reply to the most recent request._  |
|  int | [**dp\_rep\_send\_cf128**](#function-dp_rep_send_cf128) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const long double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF128 signal frame as a reply._  |
|  int | [**dp\_rep\_send\_cf32**](#function-dp_rep_send_cf32) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const float \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF32 signal frame as a reply._  |
|  int | [**dp\_rep\_send\_cf64**](#function-dp_rep_send_cf64) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF64 signal frame as a reply._  |
|  int | [**dp\_rep\_send\_ci16**](#function-dp_rep_send_ci16) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const int16\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI16 signal frame as a reply._  |
|  int | [**dp\_rep\_send\_ci32**](#function-dp_rep_send_ci32) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const int32\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI32 signal frame as a reply._  |
|  int | [**dp\_rep\_send\_ci8**](#function-dp_rep_send_ci8) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, const int8\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI8 signal frame as a reply._  |
|  void | [**dp\_rep\_set\_timeout**](#function-dp_rep_set_timeout) ([**dp\_rep**](group__types.md#typedef-dp_rep) \* ctx, int timeout\_ms) <br>_Set receive timeout for a Replier socket._  |
|  [**dp\_req**](group__types.md#typedef-dp_req) \* | [**dp\_req\_create**](#function-dp_req_create) (const char \* endpoint) <br>_Create a Requester socket and connect to_ `endpoint` _._ |
|  void | [**dp\_req\_destroy**](#function-dp_req_destroy) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx) <br>_Destroy a Requester context and release all resources._  |
|  int | [**dp\_req\_recv**](#function-dp_req_recv) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, size\_t \* size) <br>_Receive the reply to a previously sent request (zero-copy)._  |
|  int | [**dp\_req\_recv\_signal**](#function-dp_req_recv_signal) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, [**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \*\* msg, [**dp\_header\_t**](structdp__header__t.md) \* header) <br>_Receive a signal frame reply (zero-copy)._  |
|  int | [**dp\_req\_send**](#function-dp_req_send) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const void \* data, size\_t size) <br>_Send raw bytes as a request._  |
|  int | [**dp\_req\_send\_cf128**](#function-dp_req_send_cf128) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const long double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF128 signal frame as a request._  |
|  int | [**dp\_req\_send\_cf32**](#function-dp_req_send_cf32) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const float \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF32 signal frame as a request._  |
|  int | [**dp\_req\_send\_cf64**](#function-dp_req_send_cf64) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const double \_Complex \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CF64 signal frame as a request._  |
|  int | [**dp\_req\_send\_ci16**](#function-dp_req_send_ci16) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const int16\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI16 signal frame as a request._  |
|  int | [**dp\_req\_send\_ci32**](#function-dp_req_send_ci32) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const int32\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI32 signal frame as a request._  |
|  int | [**dp\_req\_send\_ci8**](#function-dp_req_send_ci8) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, const int8\_t \* samples, size\_t num\_samples, double sample\_rate, double center\_freq) <br>_Send CI8 signal frame as a request._  |
|  void | [**dp\_req\_set\_timeout**](#function-dp_req_set_timeout) ([**dp\_req**](group__types.md#typedef-dp_req) \* ctx, int timeout\_ms) <br>_Set receive timeout for a Requester socket._  |




























## Detailed Description


Strict synchronous request/reply. The Requester sends a message and must call recv before sending again. Useful for control plane messages (e.g. tuning commands, metadata queries) and signal-frame RPC.



## Public Functions Documentation




### function dp\_rep\_create

_Create a Replier socket and bind to_ `endpoint` _._
```
dp_rep * dp_rep_create (
    const char * endpoint
)
```





**Parameters:**


* `endpoint` ZMQ endpoint to bind, e.g. `"tcp://\*:5557"`.



**Returns:**

Non-NULL context on success, NULL on failure.







<hr>



### function dp\_rep\_destroy

_Destroy a Replier context and release all resources._
```
void dp_rep_destroy (
    dp_rep * ctx
)
```





**Parameters:**


* `ctx` Replier context (may be NULL).






<hr>



### function dp\_rep\_recv

_Block until an incoming request arrives on the Replier (zero-copy)._
```
int dp_rep_recv (
    dp_rep * ctx,
    dp_msg_t ** msg,
    size_t * size
)
```





**Parameters:**


* `ctx` Replier context.
* [**Message handle**](group__msg.md) Set to a zero-copy message handle.
* `size` Set to the request byte count.



**Returns:**

DP\_OK on success.







<hr>



### function dp\_rep\_recv\_signal

_Receive a signal frame request (zero-copy)._
```
int dp_rep_recv_signal (
    dp_rep * ctx,
    dp_msg_t ** msg,
    dp_header_t * header
)
```





**Parameters:**


* `ctx` Replier context.
* [**Message handle**](group__msg.md) Set to a zero-copy message handle.
* `header` Set to the frame metadata.



**Returns:**

DP\_OK on success, DP\_ERR\_TIMEOUT on timeout, negative on error.







<hr>



### function dp\_rep\_send

_Send the reply to the most recent request._
```
int dp_rep_send (
    dp_rep * ctx,
    const void * data,
    size_t size
)
```





**Parameters:**


* `ctx` Replier context.
* `data` Pointer to reply payload bytes.
* `size` Byte count.



**Returns:**

DP\_OK on success.







<hr>



### function dp\_rep\_send\_cf128

_Send CF128 signal frame as a reply._
```
int dp_rep_send_cf128 (
    dp_rep * ctx,
    const long double _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_send\_cf32

_Send CF32 signal frame as a reply._
```
int dp_rep_send_cf32 (
    dp_rep * ctx,
    const float _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_send\_cf64

_Send CF64 signal frame as a reply._
```
int dp_rep_send_cf64 (
    dp_rep * ctx,
    const double _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_send\_ci16

_Send CI16 signal frame as a reply._
```
int dp_rep_send_ci16 (
    dp_rep * ctx,
    const int16_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_send\_ci32

_Send CI32 signal frame as a reply._
```
int dp_rep_send_ci32 (
    dp_rep * ctx,
    const int32_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_send\_ci8

_Send CI8 signal frame as a reply._
```
int dp_rep_send_ci8 (
    dp_rep * ctx,
    const int8_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_rep\_set\_timeout

_Set receive timeout for a Replier socket._
```
void dp_rep_set_timeout (
    dp_rep * ctx,
    int timeout_ms
)
```





**Parameters:**


* `ctx` Replier context.
* `timeout_ms` Timeout in milliseconds (-1 = infinite).






<hr>



### function dp\_req\_create

_Create a Requester socket and connect to_ `endpoint` _._
```
dp_req * dp_req_create (
    const char * endpoint
)
```





**Parameters:**


* `endpoint` ZMQ endpoint to connect to, e.g. `"tcp://localhost:5557"`.



**Returns:**

Non-NULL context on success, NULL on failure.







<hr>



### function dp\_req\_destroy

_Destroy a Requester context and release all resources._
```
void dp_req_destroy (
    dp_req * ctx
)
```





**Parameters:**


* `ctx` Requester context (may be NULL).






<hr>



### function dp\_req\_recv

_Receive the reply to a previously sent request (zero-copy)._
```
int dp_req_recv (
    dp_req * ctx,
    dp_msg_t ** msg,
    size_t * size
)
```





**Parameters:**


* `ctx` Requester context.
* [**Message handle**](group__msg.md) Set to a zero-copy message handle.
* `size` Set to the reply byte count.



**Returns:**

DP\_OK on success.







<hr>



### function dp\_req\_recv\_signal

_Receive a signal frame reply (zero-copy)._
```
int dp_req_recv_signal (
    dp_req * ctx,
    dp_msg_t ** msg,
    dp_header_t * header
)
```





**Parameters:**


* `ctx` Requester context.
* [**Message handle**](group__msg.md) Set to a zero-copy message handle.
* `header` Set to the frame metadata.



**Returns:**

DP\_OK on success, DP\_ERR\_TIMEOUT on timeout, negative on error.







<hr>



### function dp\_req\_send

_Send raw bytes as a request._
```
int dp_req_send (
    dp_req * ctx,
    const void * data,
    size_t size
)
```





**Parameters:**


* `ctx` Requester context.
* `data` Pointer to payload bytes.
* `size` Byte count.



**Returns:**

DP\_OK on success.







<hr>



### function dp\_req\_send\_cf128

_Send CF128 signal frame as a request._
```
int dp_req_send_cf128 (
    dp_req * ctx,
    const long double _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_send\_cf32

_Send CF32 signal frame as a request._
```
int dp_req_send_cf32 (
    dp_req * ctx,
    const float _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_send\_cf64

_Send CF64 signal frame as a request._
```
int dp_req_send_cf64 (
    dp_req * ctx,
    const double _Complex * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_send\_ci16

_Send CI16 signal frame as a request._
```
int dp_req_send_ci16 (
    dp_req * ctx,
    const int16_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_send\_ci32

_Send CI32 signal frame as a request._
```
int dp_req_send_ci32 (
    dp_req * ctx,
    const int32_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_send\_ci8

_Send CI8 signal frame as a request._
```
int dp_req_send_ci8 (
    dp_req * ctx,
    const int8_t * samples,
    size_t num_samples,
    double sample_rate,
    double center_freq
)
```




<hr>



### function dp\_req\_set\_timeout

_Set receive timeout for a Requester socket._
```
void dp_req_set_timeout (
    dp_req * ctx,
    int timeout_ms
)
```





**Parameters:**


* `ctx` Requester context.
* `timeout_ms` Timeout in milliseconds (-1 = infinite).






<hr>

------------------------------
