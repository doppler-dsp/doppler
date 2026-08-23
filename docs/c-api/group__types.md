

# Group types



[**Modules**](modules.md) **>** [**types**](group__types.md)
















## Modules

| Type | Name |
| ---: | :--- |
| module | [**Sample C types**](group__sampletypes.md) <br> |
| module | [**Wire constants**](group__wire.md) <br> |




## Classes

| Type | Name |
| ---: | :--- |
| struct | [**dp\_chunk\_t**](structdp__chunk__t.md) <br>_Reassembly geometry, present only when_ [_**DP\_FLAG\_CHUNKED**_](group__wire.md#define-dp_flag_chunked) _._ |
| struct | [**dp\_header\_t**](structdp__header__t.md) <br>_Frame metadata carried in every stream message._  |


## Public Types

| Type | Name |
| ---: | :--- |
| enum  | [**dp\_frame\_kind\_t**](#enum-dp_frame_kind_t)  <br>_What a frame's payload IS, independent of how its elements are encoded._  |
| typedef struct dp\_msg | [**dp\_msg\_t**](#typedef-dp_msg_t)  <br>_Opaque zero-copy message handle returned by recv functions._  |
| typedef struct dp\_ctx | [**dp\_pub\_t**](#typedef-dp_pub_t)  <br>_Opaque streaming socket handle returned by all create functions._  |
| typedef struct dp\_ctx | [**dp\_pull\_t**](#typedef-dp_pull_t)  <br> |
| typedef struct dp\_ctx | [**dp\_push\_t**](#typedef-dp_push_t)  <br> |
| typedef struct dp\_ctx | [**dp\_rep\_t**](#typedef-dp_rep_t)  <br> |
| typedef struct dp\_ctx | [**dp\_req\_t**](#typedef-dp_req_t)  <br> |
| typedef struct dp\_ctx | [**dp\_sub\_t**](#typedef-dp_sub_t)  <br> |
















































## Public Types Documentation




### enum dp\_frame\_kind\_t 

_What a frame's payload IS, independent of how its elements are encoded._ 
```
enum dp_frame_kind_t {
    DP_KIND_IQ = 0,
    DP_KIND_TLM = 1,
    DP_KIND_EOS = 2
};
```



`TLM16` used to be a sixth `dp_sample_type_t`, which made every I/Q-only sender carry an exception for it and left the format field holding a value BLUE does not define. Telemetry is not a sample encoding, it is a different kind of frame, so it says so here and the format field stays purely a BLUE sample code. 


        

<hr>



### typedef dp\_msg\_t 

_Opaque zero-copy message handle returned by recv functions._ 
```
typedef struct dp_msg dp_msg_t;
```



The data buffer is valid until [**dp\_msg\_free()**](group__msg.md#function-dp_msg_free) is called. Use the accessor functions to retrieve a pointer to the sample data, size, etc. 


        

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



### typedef dp\_sub\_t 

```
typedef struct dp_ctx dp_sub_t;
```




<hr>

------------------------------


