

# Group utils



[**Modules**](modules.md) **>** [**utils**](group__utils.md)
















## Modules

| Type | Name |
| ---: | :--- |
| module | [**Interrupting a blocking receive (DEPRECATED)**](group__interrupt.md) <br> |


























## Public Functions

| Type | Name |
| ---: | :--- |
|  int | [**dp\_ctx\_delete\_stream**](#function-dp_ctx_delete_stream) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx) <br>_Delete the work-queue stream backing_ `ctx's` _subject._ |
|  const char \* | [**dp\_ctx\_last\_error**](#function-dp_ctx_last_error) (const [**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx) <br>_The backend's own account of the last failure on_ `ctx` _._ |
|  void | [**dp\_ctx\_set\_timestamp\_ns**](#function-dp_ctx_set_timestamp_ns) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, uint64\_t timestamp\_ns) <br>_Override the_ `timestamp_ns` _the NEXT send on_`ctx` _will stamp, instead of a fresh_[_**dp\_get\_timestamp\_ns()**_](group__utils.md#function-dp_get_timestamp_ns) _read._ |
|  size\_t | [**dp\_element\_size**](#function-dp_element_size) ([**dp\_frame\_kind\_t**](group__types.md#enum-dp_frame_kind_t) kind, [**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) format) <br>_Bytes per payload element for a frame of this kind._  |
|  uint64\_t | [**dp\_get\_timestamp\_ns**](#function-dp_get_timestamp_ns) (void) <br>_Return the current wall-clock time as nanoseconds since the UNIX epoch._  |
|  const char \* | [**dp\_host\_rep**](#function-dp_host_rep) (void) <br>_This machine's byte-order tag:_ [_**DP\_REP\_LE**_](group__wire.md#define-dp_rep_le) _or_[_**DP\_REP\_BE**_](group__wire.md#define-dp_rep_be) _._ |
|  size\_t | [**dp\_sample\_size**](#function-dp_sample_size) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Return the byte size of one complex sample for_ `type` _._ |
|  int | [**dp\_sample\_type\_is\_valid**](#function-dp_sample_type_is_valid) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_True when_ `type` _is a sample type this build knows._ |
|  const char \* | [**dp\_sample\_type\_str**](#function-dp_sample_type_str) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64")._ |
|  const char \* | [**dp\_strerror**](#function-dp_strerror) (int err) <br>_Return a human-readable description of an error code._  |



























## Macros

| Type | Name |
| ---: | :--- |
| define  | [**DP\_WORK\_QUEUE\_MAX\_AGE\_NS**](group__utils.md#define-dp_work_queue_max_age_ns)  `(3600LL \* 1000000000LL)`<br>_Retention bound doppler gives a work queue it creates itself._  |

## Public Functions Documentation




### function dp\_ctx\_delete\_stream 

_Delete the work-queue stream backing_ `ctx's` _subject._
```
int dp_ctx_delete_stream (
    dp_pub_t * ctx
) 
```



Administrative, and deliberately never automatic: a work queue is shared infrastructure, and outliving any one producer is the feature, so closing a context must not end it. Call this only when the caller owns the queue's lifetime  a test that made the subject up, or a tool tearing down what it provisioned.


Every frame still in the queue is destroyed with it, acked or not.




**Parameters:**


* `ctx` A context whose endpoint names the queue. 



**Returns:**

DP\_OK, or DP\_ERR\_INVALID (see [**dp\_ctx\_last\_error()**](group__utils.md#function-dp_ctx_last_error)). 





        

<hr>



### function dp\_ctx\_last\_error 

_The backend's own account of the last failure on_ `ctx` _._
```
const char * dp_ctx_last_error (
    const dp_pub_t * ctx
) 
```



`dp_strerror()` names the CLASS of error doppler returned; this names what the transport said, which is the part that distinguishes a slow broker from an absent one. Every non-OK publish status collapses into DP\_ERR\_SEND, so without this a caller sees "Send error" and has nothing to act on.




**Parameters:**


* `ctx` Any send-capable context. 



**Returns:**

A NUL-terminated detail string, or "" when the last call succeeded or the backend offered nothing. Owned by `ctx` and valid until the next send on it; copy to keep. 





        

<hr>



### function dp\_ctx\_set\_timestamp\_ns 

_Override the_ `timestamp_ns` _the NEXT send on_`ctx` _will stamp, instead of a fresh_[_**dp\_get\_timestamp\_ns()**_](group__utils.md#function-dp_get_timestamp_ns) _read._
```
void dp_ctx_set_timestamp_ns (
    dp_pub_t * ctx,
    uint64_t timestamp_ns
) 
```



One-shot: consumed (and cleared) by the very next send call on this context, whether or not it was actually used. Lets a hop that already knows a more precise or truer origin time (e.g. a value derived from [**dp\_sample\_clock\_stamp\_at()**](timing__core_8h.md#function-dp_sample_clock_stamp_at) over an upstream message's own header, or a passthrough of that upstream header's own `timestamp_ns`) propagate it downstream instead of every hop silently re-stamping "now" and losing the connection to when the samples actually occurred. `ctx` accepts any socket role (dp\_pub\_t / dp\_push\_t / dp\_req\_t / dp\_rep\_t are the same underlying context type).




**Parameters:**


* `ctx` Allocated send-capable context (any role). 
* `timestamp_ns` Nanoseconds since the UNIX epoch to stamp on the next send. 




        

<hr>



### function dp\_element\_size 

_Bytes per payload element for a frame of this kind._ 
```
size_t dp_element_size (
    dp_frame_kind_t kind,
    dp_sample_type_t format
) 
```



For DP\_KIND\_IQ that is [**dp\_sample\_size()**](group__utils.md#function-dp_sample_size) of `format`; for DP\_KIND\_TLM it is 16, one packed record, and `format` is not consulted because a record stream has no BLUE code.




**Parameters:**


* `kind` What the payload is (dp\_frame\_kind\_t). 
* `format` Sample format, for an I/Q frame. 



**Returns:**

Bytes per element, or 0 when the pair is not something this build can send or decode. 





        

<hr>



### function dp\_get\_timestamp\_ns 

_Return the current wall-clock time as nanoseconds since the UNIX epoch._ 
```
uint64_t dp_get_timestamp_ns (
    void
) 
```



Uses CLOCK\_REALTIME. Useful for timestamping samples before calling a send function, or for measuring round-trip latency.




**Returns:**

Nanoseconds since epoch. 





        

<hr>



### function dp\_host\_rep 

_This machine's byte-order tag:_ [_**DP\_REP\_LE**_](group__wire.md#define-dp_rep_le) _or_[_**DP\_REP\_BE**_](group__wire.md#define-dp_rep_be) _._
```
const char * dp_host_rep (
    void
) 
```



Four characters, not NUL-terminated. Derived at run time rather than compiled in, so a big-endian build tags its frames honestly instead of inheriting a constant nobody revisited. 


        

<hr>



### function dp\_sample\_size 

_Return the byte size of one complex sample for_ `type` _._
```
size_t dp_sample_size (
    dp_sample_type_t type
) 
```





**Parameters:**


* `type` Sample type enum value. 



**Returns:**

Byte count (e.g. 2 for CI8, 4 for CI16, 8 for CI32/CF32, 16 for CF64). 





        

<hr>



### function dp\_sample\_type\_is\_valid 

_True when_ `type` _is a sample type this build knows._
```
int dp_sample_type_is_valid (
    dp_sample_type_t type
) 
```



Derived from [**dp\_sample\_size()**](group__utils.md#function-dp_sample_size), so there is one table: a type with no size is not a type. Ask this rather than range-testing the enum  the values are append-only and a RETIRED one (2, the former CF128) sits inside the range while being invalid, so `type <= CF32` accepts a value nothing can send or decode.




**Parameters:**


* `type` Sample type enum value. 



**Returns:**

Non-zero when the type is known, 0 otherwise. 





        

<hr>



### function dp\_sample\_type\_str 

_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64")._
```
const char * dp_sample_type_str (
    dp_sample_type_t type
) 
```





**Parameters:**


* `type` Sample type enum value. 



**Returns:**

Statically allocated, null-terminated string. 





        

<hr>



### function dp\_strerror 

_Return a human-readable description of an error code._ 
```
const char * dp_strerror (
    int err
) 
```





**Parameters:**


* `err` Negative error code returned by any dp\_\* function. 



**Returns:**

Statically allocated, null-terminated string. 





        

<hr>
## Macro Definition Documentation





### define DP\_WORK\_QUEUE\_MAX\_AGE\_NS 

_Retention bound doppler gives a work queue it creates itself._ 
```
#define DP_WORK_QUEUE_MAX_AGE_NS `(3600LL * 1000000000LL)`
```



A work queue drops a frame when a consumer ACKS it, so a frame nobody consumes is kept forever  and the stream is file-backed. Created with no limits, a producer with no consumer is an unbounded disk sink (doppler#1136: 40 GB of residue from repeated test runs). An AGE bound is the one limit that cannot silently drop a frame a live consumer was about to take, unlike MaxBytes or MaxMsgs.


One hour, in nanoseconds. To choose differently, PRE-PROVISION the stream: doppler adopts an existing one as-is rather than reconfiguring it, which is how a Helm-created R=3 stream already works. 


        

<hr>

------------------------------


