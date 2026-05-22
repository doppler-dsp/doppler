

# Group types



[**Modules**](modules.md) **>** [**types**](group__types.md)
















## Modules

| Type | Name |
| ---: | :--- |
| module | [**Sample C types**](group__sampletypes.md) <br> |




## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_header\_t**](structdp__header__t.md) <br>_Frame metadata header carried in every ZMQ message._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_msg | [**dp\_msg\_t**](#typedef-dp_msg_t)  <br>_Opaque zero-copy message handle returned by recv functions._  |
| enum  | [**dp\_protocol\_t**](#enum-dp_protocol_t)  <br>_Protocol identifier for the wire header._  |
| typedef struct dp\_ctx | [**dp\_pub**](#typedef-dp_pub)  <br>_Opaque streaming socket handle returned by all create functions._  |
| typedef struct dp\_ctx | [**dp\_pull**](#typedef-dp_pull)  <br> |
| typedef struct dp\_ctx | [**dp\_push**](#typedef-dp_push)  <br> |
| typedef struct dp\_ctx | [**dp\_rep**](#typedef-dp_rep)  <br> |
| typedef struct dp\_ctx | [**dp\_req**](#typedef-dp_req)  <br> |
| enum  | [**dp\_sample\_type\_t**](#enum-dp_sample_type_t)  <br>_Selects the wire format of complex samples._  |
| typedef struct dp\_ctx | [**dp\_sub**](#typedef-dp_sub)  <br> |
















































## Public Types Documentation




### typedef dp\_msg\_t 

_Opaque zero-copy message handle returned by recv functions._ 
```
typedef struct dp_msg dp_msg_t;
```



The data buffer is valid until [**dp\_msg\_free()**](group__msg.md#function-dp_msg_free) is called. Use the accessor functions to retrieve a pointer to the sample data, size, etc. 


        

<hr>



### enum dp\_protocol\_t 

_Protocol identifier for the wire header._ 
```
enum dp_protocol_t {
    DP_PROTO_SIGS = 0,
    DP_PROTO_DIFI = 1
};
```




<hr>



### typedef dp\_pub 

_Opaque streaming socket handle returned by all create functions._ 
```
typedef struct dp_ctx dp_pub;
```




<hr>



### typedef dp\_pull 

```
typedef struct dp_ctx dp_pull;
```




<hr>



### typedef dp\_push 

```
typedef struct dp_ctx dp_push;
```




<hr>



### typedef dp\_rep 

```
typedef struct dp_ctx dp_rep;
```




<hr>



### typedef dp\_req 

```
typedef struct dp_ctx dp_req;
```




<hr>



### enum dp\_sample\_type\_t 

_Selects the wire format of complex samples._ 
```
enum dp_sample_type_t {
    CI32 = 0,
    CF64 = 1,
    CF128 = 2,
    CI8 = 3,
    CI16 = 4,
    CF32 = 5
};
```



All sample arrays are packed as interleaved I/Q pairs.


Values are fixed — new types are appended to preserve wire compatibility with older receivers. 


        

<hr>



### typedef dp\_sub 

```
typedef struct dp_ctx dp_sub;
```




<hr>

------------------------------


