

# Group utils



[**Modules**](modules.md) **>** [**utils**](group__utils.md)










































## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_ctx\_set\_timestamp\_ns**](#function-dp_ctx_set_timestamp_ns) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, uint64\_t timestamp\_ns) <br>_Override the_ `timestamp_ns` _the NEXT send on_`ctx` _will stamp, instead of a fresh_[_**dp\_get\_timestamp\_ns()**_](group__utils.md#function-dp_get_timestamp_ns) _read._ |
|  uint64\_t | [**dp\_get\_timestamp\_ns**](#function-dp_get_timestamp_ns) (void) <br>_Return the current wall-clock time as nanoseconds since the UNIX epoch._  |
|  size\_t | [**dp\_sample\_size**](#function-dp_sample_size) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_Return the byte size of one complex sample for_ `type` _._ |
|  int | [**dp\_sample\_type\_is\_iq**](#function-dp_sample_type_is_iq) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_True when_ `type` _is a known I/Q sample type._ |
|  int | [**dp\_sample\_type\_is\_valid**](#function-dp_sample_type_is_valid) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_True when_ `type` _is a sample type this build knows._ |
|  const char \* | [**dp\_sample\_type\_str**](#function-dp_sample_type_str) ([**dp\_sample\_type\_t**](group__types.md#enum-dp_sample_type_t) type) <br>_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64")._ |
|  const char \* | [**dp\_strerror**](#function-dp_strerror) (int err) <br>_Return a human-readable description of an error code._  |




























## Public Functions Documentation




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



### function dp\_sample\_type\_is\_iq 

_True when_ `type` _is a known I/Q sample type._
```
int dp_sample_type_is_iq (
    dp_sample_type_t type
) 
```



Every valid type except TLM16, whose payload is telemetry records rather than samples. This is the check a sender that carries only I/Q (PUSH, REQ, REP) wants; PUB additionally accepts TLM16.




**Parameters:**


* `type` Sample type enum value. 



**Returns:**

Non-zero when the type is a known I/Q type, 0 otherwise. 





        

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

------------------------------


