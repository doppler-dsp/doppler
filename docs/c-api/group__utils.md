

# Group utils



[**Modules**](modules.md) **>** [**utils**](group__utils.md)
















## Modules

| Type | Name |
| ---: | :--- |
| module | [**Interrupting a blocking receive (DEPRECATED)**](group__interrupt.md) <br> |


























## Public Functions

| Type | Name |
| ---: | :--- |
|  void | [**dp\_ctx\_set\_timestamp\_ns**](#function-dp_ctx_set_timestamp_ns) ([**dp\_pub\_t**](group__types.md#typedef-dp_pub_t) \* ctx, uint64\_t timestamp\_ns) <br>_Override the_ `timestamp_ns` _the NEXT send on_`ctx` _will stamp, instead of a fresh_[_**dp\_get\_timestamp\_ns()**_](group__utils.md#function-dp_get_timestamp_ns) _read._ |
|  size\_t | [**dp\_element\_size**](#function-dp_element_size) ([**dp\_frame\_kind\_t**](group__types.md#enum-dp_frame_kind_t) kind, [**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) format) <br>_Bytes per payload element for a frame of this kind._  |
|  uint64\_t | [**dp\_get\_timestamp\_ns**](#function-dp_get_timestamp_ns) (void) <br>_Return the current wall-clock time as nanoseconds since the UNIX epoch._  |
|  const char \* | [**dp\_host\_rep**](#function-dp_host_rep) (void) <br>_This machine's byte-order tag:_ [_**DP\_REP\_LE**_](group__wire.md#define-dp_rep_le) _or_[_**DP\_REP\_BE**_](group__wire.md#define-dp_rep_be) _._ |
|  size\_t | [**dp\_sample\_size**](#function-dp_sample_size) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Return the byte size of one complex sample for_ `type` _._ |
|  int | [**dp\_sample\_type\_is\_valid**](#function-dp_sample_type_is_valid) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_True when_ `type` _is a sample type this build knows._ |
|  const char \* | [**dp\_sample\_type\_str**](#function-dp_sample_type_str) ([**dp\_sample\_type\_t**](dp__format_8h.md#enum-dp_sample_type_t) type) <br>_Return a short string name for_ `type` _("CI8", "CI16", "CI32", "CF32", "CF64")._ |
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

------------------------------


