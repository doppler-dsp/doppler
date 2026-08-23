

# Group msg



[**Modules**](modules.md) **>** [**msg**](group__msg.md)










































## Public Functions

| Type | Name |
| ---: | :--- |
|  double | [**dp\_mean\_power**](#function-dp_mean_power) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) format, const void \* data, size\_t n) <br>_Mean power of a complex sample block, normalised to full scale._  |
|  int | [**dp\_msg\_ack**](#function-dp_msg_ack) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Acknowledge a message on a durable (JetStream) consumer._  |
|  void \* | [**dp\_msg\_data**](#function-dp_msg_data) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return a pointer to the raw sample data inside the message._  |
|  void | [**dp\_msg\_free**](#function-dp_msg_free) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Free a message handle and release the underlying buffer._  |
|  [**dp\_frame\_kind\_t**](group__types.md#enum-dp_frame_kind_t) | [**dp\_msg\_kind**](#function-dp_msg_kind) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_What the message's payload IS (dp\_frame\_kind\_t)._  |
|  double | [**dp\_msg\_mean\_power**](#function-dp_msg_mean_power) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Mean power of a received I/Q frame, normalised to full scale._  |
|  size\_t | [**dp\_msg\_num\_samples**](#function-dp_msg_num_samples) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the number of complex samples in the message._  |
|  [**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) | [**dp\_msg\_sample\_type**](#function-dp_msg_sample_type) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the sample type of the message._  |
|  size\_t | [**dp\_msg\_size**](#function-dp_msg_size) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the byte size of the sample data._  |




























## Public Functions Documentation




### function dp\_mean\_power 

_Mean power of a complex sample block, normalised to full scale._ 
```
double dp_mean_power (
    dp_sample_type_t format,
    const void * data,
    size_t n
) 
```



`mean(|x|^2)`, with the integer formats divided by dp\_format\_full\_scale() first so the answer means the same thing whatever the format is — `10*log10()` of it is dBFS in every case. The frame-level [**dp\_msg\_mean\_power()**](group__msg.md#function-dp_msg_mean_power) is this over a received message.




**Parameters:**


* `format` Sample format of `data`. 
* `data` `n` complex samples, interleaved for an integer format. 
* `n` Sample count. 



**Returns:**

Mean power, or 0 for a NULL pointer, an empty block, or a format this build does not know. 





        

<hr>



### function dp\_msg\_ack 

_Acknowledge a message on a durable (JetStream) consumer._ 
```
int dp_msg_ack (
    dp_msg_t * msg
) 
```



For the resilient NATS work-queue tier (a `nats://` Pull consumer), delivery is at-least-once: a message stays pending until acked, and is redelivered if the consumer dies before acking. Call this once the message has been fully processed, then [**dp\_msg\_free()**](group__msg.md#function-dp_msg_free).


A no-op (returns DP\_OK) for transports without acks — NATS core PUB/SUB and reassembled chunked frames — so callers can ack unconditionally.




**Parameters:**


* `msg` Message handle returned by a recv function. 



**Returns:**

DP\_OK on success, negative error code on failure. 





        

<hr>



### function dp\_msg\_data 

_Return a pointer to the raw sample data inside the message._ 
```
void * dp_msg_data (
    dp_msg_t * msg
) 
```





**Parameters:**


* `msg` Message handle returned by a recv function. 



**Returns:**

Pointer to contiguous sample data (valid until dp\_msg\_free). 





        

<hr>



### function dp\_msg\_free 

_Free a message handle and release the underlying buffer._ 
```
void dp_msg_free (
    dp_msg_t * msg
) 
```





**Parameters:**


* `msg` Message handle (may be NULL). 




        

<hr>



### function dp\_msg\_kind 

_What the message's payload IS (dp\_frame\_kind\_t)._ 
```
dp_frame_kind_t dp_msg_kind (
    dp_msg_t * msg
) 
```



Ask this before [**dp\_msg\_sample\_type()**](group__msg.md#function-dp_msg_sample_type): a telemetry frame's format field is 0, because BLUE has no code for a record stream.




**Parameters:**


* `msg` Message handle. 



**Returns:**

The frame's kind, or DP\_KIND\_IQ for a NULL handle. 





        

<hr>



### function dp\_msg\_mean\_power 

_Mean power of a received I/Q frame, normalised to full scale._ 
```
double dp_msg_mean_power (
    dp_msg_t * msg
) 
```



[**dp\_mean\_power()**](group__msg.md#function-dp_mean_power) over the frame's own samples: the format and the count come from its header, so a subscriber that takes whatever arrives can compare frames without branching on the type. `10*log10()` of it is dBFS.


Exists because every consumer was writing this loop: the C receiver example carried one copy per wire type and a switch to pick between them, which is the same duplication the library forbids internally.




**Parameters:**


* `msg` Message handle from a recv. 



**Returns:**

Mean power, or 0 for a NULL handle, an empty frame, or a non-I/Q kind (telemetry records are not samples). 





        

<hr>



### function dp\_msg\_num\_samples 

_Return the number of complex samples in the message._ 
```
size_t dp_msg_num_samples (
    dp_msg_t * msg
) 
```





**Parameters:**


* `msg` Message handle. 



**Returns:**

Number of samples (header num\_samples). 





        

<hr>



### function dp\_msg\_sample\_type 

_Return the sample type of the message._ 
```
dp_sample_type_t dp_msg_sample_type (
    dp_msg_t * msg
) 
```





**Parameters:**


* `msg` Message handle. 



**Returns:**

Sample type enum value. 





        

<hr>



### function dp\_msg\_size 

_Return the byte size of the sample data._ 
```
size_t dp_msg_size (
    dp_msg_t * msg
) 
```





**Parameters:**


* `msg` Message handle. 



**Returns:**

Total data bytes. 





        

<hr>

------------------------------


