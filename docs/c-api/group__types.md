

# Group types



[**Modules**](modules.md) **>** [**types**](group__types.md)
















## Modules

| Type | Name |
| ---: | :--- |
| module | [**Sample C types**](group__sampletypes.md) <br> |




## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_header\_t**](structdp__header__t.md) <br>_Frame metadata header carried in every stream message._  |


## Public Types

| Type | Name |
| ---: | :--- |
| typedef struct dp\_msg | [**dp\_msg\_t**](#typedef-dp_msg_t)  <br>_Opaque zero-copy message handle returned by recv functions._  |
| enum  | [**dp\_protocol\_t**](#enum-dp_protocol_t)  <br>_Protocol identifier for the wire header._  |
| typedef struct dp\_ctx | [**dp\_pub\_t**](#typedef-dp_pub_t)  <br>_Opaque streaming socket handle returned by all create functions._  |
| typedef struct dp\_ctx | [**dp\_pull\_t**](#typedef-dp_pull_t)  <br> |
| typedef struct dp\_ctx | [**dp\_push\_t**](#typedef-dp_push_t)  <br> |
| typedef struct dp\_ctx | [**dp\_rep\_t**](#typedef-dp_rep_t)  <br> |
| typedef struct dp\_ctx | [**dp\_req\_t**](#typedef-dp_req_t)  <br> |
| enum  | [**dp\_sample\_type\_t**](#enum-dp_sample_type_t)  <br>_Selects the wire format of complex samples._  |
| typedef struct dp\_ctx | [**dp\_sub\_t**](#typedef-dp_sub_t)  <br> |
















































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



### typedef dp\_pub\_t 

_Opaque streaming socket handle returned by all create functions._ 
```
typedef struct dp_ctx dp_pub_t;
```




<hr>



### typedef dp\_pull\_t 

```
typedef struct dp_ctx dp_pull_t;
```




<hr>



### typedef dp\_push\_t 

```
typedef struct dp_ctx dp_push_t;
```




<hr>



### typedef dp\_rep\_t 

```
typedef struct dp_ctx dp_rep_t;
```




<hr>



### typedef dp\_req\_t 

```
typedef struct dp_ctx dp_req_t;
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



### typedef dp\_sub\_t 

```
typedef struct dp_ctx dp_sub_t;
```




<hr>

------------------------------


