

# Group msg



[**Modules**](modules.md) **>** [**msg**](group__msg.md)










































## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_msg\_ack**](#function-dp_msg_ack) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Acknowledge a message on a durable (JetStream) consumer._  |
|  void \* | [**dp\_msg\_data**](#function-dp_msg_data) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return a pointer to the raw sample data inside the message._  |
|  void | [**dp\_msg\_free**](#function-dp_msg_free) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Free a message handle and release the underlying buffer._  |
|  size\_t | [**dp\_msg\_num\_samples**](#function-dp_msg_num_samples) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the number of complex samples in the message._  |
|  [**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) | [**dp\_msg\_sample\_type**](#function-dp_msg_sample_type) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the sample type of the message._  |
|  size\_t | [**dp\_msg\_size**](#function-dp_msg_size) ([**dp\_msg\_t**](group__types.md#typedef-dp_msg_t) \* msg) <br>_Return the byte size of the sample data._  |




























## Public Functions Documentation




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


